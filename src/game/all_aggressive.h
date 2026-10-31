#pragma once

#include "../domain/work_budget.h"

#include <cstddef>
#include <cstdint>

namespace s8::game {

struct AllAggressive {
    std::uintptr_t hero = 0;
    std::uintptr_t module_base = 0;
    std::uintptr_t roster = 0;
    std::size_t cursor = 0;
    std::uint64_t detected_ms = 0;
    std::uint64_t last_tick_ms = 0;
    bool applied = false;
    bool running = false;
};

void update_all_aggressive(AllAggressive& state, std::uintptr_t module_base, std::uintptr_t hero,
                           std::uint64_t now_ms, bool enabled, const domain::WorkBudget& work_budget = {});

} // namespace s8::game
