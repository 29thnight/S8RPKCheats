#pragma once

#include "hook.h"
#include "dx11_present_target.h"

#include <cstdint>
#include <expected>
#include <optional>

struct IDXGISwapChain;

namespace s8::platform::windows {

using PresentCallback = void (*)(void* context, IDXGISwapChain* swap_chain) noexcept;
using PresentResultCallback = void (*)(void* context, IDXGISwapChain* swap_chain,
                                       std::int32_t result) noexcept;

struct PresentCallbacks {
    PresentCallback present = nullptr;
    PresentCallback before_resize = nullptr;
    PresentResultCallback failed_present = nullptr;
    void* context = nullptr;
};

struct PresentHook {
    std::optional<Hook> hook;
    std::optional<Hook> resize_hook;
    PresentCallbacks callbacks;
    bool enabled = false;
};

std::expected<void, int> install_present_hook(PresentHook& state, Dx11PresentTargets targets,
                                              PresentCallbacks callbacks);
std::expected<void, int> install_present_hook(PresentHook& state, void* target,
                                              PresentCallback callback, void* context);
// These calls close callback admission before disabling either entry point.
int disable_present_hook(PresentHook& state);
bool present_callbacks_idle(const PresentHook& state) noexcept;
bool present_detours_idle(const PresentHook& state) noexcept;
// Detach the borrowed callback context after admission is closed and callbacks drain.
// Disabled hook/trampoline storage remains retained until actual quiescence is proved.
bool detach_present_callbacks(PresentHook& state) noexcept;
// Call only after hook entry is closed and all callbacks and trampolines have returned.
int remove_present_hook(PresentHook& state);

} // namespace s8::platform::windows
