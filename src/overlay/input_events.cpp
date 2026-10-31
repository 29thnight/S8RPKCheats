#include "input_events.h"

#include "../../third_party/imgui_legacy/imgui.h"

namespace s8::overlay {

bool InputEvents::push(InputEvent event) {
    if ((event.kind == InputKind::MouseButton && event.value >= 5) ||
        (event.kind == InputKind::Character && event.value > 0xffff)) {
        return false;
    }
    std::lock_guard lock(mutex);
    if (count == CAPACITY) {
        return false;
    }
    events[count++] = event;
    return true;
}

bool InputEvents::push_text(std::wstring_view text) {
    std::lock_guard lock(mutex);
    if (text.size() > CAPACITY - count) {
        return false;
    }
    for (const auto character : text) {
        InputEvent event{};
        event.kind = InputKind::Character;
        event.value = static_cast<std::uint16_t>(character);
        events[count++] = event;
    }
    return true;
}

void InputEvents::deliver() {
    std::array<InputEvent, CAPACITY> pending{};
    std::size_t size = 0;
    {
        std::lock_guard lock(mutex);
        size = count;
        for (std::size_t i = 0; i < size; ++i) {
            pending[i] = events[i];
        }
        count = 0;
    }
    auto& io = ImGui::GetIO();
    for (std::size_t i = 0; i < size; ++i) {
        const auto& event = pending[i];
        switch (event.kind) {
        case InputKind::MousePosition:
            io.AddMousePosEvent(event.x, event.y);
            break;
        case InputKind::MouseButton:
            io.AddMouseButtonEvent(static_cast<int>(event.value), event.down);
            break;
        case InputKind::MouseWheel:
            io.AddMouseWheelEvent(event.x, event.y);
            break;
        case InputKind::Character:
            io.AddInputCharacterUTF16(static_cast<unsigned short>(event.value));
            break;
        case InputKind::Focus:
            io.AddFocusEvent(event.down);
            break;
        case InputKind::Key:
            io.AddKeyEvent(ImGuiMod_Ctrl, (event.modifiers & 1) != 0);
            io.AddKeyEvent(ImGuiMod_Shift, (event.modifiers & 2) != 0);
            io.AddKeyEvent(ImGuiMod_Alt, (event.modifiers & 4) != 0);
            io.AddKeyEvent(ImGuiMod_Super, (event.modifiers & 8) != 0);
            io.AddKeyEvent(static_cast<ImGuiKey>(event.value), event.down);
            break;
        }
    }
}

} // namespace s8::overlay
