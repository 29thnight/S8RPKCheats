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

// The owner retains this state and published trampolines for process lifetime.
std::expected<void, int> start_overlay(OverlayLifecycle& state, HWND game_window);
// Close command admission and ask the render callback to release its resources.
int stop_overlay_entries(OverlayLifecycle& state);
// Returns ERROR_BUSY until the renderer acknowledges stop and the subclass is
// removed. Never removes a published trampoline or frees a borrowed context.
int finish_overlay_stop(OverlayLifecycle& state);

} // namespace s8::runtime
