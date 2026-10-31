#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace s8::game {

std::optional<std::uintptr_t> resolve_stratagem5_metadata(
    std::span<const std::byte> image, std::uintptr_t game_base);

} // namespace s8::game
