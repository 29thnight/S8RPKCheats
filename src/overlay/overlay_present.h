#pragma once

#include "dx11_overlay.h"
#include "feature_menu.h"
#include "win32_input.h"

#include <atomic>

namespace s8::overlay {

struct OverlayPresent {
    Dx11Overlay renderer;
    FeatureMenu menu;
    services::UiBridge& bridge;
    HWND target_window = nullptr;
    std::atomic<IDXGISwapChain*> target_swap_chain{nullptr};
    std::atomic<bool> capture_mouse{false};
    std::atomic<bool> capture_keyboard{false};
    std::atomic<unsigned> toggle_requests{0};
    std::uint64_t last_menu_key_ms = 0;
    unsigned mouse_buttons = 0;
    bool owns_mouse_capture = false;
};

// Passed to PresentHook after the owner selects the target swap chain.
void on_overlay_present(void* context, IDXGISwapChain* swap_chain) noexcept;
// Returns true when this message was accepted and belongs to the overlay.
bool on_overlay_window_message(OverlayPresent& state, HWND window, UINT message, WPARAM wparam, LPARAM lparam);

} // namespace s8::overlay
