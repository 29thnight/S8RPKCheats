#pragma once

#include <cstdint>

namespace s8::game {

struct TechZero {
    bool enabled = false;
    bool done = false;
    bool pending = false;
    std::uint64_t last_poll_ms = 0;
    std::uint64_t trigger_ms = 0;
};

void set_tech_zero(TechZero& state, bool enabled);
void tick_tech_zero(TechZero& state, std::uintptr_t module_base, std::uint64_t now_ms);

} // namespace s8::game
