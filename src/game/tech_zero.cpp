#include "tech_zero.h"

#include "../platform/windows/process_memory.h"

#include <array>
#include <algorithm>
#include <span>

namespace s8::game {

void set_tech_zero(TechZero& state, bool enabled) {
    if (state.enabled != enabled) {
        state = {};
        state.enabled = enabled;
    }
}

void tick_tech_zero(TechZero& state, std::uintptr_t module_base, std::uint64_t now_ms,
    const domain::WorkBudget& work_budget) {
    if (!state.enabled || (!state.running && now_ms - state.last_poll_ms < 200) ||
        !module_base || work_budget.exhausted()) {
        return;
    }
    state.last_poll_ms = now_ms;
    std::uintptr_t address = module_base + 0x034c8630;
    constexpr std::array<std::uintptr_t, 5> OFFSETS{0, 0, 8, 0x10, 0};
    for (const std::uintptr_t offset : OFFSETS) {
        std::uintptr_t next = 0;
        if (!platform::windows::read_memory(address + offset, std::as_writable_bytes(std::span(&next, 1))) || !next) {
            return;
        }
        address = next;
    }
    if (state.table_root != address) {
        state = {};
        state.enabled = true;
        state.table_root = address;
        state.last_poll_ms = now_ms;
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
        state.running = false;
        state.cursor = 0;
        state.trigger_ms = 0;
        return;
    }
    if (state.pending && state.active_month != month) {
        state.pending = false;
        state.running = false;
        state.cursor = 0;
        state.trigger_ms = 0;
    }
    if (!state.pending) {
        if ((month == 1 || month == 4 || month == 7 || month == 10) && elapsed == 0 && !state.done) {
            state.pending = true;
            state.trigger_ms = now_ms;
            state.active_month = month;
            state.cursor = 0;
        }
        return;
    }
    if (now_ms - state.trigger_ms < 200) {
        return;
    }
    constexpr std::array<std::byte, 5> ZERO{};
    constexpr std::size_t RECORDS_PER_TICK = 16;
    const std::size_t end = std::min(state.cursor + RECORDS_PER_TICK, std::size_t{100});

    state.running = true;
    for (; state.cursor < end && !work_budget.exhausted(); ++state.cursor) {
        const std::uintptr_t target = address + 0xc6a4d + state.cursor * 0x998;
        std::array<std::byte, 5> current{};

        if (platform::windows::read_memory(target, current) && current != ZERO) {
            platform::windows::write_data_memory(target, ZERO);
        }
    }
    if (state.cursor == 100) {
        state.done = true;
        state.pending = false;
        state.running = false;
        state.trigger_ms = 0;
    }
}

} // namespace s8::game
