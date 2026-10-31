#pragma once

#include "../domain/ids.h"

#include <array>
#include <cstdint>

namespace s8::game {

struct RoadBlock {
    std::array<std::uint64_t, 2> original{};
    std::uintptr_t root = 0;
    domain::GameEpoch epoch{};
    bool requested = false;
    bool applied = false;
    std::uint64_t last_tick_ms = 0;
};

bool set_road_block(RoadBlock& state, std::uintptr_t module_base, bool second, bool enabled,
                    domain::GameEpoch epoch);
void tick_road_block(RoadBlock& state, std::uintptr_t module_base, bool second, std::uint64_t now_ms,
                     domain::GameEpoch epoch);

} // namespace s8::game
