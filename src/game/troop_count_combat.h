#pragma once

#include <cstdint>

namespace s8::game {

struct TroopCountCombat {
    bool applied = false;
};

bool set_troop_count_combat(TroopCountCombat& state, std::uintptr_t module_base, bool enabled);

} // namespace s8::game
