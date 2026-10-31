#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace s8::game {

// The input is a snapshot of the executable image beginning at module_base.
std::optional<std::uintptr_t> find_game_base_pointer(
    std::span<const std::byte> image, std::uintptr_t module_base);
std::uintptr_t read_game_base(std::uintptr_t pointer_address);
std::uintptr_t read_player(std::uintptr_t game_base);

} // namespace s8::game
