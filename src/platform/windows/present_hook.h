#pragma once

#include "hook.h"

#include <expected>
#include <optional>

struct IDXGISwapChain;

namespace s8::platform::windows {

using PresentCallback = void (*)(void* context, IDXGISwapChain* swap_chain) noexcept;

struct PresentHook {
    std::optional<Hook> hook;
    PresentCallback callback = nullptr;
    void* context = nullptr;
};

std::expected<void, int> install_present_hook(PresentHook& state, void* target,
                                              PresentCallback callback, void* context);
int disable_present_hook(PresentHook& state);
// Call only after hook entry is closed and all callbacks and trampolines have returned.
int remove_present_hook(PresentHook& state);

} // namespace s8::platform::windows
