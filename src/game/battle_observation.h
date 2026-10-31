#pragma once

#include <cstdint>
#include <optional>

namespace s8::game {

struct BattleObservation {
    std::uintptr_t unit_list;
    std::uintptr_t day_base;
    std::uint8_t day;
    std::uint8_t unit_count;
};

struct BattleReadiness {
    std::uintptr_t unit_list = 0;
    std::uintptr_t day_base = 0;
    std::uint64_t stable_since_ms = 0;
    bool ready = false;
};

std::optional<BattleObservation> read_battle_observation(std::uintptr_t module_base);
bool battle_records_ready(std::uintptr_t game_base, const BattleObservation& observation);
bool update_battle_readiness(BattleReadiness& state, const BattleObservation& observation,
                             bool records_ready, std::uint64_t now_ms);

} // namespace s8::game
