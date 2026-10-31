#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace s8::game {

struct LoveHate {
    std::uintptr_t affinity_address = 0;
    std::uintptr_t relationship_address = 0;
    std::uintptr_t affinity_cave = 0;
    std::uintptr_t relationship_cave = 0;
    std::uintptr_t captured_affinity = 0;
    std::array<std::byte, 8> affinity_original{};
    std::array<std::byte, 11> relationship_original{};
    std::uint32_t relationship_offset = 0;
    bool affinity_applied = false;
    bool relationship_applied = false;
};

bool set_love_hate(LoveHate& state, std::uintptr_t module_base, std::span<const std::byte> image, bool enabled);

} // namespace s8::game
