#include "terrain_tactics.h"

#include "tactics_data.h"
#include "../platform/windows/process_memory.h"

#include <array>
#include <span>

namespace s8::game {

bool set_terrain_tactics(std::uintptr_t module_base, bool enabled, const domain::TerrainTacticsSettings& settings) {
    const auto root = resolve_tactics_data(module_base);
    if (!root) {
        return false;
    }
    const auto write = [](std::uintptr_t address, const auto& value) {
        return platform::windows::write_data_memory(address, std::as_bytes(std::span(&value, 1)));
    };
    constexpr std::array<int, 3> DEFAULT_AMOUNT{120, 140, 170};
    for (std::size_t kind = 0; kind < 2; ++kind) {
        const auto& amounts = kind == 0 ? settings.water_amount : settings.stone_amount;
        const auto& ranges = kind == 0 ? settings.water_range : settings.stone_range;
        for (std::size_t level = 0; level < 3; ++level) {
            const auto base = root + (kind == 0 ? 0xcae : 0xd2e) + level * 0x22;
            const auto condition = static_cast<std::uint8_t>(enabled ? 11 : (kind == 0 ? 9 : 27));
            const auto amount1 = static_cast<std::uint16_t>(enabled ? amounts[level] : DEFAULT_AMOUNT[level]);
            const auto amount2 = static_cast<std::uint16_t>(enabled ? amounts[level] : 0);
            const auto range = static_cast<std::uint8_t>(enabled ? ranges[level] : 0);
            if (!write(base, condition) || !write(base + 4, amount1) ||
                !write(base + 0x12, amount2) || !write(base + 0x18, range)) {
                return false;
            }
            if (enabled) {
                const auto effect = static_cast<std::uint8_t>(kind == 0 ? 9 : 27);
                const std::uint8_t target = 1, special = 3, probability = 100;
                if (!write(base + 0x0e, effect) || !write(base + 0x10, target) ||
                    !write(base + 0x14, special) || !write(base + 0x19, probability)) {
                    return false;
                }
            }
        }
    }
    return true;
}

} // namespace s8::game
