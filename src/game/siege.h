#pragma once

#include "battle_observation.h"
#include <cstdint>

namespace s8::game {

struct Siege {
    bool move_applied = false;
    bool terrain_applied = false;
    int previous_day = -1;
    std::uint64_t last_attempt_ms = 0;
};

void disable_siege(Siege& state, std::uintptr_t module_base);
void update_siege(Siege& state, std::uintptr_t module_base, const BattleObservation& observation,
                  std::uint64_t now_ms, bool wide, int heal_rate);

} // namespace s8::game
