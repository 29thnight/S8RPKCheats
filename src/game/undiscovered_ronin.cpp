#include "undiscovered_ronin.h"
#include "../platform/windows/process_memory.h"

#include <array>
#include <span>

namespace s8::game {
namespace {

template <typename T>
bool read_value(std::uintptr_t address, T& value) {
    return platform::windows::read_memory(address, std::as_writable_bytes(std::span(&value, 1)));
}

} // namespace

void update_undiscovered_ronin(UndiscoveredRonin& state, std::uintptr_t module_base,
                               std::uintptr_t game_base, std::uintptr_t player,
                               std::uint64_t now_ms, bool enabled) {
    if (now_ms - state.last_tick_ms < 200) {
        return;
    }
    state.last_tick_ms = now_ms;
    if (!enabled) {
        state.enabled = false;
        state.pending = false;
        state.last_game_base = 0;
        state.last_player = 0;
        return;
    }
    if (!state.enabled) {
        state.enabled = true;
        state.pending = true;
    }
    if (game_base <= 0x10000 || !platform::windows::is_committed_memory(game_base + 0xe0, 8)) {
        state.last_game_base = 0;
        state.last_player = 0;
        return;
    }
    if (state.last_game_base != game_base) {
        state.last_game_base = game_base;
        state.last_player = 0;
        state.pending = true;
    }
    if (player <= 0x10000 || !platform::windows::is_committed_memory(player, 0x100)) {
        state.last_player = 0;
        return;
    }
    if (state.last_player != player) {
        state.last_player = player;
        state.pending = true;
    }
    if (!state.pending || !module_base) {
        return;
    }
    auto root = module_base + 0x034c8630;
    constexpr std::array<std::uintptr_t, 5> CHAIN{0, 0, 8, 0x10, 0};
    for (const auto offset : CHAIN) {
        std::uintptr_t next = 0;
        if (!read_value(root + offset, next) || !next) {
            return;
        }
        root = next;
    }
    const auto base = root + 0x1d4560;
    if (!platform::windows::is_committed_memory(base, 0x100)) {
        return;
    }
    std::uintptr_t scenario = 0;
    std::array<std::byte, 3> date{};
    if (!read_value(module_base + 0x02e98bc8, scenario) || scenario < 0x10000 ||
        !platform::windows::read_memory(scenario + 0x72d0, date)) {
        return;
    }
    const auto year = static_cast<std::uint16_t>(std::to_integer<unsigned>(date[0]) |
                                                 (std::to_integer<unsigned>(date[1]) << 8));
    if (year < 100 || year >= 400) {
        return;
    }
    int modified_count = 0;
    for (std::uintptr_t i = 0; i < 1000; ++i) {
        const auto officer = base + i * 0x3d0;
        if (!platform::windows::is_committed_memory(officer, 0x40)) {
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
                ++modified_count;
            }
            continue;
        }
        if (years[0] > year && years[0] < 400) {
            platform::windows::write_memory(officer + 0x32, std::as_bytes(std::span(&year, 1)));
            modified = true;
        }
        if (years[1] >= year - 15 && years[1] < 400) {
            const auto birth = static_cast<std::uint16_t>(year - 20);
            platform::windows::write_memory(officer + 0x34, std::as_bytes(std::span(&birth, 1)));
            modified = true;
        }
        if (years[2] && years[2] <= year) {
            const auto death = static_cast<std::uint16_t>(year + 50);
            platform::windows::write_memory(officer + 0x36, std::as_bytes(std::span(&death, 1)));
            modified = true;
        }
        if (modified) {
            ++modified_count;
        }
    }
    state.pending = false;
    if (modified_count > 0) {
        state.completed_modified = modified_count;
    }
}

} // namespace s8::game
