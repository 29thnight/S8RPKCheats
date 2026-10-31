#include "terrain_tactics.h"

#include "tactics_data.h"
#include "../platform/windows/process_memory.h"

#include <array>
#include <span>

namespace s8::game {
namespace {

template <typename Value>
bool write_value(std::uintptr_t address, const Value& value) {
    return platform::windows::write_data_memory(address, std::as_bytes(std::span(&value, 1)));
}

} // namespace

bool write_terrain_tactic_conditions(std::uintptr_t tactics, bool terrain_override,
                                     std::uint8_t water_condition, std::uint8_t stone_condition) {
    const std::array<std::uint8_t, 2> desired{
        static_cast<std::uint8_t>(terrain_override ? 11 : water_condition),
        static_cast<std::uint8_t>(terrain_override ? 11 : stone_condition)};
    constexpr std::array<std::uintptr_t, 2> OFFSETS{0xcae, 0xd2e};
    bool complete = true;

    for (std::size_t kind = 0; kind < OFFSETS.size(); ++kind) {
        for (std::uintptr_t level = 0; level < 3; ++level) {
            const std::uintptr_t address = tactics + OFFSETS[kind] + level * 0x22;
            std::uint8_t current = 0;

            if (!platform::windows::read_memory(address, std::as_writable_bytes(std::span(&current, 1)))) {
                complete = false;
            } else if (current != desired[kind] && !platform::windows::write_data_memory(address,
                           std::as_bytes(std::span(&desired[kind], 1)))) {
                complete = false;
            }
        }
    }
    return complete;
}

bool set_terrain_tactics(std::uintptr_t module_base, bool enabled, const domain::TerrainTacticsSettings& settings) {
    const std::uintptr_t root = resolve_tactics_data(module_base);

    if (!root) {
        return false;
    }
    constexpr std::array<int, 3> DEFAULT_AMOUNT{120, 140, 170};

    for (std::size_t kind = 0; kind < 2; ++kind) {
        const std::array<int, 3>& amounts = kind == 0 ? settings.water_amount : settings.stone_amount;
        const std::array<int, 3>& ranges = kind == 0 ? settings.water_range : settings.stone_range;

        for (std::size_t level = 0; level < 3; ++level) {
            const std::uintptr_t base = root + (kind == 0 ? 0xcae : 0xd2e) + level * 0x22;
            const std::uint16_t amount1 = static_cast<std::uint16_t>(enabled ? amounts[level] : DEFAULT_AMOUNT[level]);
            const std::uint16_t amount2 = static_cast<std::uint16_t>(enabled ? amounts[level] : 0);
            const std::uint8_t range = static_cast<std::uint8_t>(enabled ? ranges[level] : 0);

            if (!write_value(base + 4, amount1) ||
                !write_value(base + 0x12, amount2) || !write_value(base + 0x18, range)) {
                return false;
            }
            if (enabled) {
                const std::uint8_t effect = static_cast<std::uint8_t>(kind == 0 ? 9 : 27);
                const std::uint8_t target = 1, special = 3, probability = 100;

                if (!write_value(base + 0x0e, effect) || !write_value(base + 0x10, target) ||
                    !write_value(base + 0x14, special) || !write_value(base + 0x19, probability)) {
                    return false;
                }
            }
        }
    }
    return write_terrain_tactic_conditions(root, enabled, 9, 27);
}

} // namespace s8::game
