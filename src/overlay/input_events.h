#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
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
    static constexpr std::size_t CAPACITY = 256;

    // The window thread is the only producer; serialized Present is the consumer.
    bool push(InputEvent event);
    bool push_text(std::wstring_view text);
    void request_reset() noexcept;
    void deliver();

private:
    std::array<InputEvent, CAPACITY> events{};
    alignas(64) std::atomic<std::uint64_t> read_position{0};
    alignas(64) std::atomic<std::uint64_t> write_position{0};
    std::atomic<bool> reset_pending{false};
};

} // namespace s8::overlay
