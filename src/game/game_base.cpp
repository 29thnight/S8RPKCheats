#include "game_base.h"
#include "../platform/windows/process_memory.h"

#include <cstring>

namespace s8::game {

std::optional<std::uintptr_t> find_game_base_pointer(
    std::span<const std::byte> image, std::uintptr_t module_base) {
    constexpr std::size_t PATTERN_SIZE = 10;
    if (image.size() < PATTERN_SIZE)
        return std::nullopt;
    for (std::size_t i = 0; i + PATTERN_SIZE <= image.size(); ++i) {
        if (image[i] != std::byte{0x4c} || image[i + 1] != std::byte{0x8b} ||
            image[i + 2] != std::byte{0x05} || image[i + 7] != std::byte{0x41} ||
            image[i + 8] != std::byte{0x0f} || image[i + 9] != std::byte{0xb7})
            continue;
        std::int32_t displacement = 0;
        std::memcpy(&displacement, image.data() + i + 3, sizeof(displacement));
        return module_base + i + 7 + static_cast<std::intptr_t>(displacement);
    }
    return std::nullopt;
}

std::uintptr_t read_game_base(std::uintptr_t pointer_address) {
    std::uintptr_t game_base = 0;
    if (!pointer_address || !platform::windows::read_memory(
            pointer_address, std::as_writable_bytes(std::span(&game_base, 1))))
        return 0;
    return game_base;
}

std::uintptr_t read_player(std::uintptr_t game_base) {
    std::uintptr_t player = 0;
    if (game_base <= 0x10000 || !platform::windows::read_memory(
            game_base + 0xe0, std::as_writable_bytes(std::span(&player, 1))))
        return 0;
    return player;
}

} // namespace s8::game
