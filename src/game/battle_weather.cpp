#include "battle_weather.h"

#include "tactics_data.h"
#include "terrain_tactics.h"
#include "../platform/windows/process_memory.h"

#include <array>
#include <span>

namespace s8::game {
namespace {

std::uintptr_t follow_chain(std::uintptr_t address, std::span<const std::uintptr_t> offsets) {
    for (const std::uintptr_t offset : offsets) {
        std::uintptr_t next = 0;

        if (!platform::windows::read_memory(address, std::as_writable_bytes(std::span(&next, 1))) || !next) {
            return 0;
        }
        address = next + offset;
    }
    return address;
}

bool write_byte(std::uintptr_t address, std::uint8_t value) {
    std::uint8_t current = 0;

    if (!platform::windows::read_memory(address, std::as_writable_bytes(std::span(&current, 1)))) {
        return false;
    }
    return current == value || platform::windows::write_data_memory(address, std::as_bytes(std::span(&value, 1)));
}

} // namespace

bool update_battle_weather(BattleWeather& state, std::uintptr_t module_base, std::uint64_t now_ms,
                           bool simple, bool complex, bool force, bool terrain_override) {
    if (!module_base || (!force && now_ms - state.last_update_ms < 200)) {
        return false;
    }
    state.last_update_ms = now_ms;
    const std::uintptr_t tactics = resolve_tactics_data(module_base);

    if (!tactics) {
        return false;
    }
    constexpr std::array<std::uintptr_t, 7> WEATHER_CHAIN{0x28, 0xe8, 0x1a8, 0x218, 0xf8, 0x218, 0};
    constexpr std::array<std::uintptr_t, 7> WIND_CHAIN{0x28, 0x250, 0x1d8, 0x3c0, 0, 0x50, 0};
    constexpr std::array<std::uintptr_t, 5> TERRAIN_CHAIN{0, 8, 0x10, 0, 0};
    const std::uintptr_t weather_address = simple || complex ? follow_chain(module_base + 0x02e99460, WEATHER_CHAIN) : 0;
    const std::uintptr_t wind_address = complex ? follow_chain(module_base + 0x02e99460, WIND_CHAIN) : 0;
    std::uint8_t weather = 0;
    std::uint8_t wind = 0;
    const bool weather_valid = weather_address && platform::windows::read_memory(
        weather_address + 0x559, std::as_writable_bytes(std::span(&weather, 1)));
    const bool wind_valid = wind_address && platform::windows::read_memory(
        wind_address + 0xf9, std::as_writable_bytes(std::span(&wind, 1)));
    const bool detailed = complex && weather_valid && wind_valid;
    const std::uint8_t fire_effect = detailed && weather == 0 && wind == 0 ? 2 : 25;
    const std::uint8_t rock_effect = detailed && wind == 3 ? 2 : 27;
    const std::uint8_t ambush_effect = detailed && (weather == 3 || weather == 4) ? 2 : 26;
    bool written = true;

    for (const std::uintptr_t offset : std::array<std::uintptr_t, 3>{0xc2e, 0xc50, 0xc72}) {
        written = write_byte(tactics + offset, fire_effect) && written;
    }
    for (const std::uintptr_t offset : std::array<std::uintptr_t, 3>{0xdae, 0xdd0, 0xdf2}) {
        written = write_byte(tactics + offset, ambush_effect) && written;
    }
    const std::uintptr_t terrain = follow_chain(module_base + 0x034c8630, TERRAIN_CHAIN);

    if (terrain) {
        const std::uint8_t ignite = detailed && weather == 0 && wind == 0 ? 2 : 0;
        const std::array<std::uint8_t, 5> desired = detailed && weather == 0
            ? std::array<std::uint8_t, 5>{19, 19, 0, 20, 30}
            : std::array<std::uint8_t, 5>{0, 0, 0, 100, 100};
        for (const std::uintptr_t offset : std::array<std::uintptr_t, 6>{0x1aa3a1, 0x1aa3e1, 0x1aa441,
                                                                0x1aa461, 0x1aa561, 0x1aa5a1}) {
            written = write_byte(terrain + offset, ignite) && written;
            std::array<std::uint8_t, 5> current{};
            if (!platform::windows::read_memory(terrain + offset + 3, std::as_writable_bytes(std::span(current)))) {
                written = false;
            } else if (current != desired) {
                written = platform::windows::write_data_memory(terrain + offset + 3, std::as_bytes(std::span(desired))) && written;
            }
        }
    }
    const bool adjusted = (simple || complex) && weather_valid;
    const std::uint8_t fire = adjusted && (weather == 2 || weather == 3 || weather == 4) ? 123 : 70;
    const std::uint8_t water = adjusted && (weather == 2 || weather == 3) ? 2 : 9;
    for (const std::uintptr_t offset : std::array<std::uintptr_t, 3>{0x826, 0x9a6, 0xc26}) {
        written = write_byte(tactics + offset, fire) && written;
    }
    return write_terrain_tactic_conditions(tactics, terrain_override, water, rock_effect) && written && terrain != 0;
}

} // namespace s8::game
