#pragma once

#include "../domain/work_budget.h"

#include <cstddef>
#include <cstdint>

namespace s8::game {

struct TechZero {
    bool enabled = false;
    bool done = false;
    bool pending = false;
    bool running = false;
    std::uintptr_t table_root = 0;
    std::size_t cursor = 0;
    std::uint8_t active_month = 0;
    std::uint64_t last_poll_ms = 0;
    std::uint64_t trigger_ms = 0;
};

void set_tech_zero(TechZero& state, bool enabled);
void tick_tech_zero(TechZero& state, std::uintptr_t module_base, std::uint64_t now_ms,
    const domain::WorkBudget& work_budget = {});

} // namespace s8::game
