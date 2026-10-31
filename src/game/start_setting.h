#pragma once

#include <array>
#include <cstdint>

namespace s8::game {

struct StartSettingState {
    std::uintptr_t root = 0;
    std::uint64_t last_tick_ms = 0;
    std::array<bool, 5> applied{};
};

// Runs while scenario data are being initialized (year == 0), including menus.
// The returned bitmask identifies the five fixes newly applied in this tick.
std::uint8_t update_start_setting(StartSettingState& state, std::uintptr_t module_base,
                                 std::uint64_t now_ms, bool enabled);

} // namespace s8::game
