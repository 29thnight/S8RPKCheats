#pragma once

#include "../domain/work_budget.h"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace s8::game {

struct UndiscoveredRonin {
    std::uintptr_t last_game_base = 0;
    std::uintptr_t last_player = 0;
    std::uintptr_t record_base = 0;
    std::uintptr_t scenario = 0;
    std::size_t cursor = 0;
    std::uint16_t year = 0;
    int modified_count = 0;
    std::uint64_t last_tick_ms = 0;
    bool enabled = false;
    bool pending = false;
    std::optional<int> completed_modified;
};

void update_undiscovered_ronin(UndiscoveredRonin& state, std::uintptr_t module_base,
                               std::uintptr_t game_base, std::uintptr_t player,
                               std::uint64_t now_ms, bool enabled, const domain::WorkBudget& work_budget = {});

} // namespace s8::game
