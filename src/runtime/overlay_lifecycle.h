#pragma once

#include "../overlay/overlay_present.h"
#include "../platform/windows/present_hook.h"

#include <expected>

namespace s8::runtime {

struct OverlayLifecycle {
    services::UiBridge bridge;
    overlay::OverlayPresent overlay{.bridge = bridge};
    platform::windows::PresentHook present;
};

// The owner keeps this state alive until callbacks and trampolines are quiescent.
std::expected<void, int> start_overlay(OverlayLifecycle& state, HWND game_window);
int stop_overlay_entries(OverlayLifecycle& state);
// Call after Present and window callbacks have returned and the subclass is removed.
int finish_overlay_stop(OverlayLifecycle& state);

} // namespace s8::runtime
