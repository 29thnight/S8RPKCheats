#include "officer_stats.h"

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

} // namespace

void update_officer_stats(OfficerStatsMonitor& state, std::uintptr_t module_base,
                          std::uintptr_t game_base, bool enabled, const domain::WorkBudget& work_budget) {
    if (state.module_base != module_base || state.game_base != game_base) {
        state = {};
        state.module_base = module_base;
        state.game_base = game_base;
    }
    if (!enabled) {
        state.running = false;
    }
    if (work_budget.exhausted()) {
        return;
    }
    std::uint8_t game_state = 0;

    if (!game_base || !read_value(game_base + 0xd0, game_state)) {
        return;
    }
    if (game_state == 7) {
        state.last_council_month = 0xff;
        state.running = false;
        return;
    }
    if (game_state != 5) {
        return;
    }
    std::uintptr_t scenario = 0;
    std::uint8_t month = 0;
    if (module_base && read_value(module_base + 0x02e98bc8, scenario) && scenario >= 0x10000) {
        std::array<std::byte, 3> date{};
        if (platform::windows::read_memory(scenario + 0x72d0, date)) {
            month = std::to_integer<std::uint8_t>(date[2]);
        }
    }
    if (!state.running && state.last_council_month == month) {
        return;
    }
    if (!enabled) {
        state.last_council_month = month;
        return;
    }
    std::uintptr_t array = 0;
    if (!read_value(game_base + 0x3b8, array) || !platform::windows::is_committed_memory(array, 0x1000)) {
        std::uintptr_t player = 0;
        std::uint16_t id = 0;
        if (!read_value(game_base + 0xe0, player) || player <= 0x10000 ||
            !read_value(player + 8, id) || id == 0 || id > 5102) {
            return;
        }
        array = player - static_cast<std::uintptr_t>(id - 1) * 0x3d0;
        if (!platform::windows::is_committed_memory(array, 0x1000)) {
            return;
        }
    }
    if (!state.running || state.roster != array || state.last_council_month != month) {
        state.roster = array;
        state.cursor = 0;
        state.last_council_month = month;
        state.running = true;
    }
    constexpr std::size_t OFFICERS_PER_TICK = 32;
    const std::size_t end = std::min(state.cursor + OFFICERS_PER_TICK, std::size_t{5102});

    for (; state.cursor < end && !work_budget.exhausted(); ++state.cursor) {
        const std::uintptr_t officer = array + state.cursor * 0x3d0;

        if (!platform::windows::is_committed_memory(officer, 0x150)) {
            continue;
        }
        std::uint8_t status = 0;
        if (!read_value(officer + 0x10, status)) {
            continue;
        }
        if (status != 0x18 && status != 0x28 && status != 0x38 && status != 0x48 &&
            status != 0xe8 && status != 0xd8 && status != 0xc8) {
            continue;
        }
        std::array<std::uint8_t, 5> stats{};
        if (!platform::windows::read_memory(officer + 0xaa, std::as_writable_bytes(std::span(stats)))) {
            continue;
        }
        constexpr std::uint8_t UPGRADED = 100;
        for (std::size_t stat = 0; stat < stats.size(); ++stat) {
            if (stats[stat] == 99) {
                platform::windows::write_data_memory(officer + 0xaa + stat,
                    std::as_bytes(std::span(&UPGRADED, 1)));
            }
        }
    }
    if (state.cursor == 5102) {
        state.running = false;
    }
}

} // namespace s8::game
