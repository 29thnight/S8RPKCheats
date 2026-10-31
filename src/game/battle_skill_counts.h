#pragma once

#include "battle_observation.h"
#include "../domain/skill_counts.h"

#include <cstdint>

namespace s8::game {

struct BattleSkillCounts {
    bool initialized = false;
    std::uint8_t last_day = 0;
};

void update_battle_skill_counts(BattleSkillCounts& state, const BattleObservation& observation,
                                const domain::SkillCounts& counts);

} // namespace s8::game
