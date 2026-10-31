#include "siege.h"
#include "../platform/windows/process_memory.h"

#include <algorithm>
#include <array>
#include <span>

namespace s8::game {
namespace {

template <typename T>
bool read_value(std::uintptr_t address, T& value) {
    return platform::windows::read_memory(address, std::as_writable_bytes(std::span(&value, 1)));
}

std::uintptr_t resolve(std::uintptr_t address, std::span<const std::uintptr_t> offsets) {
    for (const auto offset : offsets) {
        std::uintptr_t next = 0;
        if (!read_value(address, next) || !next) {
            return 0;
        }
        address = next + offset;
    }
    return address;
}

bool set_move_cost(std::uintptr_t module_base, std::uint8_t value) {
    constexpr std::array<std::uintptr_t, 5> CHAIN{0, 8, 0x10, 0, 0x4b2f18};
    const auto base = resolve(module_base + 0x034c8630, CHAIN);
    if (!base) {
        return false;
    }
    for (std::uintptr_t i = 0; i < 12; ++i) {
        if (!platform::windows::write_memory(base + i * 0x28, std::as_bytes(std::span(&value, 1)))) {
            return false;
        }
    }
    return true;
}

bool set_shallow(std::uintptr_t module_base, bool wide) {
    constexpr std::array<std::uintptr_t, 5> CASTLE{0, 8, 0x10, 0, 0x1aa510};
    constexpr std::array<std::uintptr_t, 5> SHALLOW{0, 8, 0x10, 0, 0x1aa4b0};
    constexpr std::array<std::uintptr_t, 7> MAP{0, 0x78, 0, 0x50, 0x18, 0x1a0, 0x10};
    const auto castle = resolve(module_base + 0x034c8630, CASTLE);
    const auto shallow = resolve(module_base + 0x034c8630, SHALLOW);
    const auto map = resolve(module_base + 0x035100b8, MAP);
    if (!castle || !shallow || !map) {
        return false;
    }
    int first = -1;
    for (int i = 0; i < 600; ++i) {
        std::uintptr_t value = 0;
        if (read_value(map + i * 0x40, value) && value == castle) {
            first = i;
            break;
        }
    }
    if (first < 0) {
        return false;
    }
    constexpr std::array ODD{-21,-20,-19,-18,-17,-16,-1,5,19,24,39,45,59,64,79,85,99,100,101,102,103,104};
    constexpr std::array EVEN{-20,-19,-18,-17,-16,-15,-1,5,20,25,39,45,60,65,79,85,100,101,102,103,104,105};
    constexpr std::array WIDE_ODD{-41,-40,-39,-38,-37,-36,-35,-22,-21,-20,-19,-18,-17,-16,-15,
        -2,-1,5,6,18,19,24,25,38,39,45,46,58,59,64,65,78,79,85,86,98,99,100,101,102,103,104,105,
        119,120,121,122,123,124,125};
    constexpr std::array WIDE_EVEN{-41,-40,-39,-38,-37,-36,-35,-21,-20,-19,-18,-17,-16,-15,-14,
        -2,-1,5,6,19,20,25,26,38,39,45,46,59,60,65,66,78,79,85,86,99,100,101,102,103,104,105,106,
        119,120,121,122,123,124,125};
    const bool odd = (first / 20) % 2 != 0;
    const std::span<const int> deltas = wide ? (odd ? std::span<const int>(WIDE_ODD) : std::span<const int>(WIDE_EVEN))
                                              : (odd ? std::span<const int>(ODD) : std::span<const int>(EVEN));
    for (const int delta : deltas) {
        const auto address = static_cast<std::uintptr_t>(static_cast<std::intptr_t>(map) + (first + delta) * 0x40);
        platform::windows::write_data_memory(address, std::as_bytes(std::span(&shallow, 1)));
    }
    return true;
}

} // namespace

bool disable_siege(Siege& state, std::uintptr_t module_base) {
    if (state.move_applied && (!module_base || !set_move_cost(module_base, 255))) {
        return false;
    }
    state = {};
    return true;
}

void update_siege(Siege& state, std::uintptr_t module_base, const BattleObservation& observation,
                  std::uint64_t now_ms, bool wide, int heal_rate) {
    if (!module_base || observation.day < 1 || observation.day > 30) {
        return;
    }
    if (!state.move_applied) {
        if (now_ms - state.last_attempt_ms < 1000) {
            return;
        }
        state.last_attempt_ms = now_ms;
        if (!set_move_cost(module_base, 60)) {
            return;
        }
        state.move_applied = true;
    }
    if (!state.terrain_applied) {
        if (now_ms - state.last_attempt_ms < 1000) {
            return;
        }
        state.last_attempt_ms = now_ms;
        if (!set_shallow(module_base, wide)) {
            return;
        }
        state.terrain_applied = true;
    }
    if (state.previous_day == observation.day) {
        return;
    }
    state.previous_day = observation.day;
    constexpr std::array<std::uintptr_t, 8> DEFENDER{0x100,0x80,0,8,0xc8,8,0x18,8};
    const auto defender_address = resolve(module_base + 0x03510578, DEFENDER);
    std::uint8_t defender = 0;
    if (defender_address) {
        read_value(defender_address, defender);
    }
    constexpr std::array<std::uintptr_t, 3> FORCE{8,0x18,8};
    constexpr std::array<std::uintptr_t, 1> MAX{0};
    constexpr std::array<std::uintptr_t, 2> TERRAIN{0x10,8};
    for (std::size_t i = 0; i < observation.unit_count; ++i) {
        std::uintptr_t unit = 0;
        if (!read_value(observation.unit_list + 8 + i * 0x10, unit) || !unit) {
            continue;
        }
        const auto force_addr = resolve(unit + 0x18, FORCE);
        const auto max_addr = resolve(unit + 0x18, MAX);
        const auto terrain_addr = resolve(unit + 0x40, TERRAIN);
        std::uint8_t force = 0, terrain = 0;
        std::uint16_t maximum = 0, current = 0;
        if (!force_addr || !max_addr || !terrain_addr || !read_value(force_addr, force) ||
            !read_value(max_addr, maximum) || !read_value(unit + 0x38, current) || !read_value(terrain_addr, terrain)) {
            continue;
        }
        if (force == defender && terrain == 10 && current < maximum) {
            const int heal = std::max(1, static_cast<int>(maximum * (static_cast<float>(heal_rate) / 100.0f)));
            const auto updated = static_cast<std::uint16_t>(std::min(static_cast<int>(current) + heal, static_cast<int>(maximum)));
            platform::windows::write_memory(unit + 0x38, std::as_bytes(std::span(&updated, 1)));
        }
    }
}

} // namespace s8::game
