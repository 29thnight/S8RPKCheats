#include "tech_zero.h"

#include "../platform/windows/process_memory.h"

#include <array>
#include <span>

namespace s8::game {

void set_tech_zero(TechZero& state, bool enabled) {
    if (state.enabled != enabled) {
        state = {};
        state.enabled = enabled;
    }
}

void tick_tech_zero(TechZero& state, std::uintptr_t module_base, std::uint64_t now_ms) {
    if (!state.enabled || now_ms - state.last_poll_ms < 200 || !module_base) {
        return;
    }
    state.last_poll_ms = now_ms;
    auto address = module_base + 0x034c8630;
    constexpr std::array<std::uintptr_t, 5> OFFSETS{0, 0, 8, 0x10, 0};
    for (const auto offset : OFFSETS) {
        std::uintptr_t next = 0;
        if (!platform::windows::read_memory(address + offset, std::as_writable_bytes(std::span(&next, 1))) || !next) {
            return;
        }
        address = next;
    }
    std::uint8_t month = 0;
    std::uint16_t elapsed = 0;
    if (!platform::windows::read_memory(address + 0x49ec94, std::as_writable_bytes(std::span(&month, 1))) ||
        !platform::windows::read_memory(address + 0x8142, std::as_writable_bytes(std::span(&elapsed, 1)))) {
        return;
    }
    if (month == 0 && elapsed == 0) {
        state.done = false;
        state.pending = false;
        state.trigger_ms = 0;
        return;
    }
    if (!state.pending) {
        if ((month == 1 || month == 4 || month == 7 || month == 10) && elapsed == 0 && !state.done) {
            state.pending = true;
            state.trigger_ms = now_ms;
        }
        return;
    }
    if (now_ms - state.trigger_ms < 200) {
        return;
    }
    constexpr std::array<std::byte, 5> ZERO{};
    for (std::uintptr_t i = 0; i < 100; ++i) {
        const auto target = address + 0xc6a4d + i * 0x998;
        std::array<std::byte, 5> current{};
        if (platform::windows::read_memory(target, current) && current != ZERO) {
            platform::windows::write_data_memory(target, ZERO);
        }
    }
    state.done = true;
    state.pending = false;
    state.trigger_ms = 0;
}

} // namespace s8::game
