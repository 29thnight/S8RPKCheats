#include "siege.h"
#include "../platform/windows/process_memory.h"

#include <algorithm>
#include <array>
#include <span>

namespace s8::game {
namespace {

constexpr std::size_t MAX_UNITS = 60;
constexpr std::size_t MAP_TILES = 600;
constexpr std::size_t TILES_PER_UPDATE = 32;
constexpr std::size_t UNITS_PER_UPDATE = 8;
constexpr std::uint64_t RETRY_INTERVAL_MS = 1000;
constexpr std::array<std::uintptr_t, 5> MOVE_CHAIN{0, 8, 0x10, 0, 0x4b2f18};
constexpr std::array<std::uintptr_t, 5> CASTLE_CHAIN{0, 8, 0x10, 0, 0x1aa510};
constexpr std::array<std::uintptr_t, 5> SHALLOW_CHAIN{0, 8, 0x10, 0, 0x1aa4b0};
constexpr std::array<std::uintptr_t, 7> MAP_CHAIN{0, 0x78, 0, 0x50, 0x18, 0x1a0, 0x10};
constexpr std::array<std::uintptr_t, 8> DEFENDER_CHAIN{0x100, 0x80, 0, 8, 0xc8, 8, 0x18, 8};
constexpr std::array<std::uintptr_t, 3> FORCE_CHAIN{8, 0x18, 8};
constexpr std::array<std::uintptr_t, 1> MAX_CHAIN{0};
constexpr std::array<std::uintptr_t, 2> TERRAIN_CHAIN{0x10, 8};
constexpr std::array<int, 22> ODD{-21, -20, -19, -18, -17, -16, -1, 5, 19, 24, 39, 45,
    59, 64, 79, 85, 99, 100, 101, 102, 103, 104};
constexpr std::array<int, 22> EVEN{-20, -19, -18, -17, -16, -15, -1, 5, 20, 25, 39, 45,
    60, 65, 79, 85, 100, 101, 102, 103, 104, 105};
constexpr std::array<int, 50> WIDE_ODD{-41, -40, -39, -38, -37, -36, -35, -22, -21, -20,
    -19, -18, -17, -16, -15, -2, -1, 5, 6, 18, 19, 24, 25, 38, 39, 45, 46, 58, 59, 64, 65,
    78, 79, 85, 86, 98, 99, 100, 101, 102, 103, 104, 105, 119, 120, 121, 122, 123, 124, 125};
constexpr std::array<int, 50> WIDE_EVEN{-41, -40, -39, -38, -37, -36, -35, -21, -20, -19,
    -18, -17, -16, -15, -14, -2, -1, 5, 6, 19, 20, 25, 26, 38, 39, 45, 46, 59, 60, 65, 66,
    78, 79, 85, 86, 99, 100, 101, 102, 103, 104, 105, 106, 119, 120, 121, 122, 123, 124, 125};

enum class TerrainProgress { Pending, Failed, Complete };

template <typename T>
bool read_value(std::uintptr_t address, T& value) {
    return platform::windows::read_memory(address, std::as_writable_bytes(std::span(&value, 1)));
}

bool admit_item(const domain::WorkBudget& budget, std::size_t processed, std::size_t limit) {
    const bool bounded = budget.deadline != (std::chrono::steady_clock::time_point::max)();

    return !budget.exhausted() && (!bounded || processed < limit);
}

std::uintptr_t resolve(std::uintptr_t address, std::span<const std::uintptr_t> offsets) {
    for (const std::uintptr_t offset : offsets) {
        std::uintptr_t next = 0;

        if (!read_value(address, next) || !next) {
            return 0;
        }
        address = next + offset;
    }
    return address;
}

bool set_move_cost(std::uintptr_t base, std::uint8_t value) {
    bool complete = true;

    // This fixed-size transaction finishes once admitted, including restoration.
    for (std::uintptr_t index = 0; index < 12; ++index) {
        complete = platform::windows::write_memory(base + index * 0x28,
            std::as_bytes(std::span(&value, 1))) && complete;
    }
    return complete;
}

TerrainProgress update_shallow(Siege& state, std::uintptr_t module_base, bool wide,
                               const domain::WorkBudget& work_budget) {
    if (!admit_item(work_budget, 0, TILES_PER_UPDATE)) {
        return TerrainProgress::Pending;
    }

    const std::uintptr_t castle = resolve(module_base + 0x034c8630, CASTLE_CHAIN);
    const std::uintptr_t shallow = resolve(module_base + 0x034c8630, SHALLOW_CHAIN);
    const std::uintptr_t map = resolve(module_base + 0x035100b8, MAP_CHAIN);

    if (!castle || !shallow || !map) {
        return TerrainProgress::Failed;
    }
    if (state.terrain_map != map || state.terrain_castle != castle || state.terrain_shallow != shallow ||
        state.terrain_wide != wide) {
        state.terrain_map = map;
        state.terrain_castle = castle;
        state.terrain_shallow = shallow;
        state.terrain_wide = wide;
        state.tile_cursor = 0;
        state.delta_cursor = 0;
        state.castle_index = -1;
    }

    std::size_t processed = 0;

    while (state.castle_index < 0 && state.tile_cursor < MAP_TILES) {
        if (!admit_item(work_budget, processed, TILES_PER_UPDATE)) {
            return TerrainProgress::Pending;
        }
        ++processed;

        std::uintptr_t value = 0;

        if (read_value(map + state.tile_cursor * 0x40, value) && value == castle) {
            state.castle_index = static_cast<int>(state.tile_cursor);
        }
        ++state.tile_cursor;
    }
    if (state.castle_index < 0) {
        state.tile_cursor = 0;
        return TerrainProgress::Failed;
    }

    const bool odd = (state.castle_index / 20) % 2 != 0;
    const std::span<const int> deltas = wide
        ? (odd ? std::span<const int>(WIDE_ODD) : std::span<const int>(WIDE_EVEN))
        : (odd ? std::span<const int>(ODD) : std::span<const int>(EVEN));

    while (state.delta_cursor < deltas.size()) {
        if (!admit_item(work_budget, processed, TILES_PER_UPDATE)) {
            return TerrainProgress::Pending;
        }
        ++processed;

        const int tile = state.castle_index + deltas[state.delta_cursor];

        if (tile >= 0 && tile < static_cast<int>(MAP_TILES)) {
            const std::uintptr_t address = map + static_cast<std::uintptr_t>(tile) * 0x40;

            if (!platform::windows::write_data_memory(address, std::as_bytes(std::span(&shallow, 1)))) {
                return TerrainProgress::Failed;
            }
        }
        ++state.delta_cursor;
    }
    return TerrainProgress::Complete;
}

bool finish_heal_write(Siege& state) {
    std::uint16_t current = 0;

    if (!read_value(state.heal_target_unit + 0x38, current)) {
        return false;
    }
    if (current == state.heal_after || current != state.heal_before) {
        // A later game write supersedes this observation; do not overwrite it.
        state.heal_write_pending = false;
        return true;
    }

    platform::windows::write_memory(state.heal_target_unit + 0x38,
        std::as_bytes(std::span(&state.heal_after, 1)));

    std::uint16_t verified = 0;

    if (!read_value(state.heal_target_unit + 0x38, verified) || verified != state.heal_after) {
        return false;
    }
    state.heal_write_pending = false;
    return true;
}

} // namespace

void reset_siege_battle(Siege& state) {
    const bool move_applied = state.move_applied;
    const bool move_restore_pending = state.move_restore_pending;
    const std::uintptr_t move_base = state.move_base;
    const std::uintptr_t move_module_base = state.move_module_base;

    state = {};
    state.move_applied = move_applied;
    state.move_restore_pending = move_restore_pending;
    state.move_base = move_base;
    state.move_module_base = move_module_base;
}

bool disable_siege(Siege& state, std::uintptr_t module_base) {
    if (state.move_applied || state.move_restore_pending) {
        if (!module_base || state.move_module_base != module_base || !state.move_base ||
            resolve(module_base + 0x034c8630, MOVE_CHAIN) != state.move_base) {
            return false;
        }
        state.move_applied = false;
        if (!set_move_cost(state.move_base, 255)) {
            return false;
        }
    }
    state = {};
    return true;
}

void update_siege(Siege& state, std::uintptr_t module_base, const BattleObservation& observation,
                  std::uint64_t now_ms, bool wide, int heal_rate, const domain::WorkBudget& work_budget) {
    if (!module_base || observation.day < 1 || observation.day > 30 || work_budget.exhausted()) {
        return;
    }
    if (state.unit_list != observation.unit_list || state.day_base != observation.day_base) {
        reset_siege_battle(state);
        state.unit_list = observation.unit_list;
        state.day_base = observation.day_base;
    }

    const std::uintptr_t move_base = resolve(module_base + 0x034c8630, MOVE_CHAIN);

    if (!move_base || ((state.move_applied || state.move_restore_pending) &&
        (state.move_module_base != module_base || state.move_base != move_base))) {
        return;
    }
    if (!state.move_applied) {
        if (now_ms - state.last_attempt_ms < RETRY_INTERVAL_MS || work_budget.exhausted()) {
            return;
        }
        // Record ownership before the first write, including a partial failure.
        state.move_base = move_base;
        state.move_module_base = module_base;
        state.move_restore_pending = true;
        state.move_applied = set_move_cost(move_base, 60);
        state.last_attempt_ms = now_ms;
        if (!state.move_applied) {
            return;
        }
    }
    if (!state.terrain_applied) {
        if (!state.terrain_pending && now_ms - state.last_attempt_ms < RETRY_INTERVAL_MS) {
            return;
        }
        state.terrain_pending = true;

        const TerrainProgress progress = update_shallow(state, module_base, wide, work_budget);

        if (progress == TerrainProgress::Pending) {
            return;
        }
        state.last_attempt_ms = now_ms;
        state.terrain_pending = false;
        if (progress != TerrainProgress::Complete) {
            return;
        }
        state.terrain_applied = true;
    }
    if (!state.healing_pending) {
        if (state.previous_day == observation.day || work_budget.exhausted()) {
            return;
        }

        const std::uintptr_t defender_address = resolve(module_base + 0x03510578, DEFENDER_CHAIN);
        std::uint8_t defender = 0;

        if (!defender_address || !read_value(defender_address, defender)) {
            return;
        }
        state.healing_pending = true;
        state.healing_day = observation.day;
        state.healing_rate = heal_rate;
        state.defender = defender;
        state.heal_cursor = 0;
        state.heal_count = std::min<std::size_t>(observation.unit_count, MAX_UNITS);
    }

    const std::size_t count = std::min<std::size_t>(state.heal_count, observation.unit_count);
    std::size_t processed = 0;

    while (state.heal_cursor < count) {
        if (!admit_item(work_budget, processed, UNITS_PER_UPDATE)) {
            return;
        }
        ++processed;

        std::uintptr_t unit = 0;

        if (!read_value(observation.unit_list + 8 + state.heal_cursor * 0x10, unit)) {
            if (state.heal_write_pending) {
                return;
            }
            ++state.heal_cursor;
            continue;
        }
        if (state.heal_write_pending) {
            if (unit == state.heal_target_unit) {
                if (!finish_heal_write(state)) {
                    return;
                }
                ++state.heal_cursor;
                continue;
            }
            state.heal_write_pending = false;
        }
        if (!unit) {
            ++state.heal_cursor;
            continue;
        }

        const std::uintptr_t force_address = resolve(unit + 0x18, FORCE_CHAIN);
        const std::uintptr_t max_address = resolve(unit + 0x18, MAX_CHAIN);
        const std::uintptr_t terrain_address = resolve(unit + 0x40, TERRAIN_CHAIN);
        std::uint8_t force = 0;
        std::uint8_t terrain = 0;
        std::uint16_t maximum = 0;
        std::uint16_t current = 0;

        if (!force_address || !max_address || !terrain_address || !read_value(force_address, force) ||
            !read_value(max_address, maximum) || !read_value(unit + 0x38, current) ||
            !read_value(terrain_address, terrain)) {
            ++state.heal_cursor;
            continue;
        }
        if (force == state.defender && terrain == 10 && current < maximum) {
            const int heal = std::max(1, static_cast<int>(maximum * (static_cast<float>(state.healing_rate) / 100.0f)));

            state.heal_target_unit = unit;
            state.heal_before = current;
            state.heal_after = static_cast<std::uint16_t>(
                std::min(static_cast<int>(current) + heal, static_cast<int>(maximum)));
            state.heal_write_pending = true;
            if (!finish_heal_write(state)) {
                return;
            }
        }
        ++state.heal_cursor;
    }
    state.healing_pending = false;
    state.heal_write_pending = false;
    state.previous_day = state.healing_day;
}

} // namespace s8::game
