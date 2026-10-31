#pragma once

#include "native_patch.h"

namespace s8::game {

struct IsolatedTerritory {
    NativePatchGroup patches{};
    std::uintptr_t module_base = 0;
    std::uintptr_t cave = 0;
};

bool set_isolated_territory(IsolatedTerritory& state, std::uintptr_t module_base, bool enabled);

} // namespace s8::game
