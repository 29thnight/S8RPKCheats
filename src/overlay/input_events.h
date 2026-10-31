#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string_view>

namespace s8::overlay {

enum class InputKind { MousePosition, MouseButton, MouseWheel, Character, Focus, Key };

struct InputEvent {
    InputKind kind;
    float x = 0;
    float y = 0;
    std::uint32_t value = 0;
    std::uint32_t modifiers = 0;
    bool down = false;
};

class InputEvents {
public:
    bool push(InputEvent event);
    bool push_text(std::wstring_view text);
    void deliver();

private:
    static constexpr std::size_t CAPACITY = 256;
    std::mutex mutex;
    std::array<InputEvent, CAPACITY> events{};
    std::size_t count = 0;
};

} // namespace s8::overlay
