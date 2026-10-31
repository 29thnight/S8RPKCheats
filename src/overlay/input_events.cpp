#include "input_events.h"

#include "../../third_party/imgui_legacy/imgui.h"

namespace s8::overlay {

bool InputEvents::push(InputEvent event) {
    if ((event.kind == InputKind::MouseButton && event.value >= 5) ||
        (event.kind == InputKind::Character && event.value > 0xffff) ||
        reset_pending.load(std::memory_order_acquire)) {
        return false;
    }

    const std::uint64_t write = write_position.load(std::memory_order_relaxed);
    const std::uint64_t read = read_position.load(std::memory_order_acquire);

    if (write - read == CAPACITY) {
        request_reset();
        return false;
    }

    events[write % CAPACITY] = event;
    write_position.store(write + 1, std::memory_order_release);
    return true;
}

bool InputEvents::push_text(std::wstring_view text) {
    if (reset_pending.load(std::memory_order_acquire)) {
        return false;
    }

    const std::uint64_t write = write_position.load(std::memory_order_relaxed);
    const std::uint64_t read = read_position.load(std::memory_order_acquire);

    if (text.size() > CAPACITY - (write - read)) {
        request_reset();
        return false;
    }

    std::uint64_t position = write;

    for (const wchar_t character : text) {
        events[position % CAPACITY] = InputEvent{
            .kind = InputKind::Character,
            .value = static_cast<std::uint16_t>(character),
        };
        ++position;
    }
    write_position.store(position, std::memory_order_release);
    return true;
}

void InputEvents::request_reset() noexcept {
    reset_pending.store(true, std::memory_order_release);
}

void InputEvents::deliver() {
    ImGuiIO& io = ImGui::GetIO();

    if (reset_pending.load(std::memory_order_acquire)) {
        // The producer stays paused until the old queue has been discarded.
        const std::uint64_t end = write_position.load(std::memory_order_acquire);

        read_position.store(end, std::memory_order_release);
        io.ClearEventsQueue();
        io.ClearInputKeys();
        io.ClearInputMouse();
        reset_pending.store(false, std::memory_order_release);
        return;
    }

    std::uint64_t position = read_position.load(std::memory_order_relaxed);
    const std::uint64_t end = write_position.load(std::memory_order_acquire);

    while (position != end) {
        const InputEvent event = events[position % CAPACITY];

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
        ++position;
        read_position.store(position, std::memory_order_release);
    }
}

} // namespace s8::overlay
