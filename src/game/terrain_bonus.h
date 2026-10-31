#pragma once

#include "battle_observation.h"
#include "../domain/terrain_bonus.h"
#include "../domain/work_budget.h"

#include <cstddef>
#include <cstdint>

namespace s8::game {

struct TerrainBonusState {
    std::uint64_t last_update_ms = 0;
    bool active = false;
    std::uintptr_t unit_list = 0;
    std::uintptr_t day_base = 0;
    std::size_t unit_count = 0;
    std::size_t unit_cursor = 0;
    bool pending = false;
    bool pending_enabled = false;
    bool pending_all = false;
    bool pending_failed = false;
};

void update_terrain_bonus(TerrainBonusState& state, const BattleObservation& observation,
                          const domain::TerrainBonus& table, std::uint64_t now_ms, bool atk_def, bool all,
                          const domain::WorkBudget& work_budget = {});

} // namespace s8::game
