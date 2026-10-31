#pragma once

#include "native_patch.h"

namespace s8::game {

struct PrisonerCapture {
    NativePatchGroup patches{};
    std::uintptr_t module_base = 0;
    std::uintptr_t cave = 0;
};

// Deputy capture, isolated-city capture and AI execution classification follow
// the single PrisonerCaptureManagement option, with one all-or-rollback commit.
bool set_prisoner_capture(PrisonerCapture& state, std::uintptr_t module_base, bool enabled);

} // namespace s8::game
