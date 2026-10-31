#include "undiscovered_ronin.h"
#include "../platform/windows/process_memory.h"

#include <array>
#include <algorithm>
#include <span>

namespace s8::game {
namespace {

template <typename T>
bool read_value(std::uintptr_t address, T& value) {
    return platform::windows::read_memory(address, std::as_writable_bytes(std::span(&value, 1)));
}

void reset_progress(UndiscoveredRonin& state) {
    state.record_base = 0;
    state.scenario = 0;
    state.cursor = 0;
    state.year = 0;
    state.modified_count = 0;
}

} // namespace

void update_undiscovered_ronin(UndiscoveredRonin& state, std::uintptr_t module_base,
                               std::uintptr_t game_base, std::uintptr_t player,
                               std::uint64_t now_ms, bool enabled, const domain::WorkBudget& work_budget) {
    if (!enabled) {
        state.enabled = false;
        state.pending = false;
        state.last_game_base = 0;
        state.last_player = 0;
        reset_progress(state);
        return;
    }
    if (!state.enabled) {
        state.enabled = true;
        state.pending = true;
        reset_progress(state);
    }
    if (work_budget.exhausted() ||
        ((!state.pending || state.record_base == 0) && now_ms - state.last_tick_ms < 200)) {
        return;
    }
    state.last_tick_ms = now_ms;
    if (game_base <= 0x10000 || !platform::windows::is_committed_memory(game_base + 0xe0, 8)) {
        state.last_game_base = 0;
        state.last_player = 0;
        reset_progress(state);
        return;
    }
    if (state.last_game_base != game_base) {
        state.last_game_base = game_base;
        state.last_player = 0;
        state.pending = true;
        reset_progress(state);
    }
    if (player <= 0x10000 || !platform::windows::is_committed_memory(player, 0x100)) {
        state.last_player = 0;
        reset_progress(state);
        return;
    }
    if (state.last_player != player) {
        state.last_player = player;
        state.pending = true;
        reset_progress(state);
    }
    if (!state.pending || !module_base) {
        return;
    }
    std::uintptr_t root = module_base + 0x034c8630;
    constexpr std::array<std::uintptr_t, 5> CHAIN{0, 0, 8, 0x10, 0};
    for (const std::uintptr_t offset : CHAIN) {
        std::uintptr_t next = 0;
        if (!read_value(root + offset, next) || !next) {
            return;
        }
        root = next;
    }
    const std::uintptr_t base = root + 0x1d4560;
    if (!platform::windows::is_committed_memory(base, 0x100)) {
        return;
    }
    std::uintptr_t scenario = 0;
    std::array<std::byte, 3> date{};
    if (!read_value(module_base + 0x02e98bc8, scenario) || scenario < 0x10000 ||
        !platform::windows::read_memory(scenario + 0x72d0, date)) {
        return;
    }
    const std::uint16_t year = static_cast<std::uint16_t>(std::to_integer<unsigned>(date[0]) |
        (std::to_integer<unsigned>(date[1]) << 8));
    if (year < 100 || year >= 400) {
        return;
    }
    if (state.record_base != base || state.scenario != scenario) {
        reset_progress(state);
        state.record_base = base;
        state.scenario = scenario;
    }
    if (state.year != year) {
        state.year = year;
        state.cursor = 0;
    }
    constexpr std::size_t OFFICERS_PER_TICK = 32;
    const std::size_t end = std::min(state.cursor + OFFICERS_PER_TICK, std::size_t{1000});

    for (; state.cursor < end && !work_budget.exhausted(); ++state.cursor) {
        const std::uintptr_t officer = base + state.cursor * 0x3d0;

        if (!platform::windows::is_committed_memory(officer, 0x40)) {
            state.cursor = 1000;
            break;
        }
        std::uint16_t id = 0;
        std::uint8_t status = 0;
        if (!read_value(officer + 8, id) || id == 0 || id > 2000 || !read_value(officer + 0x10, status)) {
            continue;
        }
        bool modified = false;
        if (status == 0x68 || status == 0x78) {
            constexpr std::uint8_t RONIN = 0x58;
            platform::windows::write_data_memory(officer + 0x10, std::as_bytes(std::span(&RONIN, 1)));
            modified = true;
        }
        std::array<std::uint16_t, 3> years{};
        if (!platform::windows::read_memory(officer + 0x32, std::as_writable_bytes(std::span(years)))) {
            if (modified) {
                ++state.modified_count;
            }
            continue;
        }
        if (years[0] > year && years[0] < 400) {
            platform::windows::write_memory(officer + 0x32, std::as_bytes(std::span(&year, 1)));
            modified = true;
        }
        if (years[1] >= year - 15 && years[1] < 400) {
            const std::uint16_t birth = static_cast<std::uint16_t>(year - 20);
            platform::windows::write_memory(officer + 0x34, std::as_bytes(std::span(&birth, 1)));
            modified = true;
        }
        if (years[2] && years[2] <= year) {
            const std::uint16_t death = static_cast<std::uint16_t>(year + 50);
            platform::windows::write_memory(officer + 0x36, std::as_bytes(std::span(&death, 1)));
            modified = true;
        }
        if (modified) {
            ++state.modified_count;
        }
    }
    if (state.cursor == 1000) {
        state.pending = false;
        if (state.modified_count > 0) {
            state.completed_modified = state.modified_count;
        }
    }
}

} // namespace s8::game
