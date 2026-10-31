#pragma once

#include "native_patch.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace s8::game {

enum class TraitViewerStatus {
    Disabled,
    Applied,
    UnsupportedVersion,
    AllocationFailed,
    LiveObjects,
    PatchFailed,
    RollbackFailed
};

struct TraitViewerBlock {
    NativePatchGroup patches{};
    std::uintptr_t module_base = 0;
    std::uintptr_t code = 0;
    bool prepared = false;
    bool exposed = false;

    TraitViewerBlock() = default;
    ~TraitViewerBlock();
    TraitViewerBlock(const TraitViewerBlock&) = delete;
    TraitViewerBlock& operator=(const TraitViewerBlock&) = delete;
};

struct TraitViewerNative {
    static constexpr std::size_t GROUP_COUNT = 9;

    std::array<TraitViewerBlock, GROUP_COUNT> groups{};
    TraitViewerStatus status = TraitViewerStatus::Disabled;
    std::size_t failed_group = GROUP_COUNT;
};

// Owner-thread commit operation. Prepares every image before publishing any
// branch. Published images contain only game/cave addresses and remain mapped
// for process lifetime, including after OFF, because game objects retain them.
bool set_trait_viewer_native(TraitViewerNative& state, std::uintptr_t module_base, bool enabled);
bool trait_viewer_native_applied(const TraitViewerNative& state);

} // namespace s8::game
