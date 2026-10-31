#pragma once

#include "input_events.h"
#include "../platform/windows/hook.h"

#include <array>
#include <atomic>
#include <expected>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

namespace s8::overlay {

enum class InputMessageResult { Ignored, Queued, QueueFull };

struct InputCaptureHooks {
    static constexpr std::size_t COUNT = 6;
    std::array<platform::windows::Hook, COUNT> hooks{};
    std::size_t installed = 0;
    HWND window = nullptr;
    const std::atomic<bool>* capture_mouse = nullptr;
    const std::atomic<bool>* capture_keyboard = nullptr;
    bool enabled = false;
};

// Overlay/backend queries must see real input even while the game is blocked.
class InputCaptureBypass {
public:
    InputCaptureBypass() noexcept;
    ~InputCaptureBypass();
    InputCaptureBypass(const InputCaptureBypass&) = delete;
    InputCaptureBypass& operator=(const InputCaptureBypass&) = delete;
};

std::expected<void, int> install_input_capture(InputCaptureHooks& state, HWND window,
    const std::atomic<bool>& capture_mouse, const std::atomic<bool>& capture_keyboard);
int disable_input_capture(InputCaptureHooks& state);
bool input_capture_callbacks_idle(const InputCaptureHooks& state) noexcept;
// Keeps disabled trampolines retained; the owner retains the DLL as well.
bool detach_input_capture(InputCaptureHooks& state) noexcept;

InputMessageResult queue_win32_input(InputEvents& input, UINT message, WPARAM wparam, LPARAM lparam);
InputMessageResult queue_ime_result(InputEvents& input, HWND window, LPARAM lparam);

} // namespace s8::overlay
