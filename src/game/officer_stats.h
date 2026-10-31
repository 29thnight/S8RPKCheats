#pragma once

#include "../domain/work_budget.h"

#include <cstddef>
#include <cstdint>

namespace s8::game {

struct OfficerStatsMonitor {
    std::uint8_t last_council_month = 0xff;
    std::uintptr_t module_base = 0;
    std::uintptr_t game_base = 0;
    std::uintptr_t roster = 0;
    std::size_t cursor = 0;
    bool running = false;
};

void update_officer_stats(OfficerStatsMonitor& state, std::uintptr_t module_base,
                          std::uintptr_t game_base, bool enabled, const domain::WorkBudget& work_budget = {});

} // namespace s8::game
