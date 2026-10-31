#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace s8::game {

// Locate code once while preparing the module image. Recurring/native work
// only follows this cached offset and validates the five live records.
std::optional<std::uint32_t> find_stratagem5_metadata_offset(std::span<const std::byte> image);
std::optional<std::uintptr_t> resolve_stratagem5_metadata(
    std::uint32_t pointer_offset, std::uintptr_t game_base);
std::optional<std::uintptr_t> resolve_stratagem5_metadata_from_owner(std::uintptr_t owner);
std::optional<std::uintptr_t> resolve_stratagem5_metadata(
    std::span<const std::byte> image, std::uintptr_t game_base);

} // namespace s8::game
