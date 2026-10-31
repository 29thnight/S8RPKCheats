#include "debug_tools.h"

#include <charconv>
#include <cstring>
#include <limits>
#include <system_error>

namespace s8::domain {

bool parse_memory_value(MemoryValueType type, std::string_view text, MemoryValue& output) {
    MemoryValue value{{}, type};

    if (text.empty()) {
        return false;
    }
    if (type == MemoryValueType::Float) {
        float number = 0;
        const std::from_chars_result parsed = std::from_chars(text.data(), text.data() + text.size(), number);

        if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) {
            return false;
        }
        std::memcpy(value.bytes.data(), &number, sizeof(number));
    } else if (type == MemoryValueType::Double) {
        double number = 0;
        const std::from_chars_result parsed = std::from_chars(text.data(), text.data() + text.size(), number);

        if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) {
            return false;
        }
        std::memcpy(value.bytes.data(), &number, sizeof(number));
    } else {
        const std::size_t size = memory_value_size(type);
        std::uint64_t number = 0;

        if (!size) {
            return false;
        }
        if (text.front() == '-') {
            std::int64_t signed_number = 0;
            const std::from_chars_result parsed =
                std::from_chars(text.data(), text.data() + text.size(), signed_number);
            const std::int64_t minimum = size == 8 ? std::numeric_limits<std::int64_t>::min()
                : -(std::int64_t{1} << (size * 8 - 1));

            if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || signed_number < minimum) {
                return false;
            }
            number = static_cast<std::uint64_t>(signed_number);
        } else {
            const bool hexadecimal = text.starts_with("0x") || text.starts_with("0X");

            if (hexadecimal) {
                text.remove_prefix(2);
            }
            const std::from_chars_result parsed =
                std::from_chars(text.data(), text.data() + text.size(), number, hexadecimal ? 16 : 10);
            const std::uint64_t maximum = size == 8 ? std::numeric_limits<std::uint64_t>::max()
                : (std::uint64_t{1} << (size * 8)) - 1;

            if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || number > maximum) {
                return false;
            }
        }
        std::memcpy(value.bytes.data(), &number, size);
    }
    output = value;
    return true;
}

} // namespace s8::domain
