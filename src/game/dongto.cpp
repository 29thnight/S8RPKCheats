#include "dongto.h"

#include "tactics_data.h"
#include "../platform/windows/process_memory.h"

#include <array>
#include <span>

namespace s8::game {

bool set_dongto(std::uintptr_t module_base, bool enabled, const domain::DongtoSettings& settings) {
    const auto root = resolve_tactics_data(module_base);
    if (root < 0x200) {
        return false;
    }
    const auto address = root - 0x200;
    constexpr std::array<std::uintptr_t, 3> PROBABILITY{0x1029, 0x104b, 0x106d};
    for (std::size_t i = 0; i < PROBABILITY.size(); ++i) {
        const auto value = static_cast<std::uint16_t>(enabled ? settings.probability[i] : 10);
        if (!platform::windows::write_data_memory(address + PROBABILITY[i], std::as_bytes(std::span(&value, 1)))) {
            return false;
        }
    }
    if (enabled) {
        const auto value = static_cast<std::uint16_t>(settings.status_probability[0]);
        if (!platform::windows::write_data_memory(address + 0x1047, std::as_bytes(std::span(&value, 1)))) {
            return false;
        }
    }
    constexpr std::array<std::uintptr_t, 2> STATUS{0x1069, 0x108b};
    for (std::size_t i = 0; i < STATUS.size(); ++i) {
        const auto value = static_cast<std::uint8_t>(enabled ? settings.status_probability[i + 1] : 10);
        if (!platform::windows::write_data_memory(address + STATUS[i], std::as_bytes(std::span(&value, 1)))) {
            return false;
        }
    }
    return true;
}

} // namespace s8::game
