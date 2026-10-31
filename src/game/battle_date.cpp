#include "battle_date.h"

#include "../platform/windows/process_memory.h"

#include <algorithm>
#include <span>

namespace s8::game {

void update_battle_date(BattleDate& state, const BattleObservation& observation, std::uint64_t now_ms,
                        bool always_fifteen, bool dynamic, bool skip_enabled, int skip_days, bool force) {
    if ((!force && now_ms - state.last_update_ms < 200) || observation.day < 1 || observation.day > 30) {
        return;
    }
    state.last_update_ms = now_ms;
    const auto final_address = observation.day_base + 0x2c;
    std::uint8_t current_final = 0;
    if (!platform::windows::read_memory(final_address, std::as_writable_bytes(std::span(&current_final, 1)))) {
        return;
    }
    const std::uint8_t target = always_fifteen ? 15 : dynamic
        ? (observation.unit_count < 15 ? 15 : observation.unit_count < 20 ? 20 : 25) : 30;
    if (current_final != target &&
        platform::windows::write_data_memory(final_address, std::as_bytes(std::span(&target, 1)))) {
        current_final = target;
    }
    if (skip_enabled && skip_days > 0 && observation.day == 1) {
        const int days = std::min(skip_days, static_cast<int>(current_final) - 1);
        if (days > 0) {
            const auto day = static_cast<std::uint8_t>(1 + days);
            platform::windows::write_data_memory(observation.day_base + 0x28, std::as_bytes(std::span(&day, 1)));
        }
    }
}

} // namespace s8::game
