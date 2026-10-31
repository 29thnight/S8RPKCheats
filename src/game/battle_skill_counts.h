#pragma once

#include "battle_observation.h"
#include "../domain/skill_counts.h"
#include "../domain/work_budget.h"

#include <cstddef>
#include <cstdint>

namespace s8::game {

struct BattleSkillCounts {
    bool initialized = false;
    std::uint8_t last_day = 0;
    std::uintptr_t unit_list = 0;
    std::uintptr_t day_base = 0;
    std::uintptr_t unit_identity = 0;
    std::uintptr_t skill_identity = 0;
    std::uint16_t officer_id = 0;
    std::size_t unit_count = 0;
    std::size_t unit_cursor = 0;
    std::size_t skill_cursor = 0;
    std::uint8_t pending_day = 0;
    bool pending = false;
};

void update_battle_skill_counts(BattleSkillCounts& state, const BattleObservation& observation,
                                const domain::SkillCounts& counts, const domain::WorkBudget& work_budget = {});

} // namespace s8::game
