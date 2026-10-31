#include "window_subclass.h"

#include <CommCtrl.h>
#include <imm.h>
#include <windowsx.h>

#pragma comment(lib, "comctl32.lib")

namespace s8::overlay {
namespace {

struct WindowCallbackScope {
    OverlayPresent& state;

    explicit WindowCallbackScope(OverlayPresent& value) noexcept : state(value) {
        state.window_callbacks.fetch_add(1, std::memory_order_acq_rel);
    }

    ~WindowCallbackScope() {
        state.window_callbacks.fetch_sub(1, std::memory_order_release);
    }
};

LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam,
                             UINT_PTR id, DWORD_PTR data) {
    OverlayPresent& state = *reinterpret_cast<OverlayPresent*>(data);
    const WindowCallbackScope callback_scope(state);
    LRESULT dispatched_result = 0;

    if (state.message_callback && state.message_callback(state.message_context, window, message,
                                                         wparam, lparam, &dispatched_result)) {
        return dispatched_result;
    }

    if (message == WM_MOUSEMOVE) {
        TRACKMOUSEEVENT tracking{sizeof(tracking), TME_LEAVE, window, 0};

        TrackMouseEvent(&tracking);
    }
    if (message == WM_NCDESTROY) {
        RemoveWindowSubclass(window, window_proc, id);
        state.window_attached.store(false, std::memory_order_release);
        state.capture_mouse.store(false, std::memory_order_release);
        state.capture_keyboard.store(false, std::memory_order_release);
        state.renderer.input.request_reset();
        return DefSubclassProc(window, message, wparam, lparam);
    }
    const bool capture = state.capture_keyboard.load();

    if (capture && message == WM_IME_SETCONTEXT) {
        return DefWindowProcW(window, message, wparam, lparam | ISC_SHOWUICOMPOSITIONWINDOW);
    }
    if (capture && message == WM_IME_COMPOSITION) {
        on_overlay_window_message(state, window, message, wparam, lparam);
        return DefWindowProcW(window, message, wparam, lparam);
    }
    if (message == WM_CAPTURECHANGED) {
        state.mouse_buttons = 0;
        state.owns_mouse_capture = false;
    }
    bool handled = on_overlay_window_message(state, window, message, wparam, lparam);
    unsigned button = 0;
    bool pressed = false;

    switch (message) {
    case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK: button = 1; pressed = true; break;
    case WM_RBUTTONDOWN: case WM_RBUTTONDBLCLK: button = 2; pressed = true; break;
    case WM_MBUTTONDOWN: case WM_MBUTTONDBLCLK: button = 4; pressed = true; break;
    case WM_XBUTTONDOWN: case WM_XBUTTONDBLCLK:
        button = GET_XBUTTON_WPARAM(wparam) == XBUTTON1 ? 8 : 16; pressed = true; break;
    case WM_LBUTTONUP: button = 1; break;
    case WM_RBUTTONUP: button = 2; break;
    case WM_MBUTTONUP: button = 4; break;
    case WM_XBUTTONUP: button = GET_XBUTTON_WPARAM(wparam) == XBUTTON1 ? 8 : 16; break;
    }
    if (button && pressed && handled) {
        if (!state.mouse_buttons && !GetCapture()) {
            SetCapture(window);
            state.owns_mouse_capture = true;
        }
        state.mouse_buttons |= button;
    } else if (button && !pressed) {
        handled = handled || (state.mouse_buttons & button) != 0;
        state.mouse_buttons &= ~button;
        if (!state.mouse_buttons && state.owns_mouse_capture) {
            state.owns_mouse_capture = false;
            if (GetCapture() == window) {
                ReleaseCapture();
            }
        }
    }
    if (handled) {
        if (message == WM_XBUTTONDOWN || message == WM_XBUTTONUP || message == WM_XBUTTONDBLCLK) {
            return TRUE;
        }
        return 0;
    }
    return DefSubclassProc(window, message, wparam, lparam);
}

} // namespace

bool install_window_subclass(HWND window, OverlayPresent& state) {
    if (GetWindowThreadProcessId(window, nullptr) != GetCurrentThreadId()) {
        return false;
    }

    const UINT_PTR id = reinterpret_cast<UINT_PTR>(&state);
    const bool installed = SetWindowSubclass(window, window_proc, id,
                                             reinterpret_cast<DWORD_PTR>(&state)) != FALSE;

    state.window_attached.store(installed, std::memory_order_release);
    return installed;
}

bool remove_window_subclass(HWND window, OverlayPresent& state) {
    if (!state.window_attached.load(std::memory_order_acquire)) {
        return true;
    }
    if (GetWindowThreadProcessId(window, nullptr) != GetCurrentThreadId()) {
        return false;
    }

    const bool removed = RemoveWindowSubclass(window, window_proc,
                                               reinterpret_cast<UINT_PTR>(&state)) != FALSE;

    if (removed) {
        state.window_attached.store(false, std::memory_order_release);
        state.capture_mouse.store(false, std::memory_order_release);
        state.capture_keyboard.store(false, std::memory_order_release);
        state.renderer.input.request_reset();
        state.mouse_buttons = 0;
        if (state.owns_mouse_capture && GetCapture() == window) {
            ReleaseCapture();
        }
        state.owns_mouse_capture = false;
    }
    return removed;
}

bool overlay_window_callbacks_idle(const OverlayPresent& state) noexcept {
    return state.window_callbacks.load(std::memory_order_acquire) == 0;
}

} // namespace s8::overlay
