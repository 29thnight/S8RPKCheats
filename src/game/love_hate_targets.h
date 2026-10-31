#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace s8::game {

struct LoveHateTargets {
    std::optional<std::size_t> affinity;
    std::optional<std::size_t> relationship;
    std::optional<std::uint32_t> relationship_offset;
};

LoveHateTargets find_love_hate_targets(std::span<const std::byte> image);

} // namespace s8::game
