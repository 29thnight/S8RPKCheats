#pragma once

#include "native_patch.h"

namespace s8::game {

struct GovernorPrisonerDisposal {
    NativePatchGroup patches{};
    std::uintptr_t module_base = 0;
    // Published caves are retained for process lifetime, including after OFF.
    // The option byte lives with the cave, never in a transient Runtime object.
    std::uintptr_t cave = 0;
    bool consume_privilege = false;
};

bool set_governor_prisoner_disposal(GovernorPrisonerDisposal& state,
    std::uintptr_t module_base, bool enabled, bool consume_privilege);

} // namespace s8::game
