#pragma once

#include <cstdint>

namespace s8::game {
struct ZeroInfamy { std::uint64_t last_update_ms = 0; };
void update_zero_infamy(ZeroInfamy& state, std::uintptr_t game_base, std::uint64_t now_ms, bool enabled);
} // namespace s8::game
