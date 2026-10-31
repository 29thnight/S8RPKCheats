#pragma once

#include "overlay_present.h"

namespace s8::overlay {

// Install and remove on the window's owning thread. Keep state alive until
// removal has completed and all window callbacks have returned.
bool install_window_subclass(HWND window, OverlayPresent& state);
bool remove_window_subclass(HWND window, OverlayPresent& state);
bool overlay_window_callbacks_idle(const OverlayPresent& state) noexcept;

} // namespace s8::overlay
