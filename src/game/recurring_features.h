#pragma once

#include <cstdint>

namespace s8::game {

struct RecurringFeatures {
    bool infinite_action_points = false;
    bool infinite_jewel_changes = false;
    bool infinite_banquet = false;
    bool infinite_mediation = false;
};

bool tick_infinite_banquet(std::uintptr_t module_base);
bool tick_infinite_mediation(std::uintptr_t module_base);
void tick_recurring_features(const RecurringFeatures& features, std::uintptr_t module_base,
                             std::uintptr_t game_base, bool game_base_ready,
                             std::uintptr_t player, bool player_ready);

} // namespace s8::game
