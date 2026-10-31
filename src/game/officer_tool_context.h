#pragma once

#include "../domain/ids.h"

#include <cstdint>

namespace s8::game {

struct OfficerToolContext {
    std::uintptr_t module = 0;
    std::uintptr_t game_base = 0;
    std::uintptr_t player = 0;
    domain::GameEpoch epoch{};
    std::uint64_t now_ms = 0;
    std::uintptr_t game_base_pointer = 0;
};

bool read_scenario_date(std::uintptr_t module, std::uint16_t& year, std::uint8_t& month);

} // namespace s8::game
