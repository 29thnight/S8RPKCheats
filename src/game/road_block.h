#pragma once

#include <array>
#include <cstdint>

namespace s8::game {

struct RoadBlock {
    std::array<std::uint64_t, 2> original{};
    bool applied = false;
    std::uint64_t last_tick_ms = 0;
};

bool set_road_block(RoadBlock& state, std::uintptr_t module_base, bool second, bool enabled);
void tick_road_block(RoadBlock& state, std::uintptr_t module_base, bool second, std::uint64_t now_ms);

} // namespace s8::game
