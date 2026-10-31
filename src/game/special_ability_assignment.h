#pragma once

#include "../domain/ids.h"
#include "../domain/ownership.h"
#include "../domain/skill_counts.h"
#include "../domain/special_ability_assignment.h"
#include "../domain/work_budget.h"

#include <array>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <span>

namespace s8::game {

struct SpecialAbilityAssignment {
    domain::SpecialAssignmentSummary summary{};
    domain::GameEpoch epoch{};
    std::uintptr_t module_base = 0;
    std::uintptr_t game_base = 0;
    std::uintptr_t player = 0;
    std::uintptr_t roster = 0;
    std::size_t pending_count = 0;
    std::size_t commit_cursor = 0;
    std::size_t result_count = 0;
    std::bitset<domain::SPECIAL_ASSIGNMENT_OFFICER_LIMIT + 1> seen;
    std::array<std::byte, 0x3d0> record{};
    std::array<domain::SpecialAssignmentResult, domain::SPECIAL_ASSIGNMENT_OFFICER_LIMIT> pending{};
    own::shared_owner<const domain::SkillCounts> existing;
};

bool start_special_ability_assignment(SpecialAbilityAssignment& state, std::uintptr_t module_base,
                                       std::uintptr_t game_base, std::uintptr_t player,
                                       domain::GameEpoch epoch,
                                       own::shared_owner<const domain::SkillCounts> existing,
                                       bool annual);

// Scanning is cancellable without changes. Once the bounded commit starts,
// complete it; existing positive manual values are preserved throughout.
bool cancel_special_ability_assignment(SpecialAbilityAssignment& state);

// At most 32 reads or eight assignment commits per call. True means a new
// terminal report: publish/save counts once if summary.assigned is nonzero.
bool update_special_ability_assignment(SpecialAbilityAssignment& state, std::uintptr_t module_base,
                                        std::uintptr_t game_base, std::uintptr_t player,
                                        domain::GameEpoch epoch, domain::SkillCounts& counts,
                                        const domain::WorkBudget& work_budget = {});

std::span<const domain::SpecialAssignmentResult> special_ability_assignment_results(
    const SpecialAbilityAssignment& state);

} // namespace s8::game
