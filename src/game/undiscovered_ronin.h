#pragma once

#include <cstdint>
#include <optional>

namespace s8::game {

struct UndiscoveredRonin {
    std::uintptr_t last_game_base = 0;
    std::uintptr_t last_player = 0;
    std::uint64_t last_tick_ms = 0;
    bool enabled = false;
    bool pending = false;
    std::optional<int> completed_modified;
};

void update_undiscovered_ronin(UndiscoveredRonin& state, std::uintptr_t module_base,
                               std::uintptr_t game_base, std::uintptr_t player,
                               std::uint64_t now_ms, bool enabled);

} // namespace s8::game
