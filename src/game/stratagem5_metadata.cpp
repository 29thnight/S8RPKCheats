#include "stratagem5_metadata.h"
#include "stratagem5_model.h"
#include "../platform/windows/process_memory.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>

namespace s8::game {
namespace {

constexpr std::array PATTERN{
    0x48, 0x8b, -1, -1, -1, -1, -1, -1, 0xe8, -1, -1, -1, -1,
    0x48, 0x8b, -1, 0x48, 0x83, -1, -1, 0x5b, 0xc3, 0x33, -1,
    0x48, 0x8b, -1, 0xe8, -1, -1, -1, -1, 0x48, 0x8b, -1,
    0x48, 0x83, -1, -1, 0x5b, 0xc3, 0xe8, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, 0x40, 0x53,
};

std::optional<std::size_t> find_pattern(std::span<const std::byte> image) {
    if (image.size() < PATTERN.size()) {
        return std::nullopt;
    }

    for (std::size_t i = 0; i + PATTERN.size() <= image.size(); ++i) {
        bool matches = true;

        for (std::size_t j = 0; j < PATTERN.size(); ++j) {
            if (PATTERN[j] >= 0 && std::to_integer<int>(image[i + j]) != PATTERN[j]) {
                matches = false;
                break;
            }
        }
        if (matches) {
            return i;
        }
    }
    return std::nullopt;
}

bool valid_table(std::uintptr_t table) {
    std::array<std::byte, 5 * 0x20> rows{};

    if (!platform::windows::read_memory(table, rows)) {
        return false;
    }
    for (std::size_t i = 0; i < 5; ++i) {
        const std::byte expected = static_cast<std::byte>(i + 1);

        if (rows[i * 0x20 + 0x08] != expected || rows[i * 0x20 + 0x0a] != expected ||
            rows[i * 0x20 + 0x0c] != expected) {
            return false;
        }
    }
    return true;
}

} // namespace

std::optional<std::uint32_t> find_stratagem5_metadata_offset(std::span<const std::byte> image) {
    const std::size_t start = image.size() > 0x58e000 ? 0x58e000 : 0;
    const std::size_t end = std::min<std::size_t>(image.size(), 0x59e000);
    std::optional<std::size_t> found = find_pattern(image.subspan(start, end - start));

    if (found) {
        *found += start;
    } else {
        found = find_pattern(image);
    }
    if (!found) {
        return std::nullopt;
    }

    std::uint32_t pointer_offset = 0;

    std::memcpy(&pointer_offset, image.data() + *found + 4, sizeof(pointer_offset));
    return pointer_offset ? std::optional{pointer_offset} : std::nullopt;
}

std::optional<std::uintptr_t> resolve_stratagem5_metadata(
    std::uint32_t pointer_offset, std::uintptr_t game_base) {
    std::uintptr_t troop_base = 0;

    if (!game_base || !pointer_offset || !platform::windows::read_memory(game_base + pointer_offset,
                                         std::as_writable_bytes(std::span(&troop_base, 1))) ||
        !troop_base || troop_base > std::numeric_limits<std::uintptr_t>::max() - 0x9d30) {
        return std::nullopt;
    }

    const std::uintptr_t table = troop_base + 0x9d30;

    return valid_table(table) ? std::optional{table} : std::nullopt;
}

std::optional<std::uintptr_t> resolve_stratagem5_metadata_from_owner(std::uintptr_t owner) {
    std::array<StratagemEntry, 5> entries{};
    std::uint32_t count = 0;

    if (!owner || !platform::windows::read_memory(owner + 0x100, std::as_writable_bytes(std::span(entries))) ||
        !platform::windows::read_memory(owner + 0x150, std::as_writable_bytes(std::span(&count, 1))) ||
        count < 1 || count > entries.size()) {
        return std::nullopt;
    }
    for (std::uint32_t index = 0; index < count; ++index) {
        const std::uintptr_t row = entries[index].data;
        std::array<std::byte, 5> ids{};

        if (!row || !platform::windows::read_memory(row + 8, ids) ||
            ids[0] != ids[2] || ids[0] != ids[4]) {
            continue;
        }
        const std::uint32_t id = std::to_integer<std::uint32_t>(ids[0]);

        if (id < 1 || id > 5 || row < (id - 1) * 0x20) {
            continue;
        }
        const std::uintptr_t table = row - (id - 1) * 0x20;
        bool matches = valid_table(table);

        for (std::uint32_t candidate = 0; matches && candidate < count; ++candidate) {
            const std::uint64_t address = entries[candidate].data;

            matches = address >= table && address - table < 5 * 0x20 && (address - table) % 0x20 == 0;
        }
        if (matches) {
            return table;
        }
    }
    return std::nullopt;
}

std::optional<std::uintptr_t> resolve_stratagem5_metadata(
    std::span<const std::byte> image, std::uintptr_t game_base) {
    const std::optional<std::uint32_t> offset = find_stratagem5_metadata_offset(image);

    return offset ? resolve_stratagem5_metadata(*offset, game_base) : std::nullopt;
}

} // namespace s8::game
