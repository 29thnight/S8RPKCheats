#pragma once

#include <cstdint>
#include <vector>

namespace s8::game {

struct FactionLordBonus {
    std::vector<std::uintptr_t> assigned;
    std::uint64_t last_tick_ms = 0;
    bool enabled = false;
    bool names_applied = false;
};

bool set_faction_lord_bonus(FactionLordBonus& state, std::uintptr_t module_base, bool enabled);
void update_faction_lord_bonus(FactionLordBonus& state, std::uintptr_t module_base, std::uint64_t now_ms);

} // namespace s8::game
