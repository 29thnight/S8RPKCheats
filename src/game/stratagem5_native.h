#pragma once

#include "../platform/windows/code_patch.h"

#include <cstddef>
#include <cstdint>
#include <span>

namespace s8::game {

enum class Stratagem5NativeIssue {
    None,
    UnsupportedBuild,
    HookFailure,
    ForeignThread,
    InvalidSession,
    WaitingModel,
    InvalidLayout,
    AllocationFailed,
    RegistryFailed,
    MemoryFailed
};

struct Stratagem5NativeView {
    bool prepared = false;
    bool installed = false;
    bool armed = false;
    bool registered = false;
    std::uint64_t layout_generation = 0;
    Stratagem5NativeIssue issue = Stratagem5NativeIssue::None;
    platform::windows::CodePatchResult patch_result = platform::windows::CodePatchResult::AlreadyApplied;
};

struct Stratagem5NativeContext {
    std::uintptr_t dialog = 0;
    std::uintptr_t attack_owner = 0;
    std::uintptr_t defense_owner = 0;
};

struct Stratagem5NativeCallbacks {
    void* context = nullptr;
    // The adapter invokes these only on the configuring game/window thread.
    // The caller revalidates the root and epoch before touching its Runtime.
    bool (*valid_session)(void*) = nullptr;
    bool (*prepare_model)(void*, const Stratagem5NativeContext&, std::uint32_t& original_count) = nullptr;
};

// One game executable has one native dialog implementation. Retained entry
// stubs and their atomic publications live in this pinned platform adapter;
// this value is the explicit owner token and contains no native address.
struct Stratagem5Native {
    Stratagem5NativeView view;
};

// Startup preparation only: inspect the immutable image and allocate/write
// unpublished caves. Does not bind a game thread or patch an executable entry.
bool prepare_stratagem5_native_code(Stratagem5Native& state, std::uintptr_t module_base,
    std::span<const std::byte> image);
bool prepare_stratagem5_native(Stratagem5Native& state, std::uintptr_t module_base,
    std::span<const std::byte> image, Stratagem5NativeCallbacks callbacks);
bool set_stratagem5_native_metadata(Stratagem5Native& state, bool enabled);
bool ensure_stratagem5_native_count_capture(Stratagem5Native& state);
bool update_stratagem5_native(Stratagem5Native& state, bool armed,
    std::uint64_t epoch, bool retire_battle);
bool quiesce_stratagem5_native(Stratagem5Native& state);
// Owner thread/window loss: only atomics, no game object access or patch write.
void close_stratagem5_native_admission() noexcept;
// Close admission first, then restore entry patches. Busy/failure retains all
// state for retry. Published code is never freed while native callers may return.
bool stop_stratagem5_native(Stratagem5Native& state);

} // namespace s8::game
