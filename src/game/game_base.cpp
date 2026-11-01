#include "game_base.h"
#include "../domain/officer_roster_publication.h"
#include "../platform/windows/process_memory.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <Psapi.h>

#include <algorithm>
#include <cstring>
#include <limits>

#pragma comment(lib, "Psapi.lib")

namespace s8::game {
namespace {

constexpr std::size_t PATTERN_SIZE = 10;
constexpr std::size_t BLOCKS_PER_PROBE = 16;
constexpr std::uint64_t PROBE_RETRY_MS = 1000;

bool matches_game_base_pattern(std::span<const std::byte> image, std::size_t offset) {
    return image[offset] == std::byte{0x4c} && image[offset + 1] == std::byte{0x8b} &&
        image[offset + 2] == std::byte{0x05} && image[offset + 7] == std::byte{0x41} &&
        image[offset + 8] == std::byte{0x0f} && image[offset + 9] == std::byte{0xb7};
}

bool module_image_size(std::uintptr_t module, std::size_t& size) {
    MODULEINFO info{};
    if (!module || !GetModuleInformation(GetCurrentProcess(), reinterpret_cast<HMODULE>(module),
            &info, sizeof(info)) || reinterpret_cast<std::uintptr_t>(info.lpBaseOfDll) != module ||
        info.SizeOfImage < PATTERN_SIZE ||
        info.SizeOfImage > std::numeric_limits<std::uintptr_t>::max() - module) {
        return false;
    }
    size = info.SizeOfImage;
    return true;
}

bool readable_image_region(const MEMORY_BASIC_INFORMATION& region, std::uintptr_t module) {
    if (region.State != MEM_COMMIT || region.Type != MEM_IMAGE ||
        reinterpret_cast<std::uintptr_t>(region.AllocationBase) != module ||
        (region.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0) {
        return false;
    }
    constexpr DWORD READABLE = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY |
        PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
    return (region.Protect & READABLE) != 0;
}

bool executable_region(const MEMORY_BASIC_INFORMATION& region) {
    constexpr DWORD EXECUTABLE = PAGE_EXECUTE | PAGE_EXECUTE_READ |
        PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
    return (region.Protect & EXECUTABLE) != 0;
}

std::uint64_t retry_time(std::uint64_t now_ms) {
    return now_ms > std::numeric_limits<std::uint64_t>::max() - PROBE_RETRY_MS
        ? std::numeric_limits<std::uint64_t>::max() : now_ms + PROBE_RETRY_MS;
}

} // namespace

std::optional<std::uintptr_t> find_game_base_pointer(
    std::span<const std::byte> image, std::uintptr_t module_base) {
    if (image.size() < PATTERN_SIZE)
        return std::nullopt;
    for (std::size_t i = 0; i + PATTERN_SIZE <= image.size(); ++i) {
        if (!matches_game_base_pattern(image, i))
            continue;
        std::int32_t displacement = 0;
        std::memcpy(&displacement, image.data() + i + 3, sizeof(displacement));
        return module_base + i + 7 + static_cast<std::intptr_t>(displacement);
    }
    return std::nullopt;
}

bool validate_game_base_pointer(std::uintptr_t module_base, std::uintptr_t slot) {
    std::size_t image_size = 0;
    if (!module_image_size(module_base, image_size) || slot < module_base ||
        slot - module_base > image_size - sizeof(std::uintptr_t) ||
        slot % alignof(std::uintptr_t) != 0) {
        return false;
    }
    MEMORY_BASIC_INFORMATION region{};
    if (VirtualQuery(reinterpret_cast<const void*>(slot), &region, sizeof(region)) != sizeof(region) ||
        !readable_image_region(region, module_base) || executable_region(region)) {
        return false;
    }
    const std::uintptr_t begin = reinterpret_cast<std::uintptr_t>(region.BaseAddress);
    if (begin > slot || region.RegionSize < sizeof(std::uintptr_t) ||
        slot - begin > region.RegionSize - sizeof(std::uintptr_t)) {
        return false;
    }

    const std::uintptr_t root = read_game_base(slot);
    if (root <= 0x10000 || !platform::windows::is_committed_memory(root, 0x6000)) {
        return false;
    }
    const std::uintptr_t player = read_player(root);
    std::uint16_t id = 0;
    if (player <= 0x10000 || !platform::windows::is_committed_memory(player, 0x200) ||
        !platform::windows::read_memory(player + 8, std::as_writable_bytes(std::span(&id, 1))) ||
        id == 0 || id > domain::OFFICER_ROSTER_LIMIT) {
        return false;
    }
    // A loading transition between reads must not certify a mixed identity.
    std::uint16_t confirmed_id = 0;
    return read_game_base(slot) == root && read_player(root) == player &&
        platform::windows::read_memory(player + 8, std::as_writable_bytes(std::span(&confirmed_id, 1))) &&
        confirmed_id == id;
}

std::optional<std::uintptr_t> probe_game_base_pointer(GameBaseProbe& probe,
    std::uintptr_t module_base, std::uint64_t now_ms, const domain::WorkBudget& budget) {
    if (budget.exhausted()) {
        return std::nullopt;
    }
    if (probe.module != module_base) {
        probe = {};
        probe.module = module_base;
    }
    if (probe.status == GameBaseProbeStatus::Found) {
        if (validate_game_base_pointer(module_base, probe.result)) {
            return probe.result;
        }
        probe.status = GameBaseProbeStatus::Idle;
        probe.result = 0;
    }
    if (probe.status != GameBaseProbeStatus::Scanning) {
        if (now_ms < probe.next_retry_ms) {
            return std::nullopt;
        }
        probe.cursor = 0;
        probe.overlap_size = 0;
        probe.read_failed = false;
        if (!module_image_size(module_base, probe.image_size)) {
            probe.image_size = 0;
            probe.status = GameBaseProbeStatus::ReadFailed;
            probe.next_retry_ms = retry_time(now_ms);
            return std::nullopt;
        }
        probe.status = GameBaseProbeStatus::Scanning;
    }

    for (std::size_t block = 0; block < BLOCKS_PER_PROBE && probe.cursor < probe.image_size &&
            !budget.exhausted(); ++block) {
        const std::uintptr_t address = module_base + probe.cursor;
        MEMORY_BASIC_INFORMATION region{};
        if (VirtualQuery(reinterpret_cast<const void*>(address), &region, sizeof(region)) != sizeof(region)) {
            probe.read_failed = true;
            probe.overlap_size = 0;
            probe.cursor += std::min(GameBaseProbe::BLOCK_SIZE, probe.image_size - probe.cursor);
            continue;
        }
        const std::uintptr_t begin = reinterpret_cast<std::uintptr_t>(region.BaseAddress);
        if (begin > address || region.RegionSize == 0 ||
            region.RegionSize > std::numeric_limits<std::uintptr_t>::max() - begin ||
            begin + region.RegionSize <= address) {
            probe.read_failed = true;
            probe.overlap_size = 0;
            probe.cursor += std::min(GameBaseProbe::BLOCK_SIZE, probe.image_size - probe.cursor);
            continue;
        }
        const std::uintptr_t end = std::min(module_base + probe.image_size, begin + region.RegionSize);
        if (!readable_image_region(region, module_base) || !executable_region(region)) {
            probe.overlap_size = 0;
            probe.cursor = end - module_base;
            continue;
        }
        const std::size_t amount = std::min(GameBaseProbe::BLOCK_SIZE,
            static_cast<std::size_t>(end - address));
        const std::size_t overlap = probe.overlap_size;
        if (!platform::windows::read_memory(address,
                std::span(probe.bytes).subspan(overlap, amount))) {
            probe.read_failed = true;
            probe.overlap_size = 0;
            probe.cursor += amount;
            continue;
        }
        const std::size_t length = overlap + amount;
        const std::size_t first_rva = probe.cursor - overlap;
        // Once admitted, finish this 4 KiB block and retire its bytes even if
        // the native read overran its deadline. A slow page cannot starve us.
        for (std::size_t offset = 0; offset + PATTERN_SIZE <= length; ++offset) {
            if (!matches_game_base_pattern(std::span(probe.bytes).first(length), offset)) {
                continue;
            }
            std::int32_t displacement = 0;
            std::memcpy(&displacement, probe.bytes.data() + offset + 3, sizeof(displacement));
            const std::int64_t target_rva = static_cast<std::int64_t>(first_rva + offset) + 7 + displacement;
            if (target_rva < 0 || static_cast<std::uint64_t>(target_rva) >
                    probe.image_size - sizeof(std::uintptr_t)) {
                continue;
            }
            // The overlap may have been read on an earlier dispatch while the
            // executable was still unpacking. Confirm one live instruction,
            // rather than combining bytes that never existed together.
            std::array<std::byte, PATTERN_SIZE> instruction{};
            if (!platform::windows::read_memory(module_base + first_rva + offset, instruction)) {
                probe.read_failed = true;
                continue;
            }
            if (std::memcmp(instruction.data(), probe.bytes.data() + offset, PATTERN_SIZE) != 0) {
                continue;
            }
            const std::uintptr_t slot = module_base + static_cast<std::uintptr_t>(target_rva);
            if (validate_game_base_pointer(module_base, slot)) {
                probe.cursor += amount;
                probe.overlap_size = 0;
                probe.result = slot;
                probe.status = GameBaseProbeStatus::Found;
                return slot;
            }
        }
        probe.overlap_size = std::min(GameBaseProbe::OVERLAP_SIZE, length);
        std::memmove(probe.bytes.data(), probe.bytes.data() + length - probe.overlap_size, probe.overlap_size);
        probe.cursor += amount;
    }
    if (probe.cursor == probe.image_size) {
        if (probe.passes != std::numeric_limits<std::uint64_t>::max()) {
            ++probe.passes;
        }
        probe.overlap_size = 0;
        probe.status = probe.read_failed ? GameBaseProbeStatus::ReadFailed : GameBaseProbeStatus::Waiting;
        probe.next_retry_ms = retry_time(now_ms);
    }
    return std::nullopt;
}

std::uintptr_t read_game_base(std::uintptr_t pointer_address) {
    std::uintptr_t game_base = 0;
    if (!pointer_address || !platform::windows::read_memory(
            pointer_address, std::as_writable_bytes(std::span(&game_base, 1))))
        return 0;
    return game_base;
}

std::uintptr_t read_player(std::uintptr_t game_base) {
    std::uintptr_t player = 0;
    if (game_base <= 0x10000 || !platform::windows::read_memory(
            game_base + 0xe0, std::as_writable_bytes(std::span(&player, 1))))
        return 0;
    return player;
}

} // namespace s8::game
