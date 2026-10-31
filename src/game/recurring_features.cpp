#include "recurring_features.h"
#include "../platform/windows/process_memory.h"

#include <array>
#include <span>

namespace s8::game {
namespace {

bool clear_scenario_flag(std::uintptr_t module_base, std::uintptr_t offset, std::uint8_t mask) {
    if (!module_base)
        return false;
    std::uintptr_t scenario = 0;
    if (!platform::windows::read_memory(module_base + 0x02e98bc8,
                                         std::as_writable_bytes(std::span(&scenario, 1))) || scenario < 0x10000)
        return false;
    std::array<std::byte, 3> date{};
    if (!platform::windows::read_memory(scenario + 0x72d0, date))
        return false;
    const auto address = scenario + offset;
    std::uint8_t value = 0;
    if (!platform::windows::read_memory(address, std::as_writable_bytes(std::span(&value, 1))))
        return false;
    if ((value & mask) == 0)
        return true;
    value = static_cast<std::uint8_t>(value & static_cast<std::uint8_t>(~mask));
    if (!platform::windows::write_data_memory(address, std::as_bytes(std::span(&value, 1))))
        return false;
    std::uint8_t current = 0;
    return platform::windows::read_memory(address, std::as_writable_bytes(std::span(&current, 1))) &&
           (current & mask) == 0;
}

} // namespace

bool tick_infinite_banquet(std::uintptr_t module_base) {
    return clear_scenario_flag(module_base, 0x71e4, 0x02);
}

bool tick_infinite_mediation(std::uintptr_t module_base) {
    return clear_scenario_flag(module_base, 0x71e5, 0x40);
}

void tick_recurring_features(const RecurringFeatures& features, std::uintptr_t module_base,
                             std::uintptr_t game_base, bool game_base_ready,
                             std::uintptr_t player, bool player_ready) {
    if (features.infinite_action_points && player_ready) {
        const std::uint8_t value = 200;
        platform::windows::write_memory(player + 0xee, std::as_bytes(std::span(&value, 1)));
    }
    if (features.infinite_banquet)
        tick_infinite_banquet(module_base);
    if (features.infinite_mediation)
        tick_infinite_mediation(module_base);
    if (features.infinite_jewel_changes && game_base_ready) {
        const std::uint8_t value = 0;
        platform::windows::write_memory(game_base + 0x5c49, std::as_bytes(std::span(&value, 1)));
    }
}

} // namespace s8::game
