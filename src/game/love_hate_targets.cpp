#include "love_hate_targets.h"
#include "find_bytes.h"

#include <array>
#include <cstring>

namespace s8::game {

LoveHateTargets find_love_hate_targets(std::span<const std::byte> image) {
    constexpr std::array AFFINITY{std::byte{0x0f}, std::byte{0xb6}, std::byte{0x84}, std::byte{0x18},
                                   std::byte{0xf0}, std::byte{0}, std::byte{0}, std::byte{0}};
    LoveHateTargets result;
    result.affinity = find_bytes(image, AFFINITY, 0x3000000);
    // 41 80 ?? ?? ?? ?? ?? ?? ?? 75 ?? 48 8B ?? ?? ?? E8
    for (std::size_t i = 0; i + 17 <= image.size(); ++i) {
        if (image[i] == std::byte{0x41} && image[i + 1] == std::byte{0x80} &&
            image[i + 9] == std::byte{0x75} && image[i + 11] == std::byte{0x48} &&
            image[i + 12] == std::byte{0x8b} && image[i + 16] == std::byte{0xe8}) {
            result.relationship = i;
            std::uint32_t offset = 0;
            std::memcpy(&offset, image.data() + i + 4, sizeof(offset));
            result.relationship_offset = offset;
            break;
        }
    }
    return result;
}

} // namespace s8::game
