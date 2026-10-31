#include "terrain_bonus.h"

#include "../platform/windows/process_memory.h"

#include <algorithm>
#include <array>
#include <span>

namespace s8::game {
namespace {

constexpr std::size_t MAX_UNITS = 60;
constexpr std::size_t UNITS_PER_UPDATE = 8;
constexpr std::uint64_t UPDATE_INTERVAL_MS = 300;
constexpr std::array<std::uintptr_t, 3> BASES{0x658, 0x65c, 0x660};
constexpr std::array<std::uintptr_t, 3> TARGETS{0x600, 0x604, 0x608};

template <typename T>
bool read_value(std::uintptr_t address, T& value) {
    return platform::windows::read_memory(address, std::as_writable_bytes(std::span(&value, 1)));
}

bool write_stat(std::uintptr_t address, std::int16_t value) {
    std::int16_t current = 0;

    return read_value(address, current) && (current == value ||
        platform::windows::write_data_memory(address, std::as_bytes(std::span(&value, 1))));
}

bool update_unit(std::uintptr_t unit_slot, const domain::TerrainBonus& table, bool enabled, bool all) {
    std::uintptr_t unit = 0;
    std::uintptr_t members = 0;
    std::uintptr_t troop = 0;
    std::uintptr_t terrain1 = 0;
    std::uintptr_t terrain2 = 0;
    std::uint8_t troop_type = 0;
    std::uint8_t terrain = 0;

    if (!read_value(unit_slot, unit)) {
        return false;
    }
    if (!unit) {
        return true;
    }
    if (enabled && (!read_value(unit + 0x18, members) || !members ||
            !read_value(members + 0x20, troop) || !troop || !read_value(troop + 8, troop_type) ||
            !read_value(unit + 0x40, terrain1) || !terrain1 || !read_value(terrain1 + 0x10, terrain2) ||
            !terrain2 || !read_value(terrain2 + 8, terrain))) {
        return false;
    }

    const int bonus = enabled && troop_type >= 1 && troop_type <= 12 && terrain >= 1 && terrain <= 17
        ? table.values[troop_type][terrain] : 0;
    bool complete = true;

    // Once admitted, finish this unit's three stats before yielding.
    for (std::size_t stat = 0; stat < BASES.size(); ++stat) {
        std::int16_t base = 0;

        if (!read_value(unit + BASES[stat], base)) {
            complete = false;
            continue;
        }

        const int value = enabled && (stat == 0 || all)
            ? std::clamp(static_cast<int>(base) * (100 + bonus) / 100, 0, 32767) : base;

        complete = write_stat(unit + TARGETS[stat], static_cast<std::int16_t>(value)) && complete;
    }
    return complete;
}

} // namespace

void update_terrain_bonus(TerrainBonusState& state, const BattleObservation& observation,
                          const domain::TerrainBonus& table, std::uint64_t now_ms, bool atk_def, bool all,
                          const domain::WorkBudget& work_budget) {
    const bool enabled = atk_def || all;
    const std::size_t unit_count = std::min<std::size_t>(observation.unit_count, MAX_UNITS);
    const bool identity_changed = state.unit_list != observation.unit_list ||
        state.day_base != observation.day_base || state.unit_count != unit_count;
    const bool mode_changed = state.pending_enabled != enabled || state.pending_all != all;

    if (identity_changed || mode_changed) {
        state.unit_list = observation.unit_list;
        state.day_base = observation.day_base;
        state.unit_count = unit_count;
        state.unit_cursor = 0;
        state.pending = false;
        state.pending_failed = false;
        state.last_update_ms = 0;
    }
    if (!enabled && !state.active) {
        return;
    }
    if (!state.pending) {
        if (enabled && now_ms - state.last_update_ms < UPDATE_INTERVAL_MS) {
            return;
        }
        state.pending = true;
        state.pending_enabled = enabled;
        state.pending_all = all;
        state.pending_failed = false;
        state.unit_cursor = 0;
    }

    const bool bounded = work_budget.deadline != (std::chrono::steady_clock::time_point::max)();
    std::size_t processed = 0;

    while (state.unit_cursor < unit_count) {
        if (work_budget.exhausted() || (bounded && processed >= UNITS_PER_UPDATE)) {
            return;
        }
        ++processed;
        // Retain restoration intent even if an enabled write only partly succeeds.
        state.active = state.active || enabled;
        state.pending_failed = !update_unit(observation.unit_list + 8 + state.unit_cursor * 0x10,
            table, enabled, all) || state.pending_failed;
        ++state.unit_cursor;
    }
    state.pending = false;
    if (!state.pending_failed) {
        state.last_update_ms = now_ms;
        state.active = enabled;
    }
}

} // namespace s8::game
