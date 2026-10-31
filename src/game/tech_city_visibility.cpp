#include "tech_city_visibility.h"
#include "../platform/windows/process_memory.h"

#include <array>
#include <cstddef>

namespace s8::game {
namespace {

constexpr std::array ORIGINAL_SIGNATURE{
    std::byte{0x40}, std::byte{0x80}, std::byte{0xff}, std::byte{0x03}, std::byte{0x74}, std::byte{0x50},
    std::byte{0x0f}, std::byte{0xb6}, std::byte{0xcb}, std::byte{0xe8}, std::byte{0xc1}, std::byte{0xfa},
    std::byte{0x4e}, std::byte{0x00}};
constexpr std::array PATCHED_SIGNATURE{
    std::byte{0x40}, std::byte{0x80}, std::byte{0xff}, std::byte{0x03}, std::byte{0x90}, std::byte{0x90},
    std::byte{0x0f}, std::byte{0xb6}, std::byte{0xcb}, std::byte{0xe8}, std::byte{0xc1}, std::byte{0xfa},
    std::byte{0x4e}, std::byte{0x00}};
constexpr std::array ORIGINAL_BRANCH{std::byte{0x74}, std::byte{0x50}};
constexpr std::array PATCHED_BRANCH{std::byte{0x90}, std::byte{0x90}};

template <std::size_t N>
bool matches(std::uintptr_t address, const std::array<std::byte, N>& expected) {
    std::array<std::byte, N> current{};
    return platform::windows::read_memory(address, current) && current == expected;
}

} // namespace

bool set_tech_city_visibility(TechCityVisibility& state, std::uintptr_t module_base, bool enabled) {
    if (!module_base)
        return false;
    const auto signature = module_base + 0x012ca4f1;
    const auto branch = module_base + 0x012ca4f5;
    if (enabled == state.applied)
        return matches(signature, enabled ? PATCHED_SIGNATURE : ORIGINAL_SIGNATURE);

    if (enabled) {
        if (!matches(signature, ORIGINAL_SIGNATURE))
            return false;
        if (!platform::windows::write_code_memory(branch, PATCHED_BRANCH) || !matches(signature, PATCHED_SIGNATURE)) {
            if (matches(branch, PATCHED_BRANCH))
                platform::windows::write_code_memory(branch, ORIGINAL_BRANCH);
            return false;
        }
        state.applied = true;
        return true;
    }
    if (!matches(signature, PATCHED_SIGNATURE) || !platform::windows::write_code_memory(branch, ORIGINAL_BRANCH))
        return false;
    if (!matches(signature, ORIGINAL_SIGNATURE)) {
        if (matches(branch, ORIGINAL_BRANCH))
            state.applied = false;
        return false;
    }
    state.applied = false;
    return true;
}

} // namespace s8::game
