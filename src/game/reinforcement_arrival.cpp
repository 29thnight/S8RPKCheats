#include "reinforcement_arrival.h"
#include "../platform/windows/process_memory.h"

#include <array>
#include <cstddef>

namespace s8::game {
namespace {

constexpr std::array ORIGINAL_TABLE{
    std::byte{0x13}, std::byte{0x6c}, std::byte{0xd8}, std::byte{0x01},
    std::byte{0x5b}, std::byte{0x6c}, std::byte{0xd8}, std::byte{0x01}};
constexpr std::array ENABLED_TABLE{
    std::byte{0x5b}, std::byte{0x6c}, std::byte{0xd8}, std::byte{0x01},
    std::byte{0x13}, std::byte{0x6c}, std::byte{0xd8}, std::byte{0x01}};
constexpr std::array TURN_ANCHOR{
    std::byte{0xff}, std::byte{0xc8}, std::byte{0x83}, std::byte{0xf8}, std::byte{0x0e}};
constexpr std::array ARRIVAL_ANCHOR{
    std::byte{0xe8}, std::byte{0x5a}, std::byte{0xaa}, std::byte{0x0b}, std::byte{0x00}};
constexpr std::array ORDER_ANCHOR{
    std::byte{0xe8}, std::byte{0x6b}, std::byte{0x66}, std::byte{0x0b}, std::byte{0x00}};

template <std::size_t N>
bool matches(std::uintptr_t address, const std::array<std::byte, N>& expected) {
    std::array<std::byte, N> actual{};
    return platform::windows::read_memory(address, actual) && actual == expected;
}

} // namespace

bool set_reinforcement_arrival(ReinforcementArrival& state, std::uintptr_t module_base, bool enabled) {
    if (!module_base)
        return false;
    const auto table_address = module_base + 0x01d86d58;
    const bool original = matches(table_address, ORIGINAL_TABLE);
    const bool active = matches(table_address, ENABLED_TABLE);
    if (!original && !active)
        return false;

    if (enabled) {
        if (state.applied && active)
            return true;
        if (!matches(module_base + 0x01d86585, TURN_ANCHOR) ||
            !matches(module_base + 0x01d86c51, ARRIVAL_ANCHOR) ||
            !matches(module_base + 0x01d86c60, ORDER_ANCHOR))
            return false;
        if (!active && !platform::windows::write_code_memory(table_address, ENABLED_TABLE))
            return false;
        state.applied = true;
        return true;
    }

    if (!state.applied && original)
        return true;
    if (!active || !platform::windows::write_code_memory(table_address, ORIGINAL_TABLE))
        return false;
    state.applied = false;
    return true;
}

} // namespace s8::game
