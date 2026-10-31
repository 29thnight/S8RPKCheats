#pragma once

#include <cstdint>

namespace s8::game {

struct ShortBattleCooldown {
    bool applied = false;
    bool original_captured = false;
    std::int32_t original = 10;
    std::int32_t last_applied = 10;
};

bool set_short_battle_cooldown(ShortBattleCooldown& state, std::uintptr_t module_base,
                               bool enabled, std::int32_t days);

} // namespace s8::game
