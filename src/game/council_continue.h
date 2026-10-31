#pragma once

#include "native_patch.h"

namespace s8::game {

struct CouncilContinue {
    NativePatchGroup patches{};
    std::uintptr_t module_base = 0;
    std::uintptr_t cave = 0;
};

bool set_council_continue(CouncilContinue& state, std::uintptr_t module_base, bool enabled);
// Clear the retained cave's pending manual-battle identity at a session boundary.
bool reset_council_continue(CouncilContinue& state);

} // namespace s8::game
