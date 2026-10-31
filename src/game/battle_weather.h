#pragma once

#include <cstdint>

namespace s8::game {

struct BattleWeather {
    std::uint64_t last_update_ms = 0;
};

bool update_battle_weather(BattleWeather& state, std::uintptr_t module_base, std::uint64_t now_ms,
                           bool simple, bool complex, bool force = false, bool terrain_override = false);

} // namespace s8::game
