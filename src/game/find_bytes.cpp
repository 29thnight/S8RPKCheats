#include "find_bytes.h"

#include <algorithm>
#include <array>
#include <cstring>

namespace s8::game {

std::optional<std::size_t> find_bytes(std::span<const std::byte> image,
                                      std::span<const std::byte> pattern, std::size_t limit) {
    const std::size_t size = std::min(image.size(), limit);

    if (pattern.empty()) {
        return 0;
    }
    if (size < pattern.size()) {
        return std::nullopt;
    }
    std::array<std::size_t, 256> skip;
    const std::size_t tail = pattern.size() - 1;
    const std::size_t last = size - pattern.size();

    skip.fill(pattern.size());
    for (std::size_t index = 0; index < tail; ++index) {
        skip[std::to_integer<unsigned char>(pattern[index])] = tail - index;
    }
    for (std::size_t offset = 0; offset <= last;) {
        const std::byte candidate = image[offset + tail];

        if (candidate == pattern[tail] && std::memcmp(image.data() + offset, pattern.data(), tail) == 0) {
            return offset;
        }
        offset += skip[std::to_integer<unsigned char>(candidate)];
    }
    return std::nullopt;
}

} // namespace s8::game
