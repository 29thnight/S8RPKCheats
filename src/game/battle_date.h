#pragma once

#include "battle_observation.h"

#include <cstdint>

namespace s8::game {

struct BattleDate {
    std::uint64_t last_update_ms = 0;
    std::uint8_t last_observed_day = 0;
};

void update_battle_date(BattleDate& state, const BattleObservation& observation, std::uint64_t now_ms,
                        bool always_fifteen, bool dynamic, bool skip_enabled, int skip_days, bool force = false);

} // namespace s8::game
