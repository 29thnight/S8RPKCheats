#include "self_heal.h"
#include "tactics_data.h"

#include "../platform/windows/process_memory.h"

#include <array>
#include <span>

namespace s8::game {

bool set_self_heal(std::uintptr_t module_base, bool enabled, const domain::SelfHealSettings& settings) {
    const auto address = resolve_tactics_data(module_base);
    if (!address) {
        return false;
    }
    constexpr std::array<std::uintptr_t, 3> SELF_OFFSETS{0x102b, 0x104d, 0x106f};
    constexpr std::array<std::uintptr_t, 3> AMOUNT_OFFSETS{0x1032, 0x1054, 0x1076};
    constexpr std::array<std::uintptr_t, 4> RANGE_OFFSETS{0x105a, 0x1068, 0x107c, 0x108a};
    constexpr std::array<int, 3> DEFAULT_AMOUNTS{2000, 3500, 7000};
    for (std::size_t i = 0; i < SELF_OFFSETS.size(); ++i) {
        const std::uint16_t self = enabled && settings.self[i] ? 0x0100 : 0x0101;
        const auto amount = static_cast<std::uint16_t>(enabled ? settings.amount[i] : DEFAULT_AMOUNTS[i]);
        if (!platform::windows::write_data_memory(address + SELF_OFFSETS[i], std::as_bytes(std::span(&self, 1))) ||
            !platform::windows::write_data_memory(address + AMOUNT_OFFSETS[i], std::as_bytes(std::span(&amount, 1)))) {
            return false;
        }
    }
    for (std::size_t i = 0; i < RANGE_OFFSETS.size(); ++i) {
        const auto range = static_cast<std::uint8_t>(enabled ? settings.range[i / 2] : 1);
        if (!platform::windows::write_data_memory(address + RANGE_OFFSETS[i], std::as_bytes(std::span(&range, 1)))) {
            return false;
        }
    }
    return true;
}

} // namespace s8::game
