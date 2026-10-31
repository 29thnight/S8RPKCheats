#pragma once

#include "native_patch.h"

namespace s8::game {

struct RulerTransferProposal {
    NativePatchGroup patches{};
    std::uintptr_t module_base = 0;
    std::uintptr_t cave = 0;
};

// The original checkbox and saved preference select mode 2 (direct advisor
// and governor) when enabled, mode 0 when disabled. Mode 1 has no UI entry.
// No per-epoch reset is needed: the callback validates the current root/P1.
bool set_ruler_transfer_proposal(RulerTransferProposal& state,
    std::uintptr_t module_base, bool enabled);

} // namespace s8::game
