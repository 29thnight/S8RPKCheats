#include "faction_lord_bonus.h"
#include "../platform/windows/process_memory.h"

#include <algorithm>
#include <array>
#include <span>
#include <utility>

namespace s8::game {
namespace {

template <typename T>
bool read_value(std::uintptr_t address, T& value) {
    return platform::windows::read_memory(address, std::as_writable_bytes(std::span(&value, 1)));
}

std::uintptr_t root_address(std::uintptr_t module_base) {
    if (!module_base) {
        return 0;
    }
    auto address = module_base + 0x034c8630;
    constexpr std::array<std::uintptr_t, 5> OFFSETS{0, 0, 8, 0x10, 0};
    for (const auto offset : OFFSETS) {
        std::uintptr_t next = 0;
        if (!read_value(address + offset, next) || !next) {
            return 0;
        }
        address = next;
    }
    return address;
}

void write_bytes(std::uintptr_t address, std::span<const std::byte> desired) {
    std::array<std::byte, 48> current{};
    const auto data = std::span(current).first(desired.size());
    if (platform::windows::read_memory(address, data) && !std::equal(data.begin(), data.end(), desired.begin())) {
        platform::windows::write_data_memory(address, desired);
    }
}

void clear_lord(std::uintptr_t address, std::uintptr_t root) {
    std::uintptr_t current = 0;
    if (!read_value(address, current)) {
        return;
    }
    for (std::uintptr_t i = 0; i < 5; ++i) {
        if (current == root + 0x4ec6d8 + i * 24) {
            constexpr std::uintptr_t ZERO = 0;
            platform::windows::write_data_memory(address, std::as_bytes(std::span(&ZERO, 1)));
            break;
        }
    }
}

} // namespace

void set_faction_lord_bonus(FactionLordBonus& state, std::uintptr_t module_base, bool enabled) {
    if (state.enabled == enabled) {
        return;
    }
    state.enabled = enabled;
    state.last_tick_ms = 0;
    if (enabled) {
        state.names_applied = false;
        return;
    }
    const auto root = root_address(module_base);
    if (!root) {
        return;
    }
    for (const auto address : state.assigned) {
        clear_lord(address, root);
    }
    state.assigned.clear();
    constexpr std::array<std::uint8_t, 15> ZEROS{};
    for (std::uintptr_t i = 0; i < 5; ++i) {
        write_bytes(root + 0x4ec6d8 + i * 24 + 9, std::as_bytes(std::span(ZEROS)));
    }
    constexpr std::array<std::uint8_t, 48> NAMES_ZERO{};
    write_bytes(root + 0x197a774, std::as_bytes(std::span(NAMES_ZERO)));
    state.names_applied = false;
}

void update_faction_lord_bonus(FactionLordBonus& state, std::uintptr_t module_base, std::uint64_t now_ms) {
    if (!state.enabled || (state.last_tick_ms && now_ms - state.last_tick_ms < 5000)) {
        return;
    }
    state.last_tick_ms = now_ms;
    const auto root = root_address(module_base);
    if (!root) {
        return;
    }
    constexpr std::array<std::uint8_t, 48> NAMES{
        0x69,0xd6,0x1c,0xc8,0,0,0,0, 0x55,0xc6,0,0,0,0,0,0,
        0xf5,0xac,0,0,0,0,0,0, 0xfc,0xc8,0xa9,0xba,0,0,0,0,
        0x70,0xad,0xfc,0xc8,0,0,0,0, 0x9c,0xcc,0xf5,0xac,0xa5,0xc7,0x70,0xad};
    if (!state.names_applied) {
        write_bytes(root + 0x197a774, std::as_bytes(std::span(NAMES)));
        state.names_applied = true;
    }
    constexpr std::array<std::array<std::uint8_t, 15>, 5> PATCHES{{
        {0x54,0,0x14,0x88,0x13,5,5,5,5,5,0,1,3,1,0},
        {0x56,0,0x14,0xb8,0x0b,4,4,4,4,4,0,1,3,1,0},
        {0x58,0,0x14,0xd0,0x07,3,3,3,3,3,0,1,3,1,0},
        {0x5a,0,0x14,0xe8,0x03,2,2,2,2,2,0,1,3,1,0},
        {0x5c,0,0x14,0,0,1,1,1,1,1,0,1,3,1,0},
    }};
    for (std::size_t i = 0; i < PATCHES.size(); ++i) {
        write_bytes(root + 0x4ec6d8 + i * 24 + 9, std::as_bytes(std::span(PATCHES[i])));
    }
    std::vector<std::uintptr_t> current_assigned;
    for (std::uintptr_t i = 0; i < 120; ++i) {
        const auto faction = root + i * 0x998;
        std::uint8_t exists = 0, wandering = 1;
        std::uintptr_t lord = 0, title = 0;
        if (!read_value(faction + 0xc690d, exists) || !exists ||
            !read_value(faction + 0xc6a28, wandering) || wandering ||
            !read_value(faction + 0xc6908, lord) || !lord ||
            !platform::windows::is_committed_memory(lord, 0x338)) {
            continue;
        }
        read_value(faction + 0xc6a20, title);
        std::uint8_t rank = 0;
        if (title) {
            read_value(title + 0x0e, rank);
        }
        const auto target = root + 0x4ec6d8 + (rank >= 1 && rank <= 4 ? rank - 1 : 4) * 24;
        const auto address = lord + 0x330;
        current_assigned.push_back(address);
        std::uintptr_t current = 0;
        if (read_value(address, current) && current != target) {
            platform::windows::write_data_memory(address, std::as_bytes(std::span(&target, 1)));
        }
    }
    for (const auto address : state.assigned) {
        if (std::find(current_assigned.begin(), current_assigned.end(), address) == current_assigned.end()) {
            clear_lord(address, root);
        }
    }
    state.assigned = std::move(current_assigned);
}

} // namespace s8::game
