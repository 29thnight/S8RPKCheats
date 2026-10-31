#pragma once

#include "battle_observation.h"
#include "../domain/work_budget.h"

#include <cstddef>
#include <cstdint>

namespace s8::game {

struct Siege {
    bool move_applied = false;
    bool terrain_applied = false;
    int previous_day = -1;
    std::uint64_t last_attempt_ms = 0;
    bool move_restore_pending = false;
    std::uintptr_t move_base = 0;
    std::uintptr_t move_module_base = 0;
    std::uintptr_t unit_list = 0;
    std::uintptr_t day_base = 0;
    std::uintptr_t terrain_map = 0;
    std::uintptr_t terrain_castle = 0;
    std::uintptr_t terrain_shallow = 0;
    std::size_t tile_cursor = 0;
    std::size_t delta_cursor = 0;
    int castle_index = -1;
    bool terrain_pending = false;
    bool terrain_wide = false;
    std::size_t heal_cursor = 0;
    std::size_t heal_count = 0;
    int healing_day = -1;
    int healing_rate = 0;
    std::uint8_t defender = 0;
    bool healing_pending = false;
    std::uintptr_t heal_target_unit = 0;
    std::uint16_t heal_before = 0;
    std::uint16_t heal_after = 0;
    bool heal_write_pending = false;
};

void reset_siege_battle(Siege& state);
bool disable_siege(Siege& state, std::uintptr_t module_base);
void update_siege(Siege& state, std::uintptr_t module_base, const BattleObservation& observation,
                  std::uint64_t now_ms, bool wide, int heal_rate, const domain::WorkBudget& work_budget = {});

} // namespace s8::game
