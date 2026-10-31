#include "short_battle_cooldown.h"
#include "../platform/windows/process_memory.h"

#include <span>

namespace s8::game {

bool set_short_battle_cooldown(ShortBattleCooldown& state, std::uintptr_t module_base,
                               bool enabled, std::int32_t days) {
    if (!module_base)
        return false;
    const auto address = module_base + 0x03bb0214;
    std::int32_t current = 0;
    if (!platform::windows::read_memory(address, std::as_writable_bytes(std::span(&current, 1))))
        return false;

    if (enabled) {
        if (!state.applied) {
            if (current != 10 && current != days)
                return false;
            state.original = current;
            state.original_captured = true;
        } else if (current != state.last_applied && current != days) {
            return false;
        }
        if (current != days && !platform::windows::write_data_memory(address, std::as_bytes(std::span(&days, 1))))
            return false;
        state.last_applied = days;
        state.applied = true;
        return true;
    }

    if (!state.applied)
        return true;
    if (!state.original_captured || current != state.last_applied)
        return false;
    if (current != state.original &&
        !platform::windows::write_data_memory(address, std::as_bytes(std::span(&state.original, 1))))
        return false;
    state.applied = false;
    state.original_captured = false;
    state.last_applied = 10;
    return true;
}

} // namespace s8::game
