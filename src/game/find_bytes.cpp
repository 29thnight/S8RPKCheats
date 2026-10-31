#include "find_bytes.h"

#include <algorithm>

namespace s8::game {

std::optional<std::size_t> find_bytes(std::span<const std::byte> image,
                                      std::span<const std::byte> pattern, std::size_t limit) {
    const auto size = std::min(image.size(), limit);
    if (size < pattern.size())
        return std::nullopt;
    for (std::size_t i = 0; i + pattern.size() <= size; ++i)
        if (std::equal(pattern.begin(), pattern.end(), image.begin() + i))
            return i;
    return std::nullopt;
}

} // namespace s8::game
