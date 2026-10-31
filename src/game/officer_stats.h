#pragma once

#include <cstdint>

namespace s8::game {

struct OfficerStatsMonitor {
    std::uint8_t last_council_month = 0xff;
};

void update_officer_stats(OfficerStatsMonitor& state, std::uintptr_t module_base,
                          std::uintptr_t game_base, bool enabled);

} // namespace s8::game
