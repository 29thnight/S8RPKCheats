#include "win32_input.h"

#include "../../third_party/imgui_legacy/imgui.h"
#include "../../third_party/minhook/include/MinHook.h"
#include <windowsx.h>
#include <imm.h>
#include <cfloat>
#include <array>
#include <cstdint>
#include <string_view>

#pragma comment(lib, "imm32.lib")

namespace s8::overlay {
namespace {

constexpr std::uint64_t CAPTURE_CLOSED = std::uint64_t{1} << 63;
constexpr std::uint64_t CAPTURE_COUNT_MASK = CAPTURE_CLOSED - 1;
constexpr std::size_t CURSOR_INDEX = 0;
constexpr std::size_t ASYNC_KEY_INDEX = 1;
constexpr std::size_t KEY_INDEX = 2;
constexpr std::size_t KEYBOARD_INDEX = 3;
constexpr std::size_t PEEK_W_INDEX = 4;
constexpr std::size_t PEEK_A_INDEX = 5;

std::atomic<InputCaptureHooks*> active_capture{nullptr};
std::atomic<std::uint64_t> capture_gate{CAPTURE_CLOSED};
std::array<std::atomic<void*>, InputCaptureHooks::COUNT> input_originals{};
thread_local unsigned capture_bypass_depth = 0;

struct CapturePolicy {
    HWND window = nullptr;
    bool mouse = false;
    bool keyboard = false;
};

CapturePolicy read_capture_policy() noexcept {
    if (capture_bypass_depth) {
        return {};
    }

    std::uint64_t observed = capture_gate.load(std::memory_order_acquire);

    while (!(observed & CAPTURE_CLOSED)) {
        if (capture_gate.compare_exchange_weak(observed, observed + 1,
                std::memory_order_acq_rel, std::memory_order_acquire)) {
            const InputCaptureHooks* const state = active_capture.load(std::memory_order_acquire);
            const CapturePolicy policy{
                state->window,
                state->capture_mouse->load(std::memory_order_acquire),
                state->capture_keyboard->load(std::memory_order_acquire),
            };

            capture_gate.fetch_sub(1, std::memory_order_release);
            return policy;
        }
    }
    return {};
}

constexpr bool is_mouse_key(int key) noexcept {
    return key == VK_LBUTTON || key == VK_RBUTTON || key == VK_MBUTTON ||
           key == VK_XBUTTON1 || key == VK_XBUTTON2;
}

constexpr bool is_system_modifier(int key) noexcept {
    return key == VK_SHIFT || key == VK_LSHIFT || key == VK_RSHIFT ||
           key == VK_CONTROL || key == VK_LCONTROL || key == VK_RCONTROL ||
           key == VK_MENU || key == VK_LMENU || key == VK_RMENU ||
           key == VK_LWIN || key == VK_RWIN || key == VK_OEM_3 ||
           key == VK_HANGUL;
}

bool blocked_key(const CapturePolicy& policy, int key) noexcept {
    return is_mouse_key(key) ? policy.mouse : policy.keyboard && !is_system_modifier(key);
}

BOOL WINAPI capture_cursor(LPPOINT point) {
    using Function = BOOL (WINAPI*)(LPPOINT);
    const Function original = reinterpret_cast<Function>(input_originals[CURSOR_INDEX].load(std::memory_order_acquire));
    const BOOL result = original(point);
    const CapturePolicy policy = read_capture_policy();

    if (result && point && policy.mouse) {
        point->x = -1;
        point->y = -1;
    }
    return result;
}

SHORT WINAPI capture_async_key(int key) {
    using Function = SHORT (WINAPI*)(int);
    const Function original = reinterpret_cast<Function>(input_originals[ASYNC_KEY_INDEX].load(std::memory_order_acquire));
    const SHORT result = original(key);
    const CapturePolicy policy = read_capture_policy();

    return blocked_key(policy, key) ? 0 : result;
}

SHORT WINAPI capture_key(int key) {
    using Function = SHORT (WINAPI*)(int);
    const Function original = reinterpret_cast<Function>(input_originals[KEY_INDEX].load(std::memory_order_acquire));
    const SHORT result = original(key);
    const CapturePolicy policy = read_capture_policy();

    return blocked_key(policy, key) ? 0 : result;
}

BOOL WINAPI capture_keyboard_state(PBYTE keys) {
    using Function = BOOL (WINAPI*)(PBYTE);
    const Function original = reinterpret_cast<Function>(input_originals[KEYBOARD_INDEX].load(std::memory_order_acquire));
    const BOOL result = original(keys);
    const CapturePolicy policy = read_capture_policy();

    if (result && keys && (policy.mouse || policy.keyboard)) {
        constexpr int KEY_COUNT = 256;

        for (int key = 0; key < KEY_COUNT; ++key) {
            if (blocked_key(policy, key)) {
                keys[key] = 0;
            }
        }
    }
    return result;
}

void filter_message(MSG& message, UINT removal) noexcept {
    const CapturePolicy policy = read_capture_policy();

    if (!(removal & PM_REMOVE) || message.hwnd != policy.window ||
        (!policy.mouse && !policy.keyboard)) {
        return;
    }
    if (message.message == WM_INPUT) {
        RAWINPUTHEADER header{};
        UINT bytes = sizeof(header);
        const UINT result = GetRawInputData(reinterpret_cast<HRAWINPUT>(message.lParam),
                                             RID_HEADER, &header, &bytes, sizeof(header));

        if (result == sizeof(header) &&
            ((header.dwType == RIM_TYPEMOUSE && policy.mouse) ||
             (header.dwType == RIM_TYPEKEYBOARD && policy.keyboard))) {
            if (GET_RAWINPUT_CODE_WPARAM(message.wParam) == RIM_INPUT) {
                if (IsWindowUnicode(message.hwnd)) {
                    DefWindowProcW(message.hwnd, WM_INPUT, message.wParam, message.lParam);
                } else {
                    DefWindowProcA(message.hwnd, WM_INPUT, message.wParam, message.lParam);
                }
            }
            message.message = WM_NULL;
        }
    }
}

BOOL WINAPI capture_peek_w(LPMSG message, HWND window, UINT first, UINT last, UINT removal) {
    using Function = BOOL (WINAPI*)(LPMSG, HWND, UINT, UINT, UINT);
    const Function original = reinterpret_cast<Function>(input_originals[PEEK_W_INDEX].load(std::memory_order_acquire));
    const BOOL result = original(message, window, first, last, removal);

    if (result && message) {
        filter_message(*message, removal);
    }
    return result;
}

BOOL WINAPI capture_peek_a(LPMSG message, HWND window, UINT first, UINT last, UINT removal) {
    using Function = BOOL (WINAPI*)(LPMSG, HWND, UINT, UINT, UINT);
    const Function original = reinterpret_cast<Function>(input_originals[PEEK_A_INDEX].load(std::memory_order_acquire));
    const BOOL result = original(message, window, first, last, removal);

    if (result && message) {
        filter_message(*message, removal);
    }
    return result;
}

void discard_input_hooks(InputCaptureHooks& state) {
    while (state.installed) {
        --state.installed;
        platform::windows::remove_hook(state.hooks[state.installed]);
        state.hooks[state.installed] = {};
    }
}

ImGuiKey translate_key(WPARAM key, LPARAM lparam) {
    if (key >= '0' && key <= '9') {
        return static_cast<ImGuiKey>(ImGuiKey_0 + key - '0');
    }
    if (key >= 'A' && key <= 'Z') {
        return static_cast<ImGuiKey>(ImGuiKey_A + key - 'A');
    }
    if (key >= VK_F1 && key <= VK_F24) {
        return static_cast<ImGuiKey>(ImGuiKey_F1 + key - VK_F1);
    }
    if (key >= VK_NUMPAD0 && key <= VK_NUMPAD9) {
        return static_cast<ImGuiKey>(ImGuiKey_Keypad0 + key - VK_NUMPAD0);
    }
    switch (key) {
    case VK_TAB: return ImGuiKey_Tab;
    case VK_LEFT: return ImGuiKey_LeftArrow;
    case VK_RIGHT: return ImGuiKey_RightArrow;
    case VK_UP: return ImGuiKey_UpArrow;
    case VK_DOWN: return ImGuiKey_DownArrow;
    case VK_PRIOR: return ImGuiKey_PageUp;
    case VK_NEXT: return ImGuiKey_PageDown;
    case VK_HOME: return ImGuiKey_Home;
    case VK_END: return ImGuiKey_End;
    case VK_INSERT: return ImGuiKey_Insert;
    case VK_DELETE: return ImGuiKey_Delete;
    case VK_BACK: return ImGuiKey_Backspace;
    case VK_SPACE: return ImGuiKey_Space;
    case VK_RETURN: return (HIWORD(lparam) & KF_EXTENDED) ? ImGuiKey_KeypadEnter : ImGuiKey_Enter;
    case VK_ESCAPE: return ImGuiKey_Escape;
    case VK_OEM_7: return ImGuiKey_Apostrophe;
    case VK_OEM_COMMA: return ImGuiKey_Comma;
    case VK_OEM_MINUS: return ImGuiKey_Minus;
    case VK_OEM_PERIOD: return ImGuiKey_Period;
    case VK_OEM_2: return ImGuiKey_Slash;
    case VK_OEM_1: return ImGuiKey_Semicolon;
    case VK_OEM_PLUS: return ImGuiKey_Equal;
    case VK_OEM_4: return ImGuiKey_LeftBracket;
    case VK_OEM_5: return ImGuiKey_Backslash;
    case VK_OEM_6: return ImGuiKey_RightBracket;
    case VK_OEM_3: return ImGuiKey_GraveAccent;
    case VK_CAPITAL: return ImGuiKey_CapsLock;
    case VK_SCROLL: return ImGuiKey_ScrollLock;
    case VK_NUMLOCK: return ImGuiKey_NumLock;
    case VK_PAUSE: return ImGuiKey_Pause;
    case VK_DECIMAL: return ImGuiKey_KeypadDecimal;
    case VK_DIVIDE: return ImGuiKey_KeypadDivide;
    case VK_MULTIPLY: return ImGuiKey_KeypadMultiply;
    case VK_SUBTRACT: return ImGuiKey_KeypadSubtract;
    case VK_ADD: return ImGuiKey_KeypadAdd;
    case VK_LWIN: return ImGuiKey_LeftSuper;
    case VK_RWIN: return ImGuiKey_RightSuper;
    case VK_APPS: return ImGuiKey_Menu;
    case VK_CONTROL: return (HIWORD(lparam) & KF_EXTENDED) ? ImGuiKey_RightCtrl : ImGuiKey_LeftCtrl;
    case VK_SHIFT: return MapVirtualKeyW((lparam >> 16) & 0xff, MAPVK_VSC_TO_VK_EX) == VK_RSHIFT
        ? ImGuiKey_RightShift : ImGuiKey_LeftShift;
    case VK_MENU: return (HIWORD(lparam) & KF_EXTENDED) ? ImGuiKey_RightAlt : ImGuiKey_LeftAlt;
    case VK_LCONTROL: return ImGuiKey_LeftCtrl;
    case VK_RCONTROL: return ImGuiKey_RightCtrl;
    case VK_LSHIFT: return ImGuiKey_LeftShift;
    case VK_RSHIFT: return ImGuiKey_RightShift;
    case VK_LMENU: return ImGuiKey_LeftAlt;
    case VK_RMENU: return ImGuiKey_RightAlt;
    default: return ImGuiKey_None;
    }
}

} // namespace

InputCaptureBypass::InputCaptureBypass() noexcept {
    ++capture_bypass_depth;
}

InputCaptureBypass::~InputCaptureBypass() {
    --capture_bypass_depth;
}

std::expected<void, int> install_input_capture(InputCaptureHooks& state, HWND window,
    const std::atomic<bool>& capture_mouse, const std::atomic<bool>& capture_keyboard) {
    if (!window || state.installed || active_capture.load(std::memory_order_acquire)) {
        return std::unexpected(static_cast<int>(ERROR_INVALID_PARAMETER));
    }

    const HMODULE user32 = GetModuleHandleW(L"user32.dll");
    constexpr std::array<const char*, InputCaptureHooks::COUNT> NAMES{
        "GetCursorPos", "GetAsyncKeyState", "GetKeyState",
        "GetKeyboardState", "PeekMessageW", "PeekMessageA",
    };

    // Check callback types against Win32 declarations before MinHook type erasure.
    constexpr decltype(&::GetCursorPos) CURSOR_DETOUR = &capture_cursor;
    constexpr decltype(&::GetAsyncKeyState) ASYNC_KEY_DETOUR = &capture_async_key;
    constexpr decltype(&::GetKeyState) KEY_DETOUR = &capture_key;
    constexpr decltype(&::GetKeyboardState) KEYBOARD_DETOUR = &capture_keyboard_state;
    constexpr decltype(&::PeekMessageW) PEEK_W_DETOUR = &capture_peek_w;
    constexpr decltype(&::PeekMessageA) PEEK_A_DETOUR = &capture_peek_a;

    const std::array<void*, InputCaptureHooks::COUNT> detours{
        reinterpret_cast<void*>(CURSOR_DETOUR), reinterpret_cast<void*>(ASYNC_KEY_DETOUR),
        reinterpret_cast<void*>(KEY_DETOUR), reinterpret_cast<void*>(KEYBOARD_DETOUR),
        reinterpret_cast<void*>(PEEK_W_DETOUR), reinterpret_cast<void*>(PEEK_A_DETOUR),
    };

    if (!user32) {
        return std::unexpected(static_cast<int>(ERROR_MOD_NOT_FOUND));
    }
    for (std::size_t index = 0; index < NAMES.size(); ++index) {
        void* const target = reinterpret_cast<void*>(GetProcAddress(user32, NAMES[index]));

        if (!target) {
            discard_input_hooks(state);
            return std::unexpected(static_cast<int>(ERROR_PROC_NOT_FOUND));
        }

        const std::expected<platform::windows::Hook, int> hook =
            platform::windows::create_hook(target, detours[index]);

        if (!hook) {
            discard_input_hooks(state);
            return std::unexpected(hook.error());
        }
        state.hooks[index] = *hook;
        ++state.installed;
    }

    state.window = window;
    state.capture_mouse = &capture_mouse;
    state.capture_keyboard = &capture_keyboard;

    InputCaptureHooks* expected = nullptr;

    if (!active_capture.compare_exchange_strong(expected, &state, std::memory_order_acq_rel)) {
        discard_input_hooks(state);
        return std::unexpected(static_cast<int>(ERROR_ALREADY_INITIALIZED));
    }
    for (std::size_t index = 0; index < state.installed; ++index) {
        input_originals[index].store(state.hooks[index].original, std::memory_order_release);
    }
    capture_gate.store(0, std::memory_order_release);

    MH_STATUS status = MH_OK;

    for (std::size_t index = 0; index < state.installed && status == MH_OK; ++index) {
        status = MH_QueueEnableHook(state.hooks[index].target);
    }
    if (status == MH_OK) {
        status = MH_ApplyQueued();
    }
    if (status != MH_OK) {
        disable_input_capture(state);
        return std::unexpected(static_cast<int>(status));
    }

    state.enabled = true;
    return {};
}

int disable_input_capture(InputCaptureHooks& state) {
    if (active_capture.load(std::memory_order_acquire) == &state) {
        capture_gate.fetch_or(CAPTURE_CLOSED, std::memory_order_acq_rel);
    }

    int first_error = MH_OK;

    for (std::size_t index = 0; index < state.installed; ++index) {
        const int status = platform::windows::disable_hook(state.hooks[index]);
        const MH_STATUS queued = MH_QueueDisableHook(state.hooks[index].target);

        if (first_error == MH_OK && status != MH_OK && status != MH_ERROR_DISABLED) {
            first_error = status;
        }
        if (first_error == MH_OK && queued != MH_OK) {
            first_error = static_cast<int>(queued);
        }
    }
    if (first_error == MH_OK) {
        state.enabled = false;
    }
    return first_error;
}

bool input_capture_callbacks_idle(const InputCaptureHooks&) noexcept {
    return (capture_gate.load(std::memory_order_acquire) & CAPTURE_COUNT_MASK) == 0;
}

bool detach_input_capture(InputCaptureHooks& state) noexcept {
    const std::uint64_t gate = capture_gate.load(std::memory_order_acquire);

    if (!(gate & CAPTURE_CLOSED) || (gate & CAPTURE_COUNT_MASK)) {
        return false;
    }

    InputCaptureHooks* expected = &state;

    if (!active_capture.compare_exchange_strong(expected, nullptr, std::memory_order_acq_rel) &&
        expected != nullptr) {
        return false;
    }
    state.capture_mouse = nullptr;
    state.capture_keyboard = nullptr;
    state.window = nullptr;
    return true;
}

InputMessageResult queue_win32_input(InputEvents& input, UINT message, WPARAM wparam, LPARAM lparam) {
    InputEvent event{};

    switch (message) {
    case WM_MOUSEMOVE:
        event.kind = InputKind::MousePosition;
        event.x = static_cast<float>(GET_X_LPARAM(lparam));
        event.y = static_cast<float>(GET_Y_LPARAM(lparam));
        break;
    case WM_MOUSELEAVE:
        event.kind = InputKind::MousePosition;
        event.x = -FLT_MAX;
        event.y = -FLT_MAX;
        break;
    case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_LBUTTONDBLCLK:
    case WM_RBUTTONDOWN: case WM_RBUTTONUP: case WM_RBUTTONDBLCLK:
    case WM_MBUTTONDOWN: case WM_MBUTTONUP: case WM_MBUTTONDBLCLK:
        event.kind = InputKind::MouseButton;
        event.value = message == WM_LBUTTONDOWN || message == WM_LBUTTONUP || message == WM_LBUTTONDBLCLK ? 0 :
                      message == WM_RBUTTONDOWN || message == WM_RBUTTONUP || message == WM_RBUTTONDBLCLK ? 1 : 2;
        event.down = message != WM_LBUTTONUP && message != WM_RBUTTONUP && message != WM_MBUTTONUP;
        break;
    case WM_XBUTTONDOWN: case WM_XBUTTONUP: case WM_XBUTTONDBLCLK:
        event.kind = InputKind::MouseButton;
        event.value = GET_XBUTTON_WPARAM(wparam) == XBUTTON1 ? 3 : 4;
        event.down = message != WM_XBUTTONUP;
        break;
    case WM_MOUSEWHEEL: case WM_MOUSEHWHEEL:
        event.kind = InputKind::MouseWheel;
        if (message == WM_MOUSEWHEEL) {
            event.y = static_cast<float>(GET_WHEEL_DELTA_WPARAM(wparam)) / WHEEL_DELTA;
        } else {
            event.x = -static_cast<float>(GET_WHEEL_DELTA_WPARAM(wparam)) / WHEEL_DELTA;
        }
        break;
    case WM_CHAR:
        if (wparam == 0 || wparam > 0xffff) {
            return InputMessageResult::Ignored;
        }
        event.kind = InputKind::Character;
        event.value = static_cast<std::uint32_t>(wparam);
        break;
    case WM_SETFOCUS: case WM_KILLFOCUS:
        event.kind = InputKind::Focus;
        event.down = message == WM_SETFOCUS;
        break;
    case WM_KEYDOWN: case WM_KEYUP: case WM_SYSKEYDOWN: case WM_SYSKEYUP: {
        const ImGuiKey key = translate_key(wparam, lparam);

        if (key == ImGuiKey_None) {
            return InputMessageResult::Ignored;
        }
        event.kind = InputKind::Key;
        event.value = static_cast<std::uint32_t>(key);
        event.down = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
        event.modifiers = ((GetKeyState(VK_CONTROL) & 0x8000) ? 1u : 0u) |
                          ((GetKeyState(VK_SHIFT) & 0x8000) ? 2u : 0u) |
                          ((GetKeyState(VK_MENU) & 0x8000) ? 4u : 0u) |
                          (((GetKeyState(VK_LWIN) | GetKeyState(VK_RWIN)) & 0x8000) ? 8u : 0u);
        break;
    }
    default:
        return InputMessageResult::Ignored;
    }
    return input.push(event) ? InputMessageResult::Queued : InputMessageResult::QueueFull;
}

InputMessageResult queue_ime_result(InputEvents& input, HWND window, LPARAM lparam) {
    if (!(lparam & GCS_RESULTSTR)) {
        return InputMessageResult::Ignored;
    }
    const HIMC context = ImmGetContext(window);

    if (!context) {
        return InputMessageResult::Ignored;
    }
    const LONG bytes = ImmGetCompositionStringW(context, GCS_RESULTSTR, nullptr, 0);

    if (bytes <= 0 || bytes % static_cast<LONG>(sizeof(wchar_t)) != 0) {
        ImmReleaseContext(window, context);
        return InputMessageResult::Ignored;
    }
    std::array<wchar_t, InputEvents::CAPACITY> text;
    const std::size_t length = static_cast<std::size_t>(bytes) / sizeof(wchar_t);

    if (length > text.size()) {
        ImmReleaseContext(window, context);
        input.request_reset();
        return InputMessageResult::QueueFull;
    }

    const LONG copied = ImmGetCompositionStringW(context, GCS_RESULTSTR, text.data(), bytes);

    ImmReleaseContext(window, context);
    if (copied != bytes) {
        return InputMessageResult::Ignored;
    }
    return input.push_text(std::wstring_view(text.data(), length)) ? InputMessageResult::Queued : InputMessageResult::QueueFull;
}

} // namespace s8::overlay
