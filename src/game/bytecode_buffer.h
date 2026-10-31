#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <span>

namespace s8::game {

// Small native stubs have a known maximum size. Keep their construction on the
// stack and refuse overflow instead of repeatedly growing temporary vectors.
template<std::size_t Capacity>
class BytecodeBuffer {
public:
    constexpr void emit(std::initializer_list<unsigned char> values) {
        for (const unsigned char value : values) {
            append_byte(static_cast<std::byte>(value));
        }
    }

    constexpr void append(std::span<const std::byte> values) {
        for (const std::byte value : values) {
            append_byte(value);
        }
    }

    constexpr void address(std::uintptr_t value) {
        for (std::size_t index = 0; index < sizeof(value); ++index) {
            append_byte(static_cast<std::byte>((value >> (index * 8)) & 0xff));
        }
    }

    constexpr std::size_t branch() {
        const std::size_t position = used;

        emit({0, 0, 0, 0});
        return position;
    }

    constexpr void patch(std::size_t position) {
        if (position > used || used - position < 4 ||
            used - position - 4 > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())) {
            complete = false;
            return;
        }
        const std::uint32_t displacement = static_cast<std::uint32_t>(used - position - 4);

        for (std::size_t index = 0; index < 4; ++index) {
            storage[position + index] = static_cast<std::byte>((displacement >> (index * 8)) & 0xff);
        }
    }

    constexpr std::size_t size() const noexcept { return used; }
    constexpr bool valid() const noexcept { return complete; }
    constexpr std::span<const std::byte> bytes() const noexcept { return {storage.data(), used}; }

private:
    constexpr void append_byte(std::byte value) {
        if (used == Capacity) {
            complete = false;
            return;
        }
        storage[used++] = value;
    }

    std::array<std::byte, Capacity> storage{};
    std::size_t used = 0;
    bool complete = true;
};

} // namespace s8::game
