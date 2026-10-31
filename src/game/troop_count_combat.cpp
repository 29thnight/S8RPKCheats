#include "troop_count_combat.h"
#include "../platform/windows/process_memory.h"

#include <array>
#include <cstddef>

namespace s8::game {
namespace {

constexpr std::array ORIGINAL_SIGNATURE{
    std::byte{0x3b}, std::byte{0xfd}, std::byte{0x7e}, std::byte{0x57}, std::byte{0x80},
    std::byte{0x7c}, std::byte{0x24}, std::byte{0x70}, std::byte{0x00}, std::byte{0x75}, std::byte{0x50},
};
constexpr std::array PATCHED_SIGNATURE{
    std::byte{0x3b}, std::byte{0xfd}, std::byte{0xeb}, std::byte{0x57}, std::byte{0x80},
    std::byte{0x7c}, std::byte{0x24}, std::byte{0x70}, std::byte{0x00}, std::byte{0x75}, std::byte{0x50},
};
constexpr std::array ORIGINAL_BRANCH{std::byte{0x7e}, std::byte{0x57}};
constexpr std::array PATCHED_BRANCH{std::byte{0xeb}, std::byte{0x57}};

bool matches_signature(std::uintptr_t address, const std::array<std::byte, 11>& expected) {
    std::array<std::byte, 11> actual{};
    return platform::windows::read_memory(address, actual) && actual == expected;
}

} // namespace

bool set_troop_count_combat(TroopCountCombat& state, std::uintptr_t module_base, bool enabled) {
    if (!module_base)
        return false;
    const auto signature_address = module_base + 0x01e2d9ff;
    const auto branch_address = module_base + 0x01e2da01;
    if (enabled) {
        if (state.applied)
            return matches_signature(signature_address, PATCHED_SIGNATURE);
        if (!matches_signature(signature_address, ORIGINAL_SIGNATURE))
            return false;
        if (!platform::windows::write_code_memory(branch_address, PATCHED_BRANCH) ||
            !matches_signature(signature_address, PATCHED_SIGNATURE)) {
            platform::windows::write_code_memory(branch_address, ORIGINAL_BRANCH);
            return false;
        }
        state.applied = true;
        return true;
    }
    if (!state.applied)
        return true;
    if (!matches_signature(signature_address, PATCHED_SIGNATURE) ||
        !platform::windows::write_code_memory(branch_address, ORIGINAL_BRANCH) ||
        !matches_signature(signature_address, ORIGINAL_SIGNATURE))
        return false;
    state.applied = false;
    return true;
}

} // namespace s8::game
