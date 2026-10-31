#include "celestial.h"

#include "tactics_data.h"
#include "../platform/windows/process_memory.h"

#include <array>
#include <span>

namespace s8::game {

bool set_celestial(std::uintptr_t module_base, bool enabled, const domain::CelestialSettings& settings) {
    const auto root = resolve_tactics_data(module_base);
    if (!root) {
        return false;
    }
    const auto base = root + 0x10a0;
    constexpr std::array<std::uintptr_t, 3> EFFECT_OFFSETS{0x1c, 0x1e, 0x22};
    constexpr std::array<std::uint8_t, 3> EFFECT_VALUES{20, 6, 99};
    for (std::size_t i = 0; i < 3; ++i) {
        const auto level = base + i * 0x22;
        for (std::size_t j = 0; j < EFFECT_OFFSETS.size(); ++j) {
            const std::uint8_t value = enabled ? EFFECT_VALUES[j] : 0;
            if (!platform::windows::write_data_memory(level + EFFECT_OFFSETS[j], std::as_bytes(std::span(&value, 1)))) {
                return false;
            }
        }
        const auto amount = static_cast<std::uint16_t>(enabled ? settings.amount[i] : 1);
        const auto range = static_cast<std::uint8_t>(enabled ? settings.range[i] : 1);
        const auto probability = static_cast<std::uint8_t>(enabled ? settings.probability[i] : 10);
        if (!platform::windows::write_data_memory(level + 0x20, std::as_bytes(std::span(&amount, 1))) ||
            !platform::windows::write_data_memory(level + 0x26, std::as_bytes(std::span(&range, 1))) ||
            !platform::windows::write_data_memory(level + 0x27, std::as_bytes(std::span(&probability, 1)))) {
            return false;
        }
    }
    return true;
}

} // namespace s8::game
