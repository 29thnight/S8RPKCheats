#pragma once

#include "../domain/skill_counts.h"

#include <cstdint>

namespace s8::game {

struct ShipWeaponization {
    std::uint64_t last_update_ms = 0;
    std::uintptr_t last_active_unit = 0;
    bool last_triggered = false;
};

void update_ship_weaponization(ShipWeaponization& state, std::uintptr_t module_base,
                               const domain::SkillCounts& skill_counts, std::uint64_t now_ms);

} // namespace s8::game
