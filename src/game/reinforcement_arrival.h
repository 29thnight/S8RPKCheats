#pragma once

#include <cstdint>

namespace s8::game {

struct ReinforcementArrival {
    bool applied = false;
};

bool set_reinforcement_arrival(ReinforcementArrival& state, std::uintptr_t module_base, bool enabled);

} // namespace s8::game
