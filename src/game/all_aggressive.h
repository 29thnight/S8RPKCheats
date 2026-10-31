#pragma once

#include <cstdint>

namespace s8::game {

struct AllAggressive {
    std::uintptr_t hero = 0;
    std::uint64_t detected_ms = 0;
    std::uint64_t last_tick_ms = 0;
    bool applied = false;
};

void update_all_aggressive(AllAggressive& state, std::uintptr_t module_base, std::uintptr_t hero,
                           std::uint64_t now_ms, bool enabled);

} // namespace s8::game
