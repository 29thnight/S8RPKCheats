#include "stratagem5_metadata.h"
#include "../platform/windows/process_memory.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>

namespace s8::game {
namespace {

constexpr std::array PATTERN{
    0x48, 0x8b, -1, -1, -1, -1, -1, -1, 0xe8, -1, -1, -1, -1,
    0x48, 0x8b, -1, 0x48, 0x83, -1, -1, 0x5b, 0xc3, 0x33, -1,
    0x48, 0x8b, -1, 0xe8, -1, -1, -1, -1, 0x48, 0x8b, -1,
    0x48, 0x83, -1, -1, 0x5b, 0xc3, 0xe8, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, 0x40, 0x53,
};

std::optional<std::size_t> find_pattern(std::span<const std::byte> image) {
    if (image.size() < PATTERN.size())
        return std::nullopt;
    for (std::size_t i = 0; i + PATTERN.size() <= image.size(); ++i) {
        bool matches = true;
        for (std::size_t j = 0; j < PATTERN.size(); ++j) {
            if (PATTERN[j] >= 0 && std::to_integer<int>(image[i + j]) != PATTERN[j]) {
                matches = false;
                break;
            }
        }
        if (matches)
            return i;
    }
    return std::nullopt;
}

} // namespace

std::optional<std::uintptr_t> resolve_stratagem5_metadata(
    std::span<const std::byte> image, std::uintptr_t game_base) {
    if (!game_base)
        return std::nullopt;
    const std::size_t start = image.size() > 0x58e000 ? 0x58e000 : 0;
    const std::size_t end = std::min<std::size_t>(image.size(), 0x59e000);
    auto found = find_pattern(image.subspan(start, end - start));
    if (found)
        *found += start;
    else
        found = find_pattern(image);
    if (!found)
        return std::nullopt;

    std::uint32_t pointer_offset = 0;
    std::memcpy(&pointer_offset, image.data() + *found + 4, sizeof(pointer_offset));
    std::uintptr_t troop_base = 0;
    if (!platform::windows::read_memory(game_base + pointer_offset,
                                         std::as_writable_bytes(std::span(&troop_base, 1))) ||
        !troop_base || troop_base == std::numeric_limits<std::uintptr_t>::max())
        return std::nullopt;

    const auto table = troop_base + 0x9d30;
    std::array<std::byte, 5 * 0x20> rows{};
    if (!platform::windows::read_memory(table, rows))
        return std::nullopt;
    for (std::size_t i = 0; i < 5; ++i) {
        const auto expected = static_cast<std::byte>(i + 1);
        if (rows[i * 0x20 + 0x08] != expected || rows[i * 0x20 + 0x0a] != expected ||
            rows[i * 0x20 + 0x0c] != expected)
            return std::nullopt;
    }
    return table;
}

} // namespace s8::game
