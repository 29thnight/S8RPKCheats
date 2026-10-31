#pragma once

#include "native_patch.h"

namespace s8::game {

struct AffinityDisplay {
    NativePatchGroup patches{};
    std::uintptr_t module_base = 0;
    std::uintptr_t cave = 0;
};

// Includes the original VisibilityFix path for roster format/sort and the
// officer-info update. The gallery retains its original formatting contract.
bool set_affinity_display(AffinityDisplay& state, std::uintptr_t module_base, bool enabled);

} // namespace s8::game
