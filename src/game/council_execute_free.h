#pragma once

#include "../domain/work_budget.h"

#include "native_patch.h"
#include "../domain/ids.h"

namespace s8::game {

// This is the original manager pointer-table limit, independent of the 5102
// entries used by the physical officer-roster editor.
inline constexpr std::size_t COUNCIL_OFFICER_SLOT_LIMIT = 0x1450;

struct CouncilExecuteFree {
    NativePatchGroup patches{};
    std::uintptr_t module_base = 0;
    std::uintptr_t cave = 0;
    domain::GameEpoch epoch{};
    std::uintptr_t scan_root = 0;
    std::uintptr_t scan_hero = 0;
    std::uintptr_t scan_force = 0;
    std::array<std::uint16_t, COUNCIL_OFFICER_SLOT_LIMIT> candidates{};
    std::size_t candidate_count = 0;
    std::size_t next_slot = 1;
    bool ready = false;
    bool preparing_enable = false;
};

bool set_council_execute_free(CouncilExecuteFree& state, std::uintptr_t module_base,
    domain::GameEpoch epoch, bool enabled);
void update_council_execute_candidates(CouncilExecuteFree& state, std::uintptr_t module_base,
    domain::GameEpoch epoch, std::size_t slot_budget = 128, const domain::WorkBudget& work_budget = {});
void reset_council_execute_free(CouncilExecuteFree& state, domain::GameEpoch epoch);

} // namespace s8::game
