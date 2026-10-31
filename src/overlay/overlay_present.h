#pragma once

#include "dx11_overlay.h"
#include "feature_menu.h"
#include "win32_input.h"

#include <atomic>

namespace s8::overlay {

using WindowMessageCallback = bool (*)(void* context, HWND window, UINT message,
                                       WPARAM wparam, LPARAM lparam, LRESULT* result) noexcept;

struct OverlayPresent {
    Dx11Overlay renderer;
    FeatureMenu menu;
    services::UiBridge& bridge;
    HWND target_window = nullptr;
    std::atomic<IDXGISwapChain*> target_swap_chain{nullptr};
    std::atomic<bool> capture_mouse{false};
    std::atomic<bool> capture_keyboard{false};
    InputCaptureHooks input_capture;
    std::atomic<unsigned> toggle_requests{0};
    std::atomic<bool> render_failed{false};
    std::atomic<bool> render_stop_requested{false};
    std::atomic<bool> render_stopped{false};
    std::atomic<bool> window_attached{false};
    std::atomic<unsigned> window_callbacks{0};
    WindowMessageCallback message_callback = nullptr;
    void* message_context = nullptr;
    std::uint64_t last_menu_key_ms = 0;
    unsigned mouse_buttons = 0;
    bool owns_mouse_capture = false;
};

// PresentHook serializes these callbacks with ResizeBuffers and other Presents.
void on_overlay_present(void* context, IDXGISwapChain* swap_chain) noexcept;
void on_overlay_before_resize(void* context, IDXGISwapChain* swap_chain) noexcept;
void on_overlay_present_failed(void* context, IDXGISwapChain* swap_chain, std::int32_t result) noexcept;
void request_overlay_stop(OverlayPresent& state) noexcept;
// Returns true when this message was accepted and belongs to the overlay.
bool on_overlay_window_message(OverlayPresent& state, HWND window, UINT message, WPARAM wparam, LPARAM lparam);

} // namespace s8::overlay
