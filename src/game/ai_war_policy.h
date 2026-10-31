#pragma once

#include "native_patch.h"

namespace s8::game {

struct AiWarPolicy {
    NativePatchGroup patches{};
    std::uintptr_t module_base = 0;
    std::uintptr_t cave = 0;
};

// The four battle decisions, refusal-to-attack policy, and high-honor surrender
// policy are one transaction, matching the original AIWarImprove option.
bool set_ai_war_policy(AiWarPolicy& state, std::uintptr_t module_base, bool enabled);

} // namespace s8::game
