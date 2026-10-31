#include "catapult.h"

#include "tactics_data.h"
#include "../platform/windows/process_memory.h"

#include <span>

namespace s8::game {

bool set_catapult(std::uintptr_t module_base, bool enabled, const domain::CatapultSettings& settings) {
    const auto root = resolve_tactics_data(module_base);
    if (!root) {
        return false;
    }
    const auto target = root + 0x14a0;
    const auto min_range = static_cast<std::uint8_t>(enabled ? settings.min_range : 2);
    const auto max_range = static_cast<std::uint8_t>(enabled ? settings.max_range : 2);
    const auto amount = static_cast<std::uint16_t>(enabled ? settings.amount : 20);
    const auto range = static_cast<std::uint8_t>(enabled ? settings.range : 9);
    return platform::windows::write_data_memory(target + 0x0b, std::as_bytes(std::span(&min_range, 1))) &&
           platform::windows::write_data_memory(target + 0x0c, std::as_bytes(std::span(&max_range, 1))) &&
           platform::windows::write_data_memory(target + 0x12, std::as_bytes(std::span(&amount, 1))) &&
           platform::windows::write_data_memory(target + 0x18, std::as_bytes(std::span(&range, 1)));
}

} // namespace s8::game
