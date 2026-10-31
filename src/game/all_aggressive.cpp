#include "all_aggressive.h"

#include "../platform/windows/process_memory.h"

#include <array>
#include <cstring>
#include <span>

namespace s8::game {

void update_all_aggressive(AllAggressive& state, std::uintptr_t module_base, std::uintptr_t hero,
                           std::uint64_t now_ms, bool enabled) {
    if (now_ms - state.last_tick_ms < 500) {
        return;
    }
    state.last_tick_ms = now_ms;
    if (state.hero != hero || !enabled) {
        state.hero = hero;
        state.detected_ms = 0;
        state.applied = false;
    }
    if (!enabled || hero <= 0x10000 || state.applied) {
        return;
    }
    if (!state.detected_ms) {
        state.detected_ms = now_ms;
        return;
    }
    if (now_ms - state.detected_ms < 3000) {
        return;
    }
    std::uintptr_t roster = 0;
    if (module_base && platform::windows::read_memory(module_base + 0x034c8630,
                                                       std::as_writable_bytes(std::span(&roster, 1)))) {
        constexpr std::array<std::uintptr_t, 5> OFFSETS{0x48, 8, 0x10, 0, 8};
        for (const auto offset : OFFSETS) {
            std::uintptr_t next = 0;
            if (roster < 0x10000 || !platform::windows::read_memory(roster + offset,
                    std::as_writable_bytes(std::span(&next, 1))) || next < 0x10000) {
                roster = 0;
                break;
            }
            roster = next;
        }
    }
    if (roster < 0x10000 || !platform::windows::is_committed_memory(roster, 0x3d0)) {
        std::uint16_t id = 0;
        if (!platform::windows::read_memory(hero + 8, std::as_writable_bytes(std::span(&id, 1))) ||
            id < 1 || id > 5102) {
            return;
        }
        roster = hero - static_cast<std::uintptr_t>(id - 1) * 0x3d0;
    }
    if (roster <= 0x10000 || !platform::windows::is_committed_memory(roster, 0x3d0)) {
        return;
    }
    for (std::size_t i = 0; i < 5102; ++i) {
        const auto officer = roster + i * 0x3d0;
        if (!platform::windows::is_committed_memory(officer, 0x3d0)) {
            continue;
        }
        std::array<std::byte, 0x10a> record{};
        if (!platform::windows::read_memory(officer, record)) {
            continue;
        }
        std::uint16_t id = 0;
        std::memcpy(&id, record.data() + 8, sizeof(id));
        if (id != i + 1) {
            continue;
        }
        const auto status = std::to_integer<std::uint8_t>(record[0x10]);
        if (status != 0x18 && status != 0x28 && status != 0x38 && status != 0x48 &&
            status != 0x58 && status != 0x68 && status != 0x78 && status != 0x88 &&
            status != 0xe8 && status != 0xd8 && status != 0xc8) {
            continue;
        }
        if (record[0x60] != std::byte{4}) {
            constexpr std::uint8_t AGGRESSIVE = 4;
            platform::windows::write_data_memory(officer + 0x60, std::as_bytes(std::span(&AGGRESSIVE, 1)));
        }
    }
    state.applied = true;
}

} // namespace s8::game
