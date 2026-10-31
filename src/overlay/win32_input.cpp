#include "win32_input.h"

#include "../../third_party/imgui_legacy/imgui.h"
#include <windowsx.h>
#include <imm.h>
#include <cfloat>
#include <string>

#pragma comment(lib, "imm32.lib")

namespace s8::overlay {
namespace {

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
        const auto key = translate_key(wparam, lparam);
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
    const auto context = ImmGetContext(window);
    if (!context) {
        return InputMessageResult::Ignored;
    }
    const LONG bytes = ImmGetCompositionStringW(context, GCS_RESULTSTR, nullptr, 0);
    if (bytes <= 0 || bytes % static_cast<LONG>(sizeof(wchar_t)) != 0) {
        ImmReleaseContext(window, context);
        return InputMessageResult::Ignored;
    }
    std::wstring text(static_cast<std::size_t>(bytes) / sizeof(wchar_t), L'\0');
    const LONG copied = ImmGetCompositionStringW(context, GCS_RESULTSTR, text.data(), bytes);
    ImmReleaseContext(window, context);
    if (copied != bytes) {
        return InputMessageResult::Ignored;
    }
    return input.push_text(text) ? InputMessageResult::Queued : InputMessageResult::QueueFull;
}

} // namespace s8::overlay
