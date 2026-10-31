#pragma once

#include "native_patch.h"
#include "../domain/ids.h"

namespace s8::game {

struct ReinforcementPlacement {
    NativePatchGroup patches{};
    std::uintptr_t module_base = 0;
    std::uintptr_t cave = 0;
};

void change_reinforcement_epoch(domain::GameEpoch epoch);
bool set_reinforcement_placement(ReinforcementPlacement& state, std::uintptr_t module_base, bool enabled);

} // namespace s8::game
