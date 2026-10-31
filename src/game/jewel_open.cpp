#include "jewel_open.h"
#include "secondary_jewel_hook.h"
#include "../platform/windows/process_memory.h"

#include <array>
#include <span>

namespace s8::game {
namespace {

constexpr std::array<std::uint8_t, 34> MASK{
    0,0,0,0,0,0,0,0, 0xfc,0xff,0xff,0xbf,0xff,0xff,0xf7,0xff,
    0xff,0xfe,0xff,0xdf,0xff,0xff,0xfb,0xff, 0x7f,0xff,0xff,0xef,0xff,0xff,0xff,0xff, 0x03,0};

bool apply(std::uintptr_t game_base, bool enabled, bool& changed) {
    changed = false;
    if (!game_base) {
        return false;
    }
    std::array<std::uint8_t, 34> bitmap{};
    const auto address = game_base + 0x71e0;
    if (!platform::windows::read_memory(address, std::as_writable_bytes(std::span(bitmap)))) {
        return false;
    }
    for (std::size_t i = 0; i < MASK.size(); ++i) {
        const auto value = static_cast<std::uint8_t>(enabled ? bitmap[i] | MASK[i] : bitmap[i] & ~MASK[i]);
        if (value != bitmap[i]) {
            if (!platform::windows::write_memory(address + i, std::as_bytes(std::span(&value, 1)))) {
                return false;
            }
            changed = true;
        }
    }
    return true;
}

} // namespace

bool set_all_jewels_open(JewelOpen& state, std::uintptr_t game_base, bool enabled) {
    state.preferred = enabled;
    bool changed = false;
    const bool applied = apply(game_base, enabled, changed);
    if (applied && enabled) {
        reset_secondary_jewel_cache();
    }
    return applied;
}

void update_jewel_open(JewelOpen& state, std::uintptr_t game_base, std::uint64_t now_ms) {
    if (now_ms - state.last_check_ms < 250) {
        return;
    }
    state.last_check_ms = now_ms;
    if (state.preferred) {
        bool changed = false;
        if (apply(game_base, true, changed) && changed) {
            reset_secondary_jewel_cache();
        }
    }
    std::array<std::uint8_t, 34> bitmap{};
    if (game_base && platform::windows::read_memory(game_base + 0x71e0, std::as_writable_bytes(std::span(bitmap)))) {
        state.bitmap = bitmap;
    } else {
        state.bitmap.reset();
    }
}

} // namespace s8::game
