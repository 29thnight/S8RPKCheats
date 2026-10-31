#include "zero_infamy.h"
#include "../platform/windows/process_memory.h"
#include <span>

namespace s8::game {

void update_zero_infamy(ZeroInfamy& state, std::uintptr_t game_base, std::uint64_t now_ms, bool enabled) {
    if (state.last_update_ms && now_ms - state.last_update_ms < 200) {
        return;
    }
    state.last_update_ms = now_ms;
    if (!enabled || game_base <= 0x10000) {
        return;
    }
    std::uintptr_t player = 0;
    if (!platform::windows::read_memory(game_base + 0xe0, std::as_writable_bytes(std::span(&player, 1))) ||
        player <= 0x10000) {
        return;
    }
    std::uint16_t infamy = 0;
    if (platform::windows::read_memory(player + 0x108, std::as_writable_bytes(std::span(&infamy, 1))) && infamy) {
        constexpr std::uint16_t ZERO = 0;
        platform::windows::write_memory(player + 0x108, std::as_bytes(std::span(&ZERO, 1)));
    }
}

} // namespace s8::game
