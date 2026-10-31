#pragma once

#include "battle_observation.h"
#include "../domain/terrain_bonus.h"

#include <cstdint>

namespace s8::game {

struct TerrainBonusState {
    std::uint64_t last_update_ms = 0;
    bool active = false;
};

void update_terrain_bonus(TerrainBonusState& state, const BattleObservation& observation,
                          const domain::TerrainBonus& table, std::uint64_t now_ms, bool atk_def, bool all);

} // namespace s8::game
