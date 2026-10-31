#include "terrain_bonus.h"

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

void write_stat(std::uintptr_t address, std::int16_t value) {
    std::int16_t current = 0;
    if (read_value(address, current) && current != value) {
        platform::windows::write_data_memory(address, std::as_bytes(std::span(&value, 1)));
    }
}

} // namespace

void update_terrain_bonus(TerrainBonusState& state, const BattleObservation& observation,
                          const domain::TerrainBonus& table, std::uint64_t now_ms, bool atk_def, bool all) {
    const bool enabled = atk_def || all;
    if (!enabled && !state.active) {
        return;
    }
    if (enabled && now_ms - state.last_update_ms < 300) {
        return;
    }
    state.last_update_ms = now_ms;
    state.active = enabled;
    for (std::size_t i = 0; i < observation.unit_count; ++i) {
        std::uintptr_t unit = 0, members = 0, troop = 0, terrain1 = 0, terrain2 = 0;
        std::uint8_t troop_type = 0, terrain = 0;
        if (!read_value(observation.unit_list + 8 + i * 0x10, unit) || !unit ||
            !read_value(unit + 0x18, members) || !members || !read_value(members + 0x20, troop) || !troop ||
            !read_value(troop + 8, troop_type) || !read_value(unit + 0x40, terrain1) || !terrain1 ||
            !read_value(terrain1 + 0x10, terrain2) || !terrain2 || !read_value(terrain2 + 8, terrain)) {
            continue;
        }
        const int bonus = enabled && troop_type >= 1 && troop_type <= 12 && terrain >= 1 && terrain <= 17
            ? table.values[troop_type][terrain] : 0;
        constexpr std::array<std::uintptr_t, 3> BASES{0x658, 0x65c, 0x660};
        constexpr std::array<std::uintptr_t, 3> TARGETS{0x600, 0x604, 0x608};
        for (std::size_t stat = 0; stat < BASES.size(); ++stat) {
            std::int16_t base = 0;
            if (!read_value(unit + BASES[stat], base)) {
                continue;
            }
            const int value = enabled && (stat == 0 || all)
                ? std::clamp(static_cast<int>(base) * (100 + bonus) / 100, 0, 32767) : base;
            write_stat(unit + TARGETS[stat], static_cast<std::int16_t>(value));
        }
    }
}

} // namespace s8::game
