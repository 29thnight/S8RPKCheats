#include "instant_loyalty.h"
#include "find_bytes.h"
#include "signature_patterns.h"
#include "../platform/windows/code_memory.h"
#include "../platform/windows/process_memory.h"

#include <array>
#include <cstring>
#include <limits>

namespace s8::game {

bool set_instant_loyalty(InstantLoyalty& state, std::uintptr_t module_base,
                         std::span<const std::byte> image, bool enabled) {
    if (!module_base) {
        return false;
    }
    if (!enabled) {
        if (!state.applied) {
            return true;
        }
        if (!platform::windows::write_code_memory(state.address, state.original)) {
            return false;
        }
        state.applied = false;
        return true;
    }
    if (state.applied) {
        return true;
    }
    constexpr std::array ORIGINAL = signatures::INSTANT_LOYALTY;
    if (!state.address) {
        if (state.signature_prepared || image.empty()) {
            return false;
        }
        state.signature_prepared = true;

        const std::optional<std::size_t> found = find_bytes(image, ORIGINAL, 0x3000000);
        if (!found) {
            return false;
        }
        state.address = module_base + *found;
    }
    if (!platform::windows::read_memory(state.address, state.original) || state.original != ORIGINAL) {
        return false;
    }
    if (!state.cave) {
        state.cave = platform::windows::allocate_code_near(state.address, 128);
        if (!state.cave) {
            return false;
        }
        std::array<std::byte, 28> code{
            std::byte{0xc6},std::byte{0x86},std::byte{0xec},std::byte{0},std::byte{0},std::byte{0},std::byte{0x64},
            std::byte{0x0f},std::byte{0xb6},std::byte{0x86},std::byte{0xec},std::byte{0},std::byte{0},std::byte{0},
            std::byte{0xff},std::byte{0x25},std::byte{0},std::byte{0},std::byte{0},std::byte{0}};
        const auto return_address = state.address + ORIGINAL.size();
        std::memcpy(code.data() + 20, &return_address, sizeof(return_address));
        if (!platform::windows::write_code_memory(state.cave, code)) {
            platform::windows::free_code(state.cave);
            state.cave = 0;
            return false;
        }
    }
    const auto delta = static_cast<std::int64_t>(state.cave) - static_cast<std::int64_t>(state.address + 5);
    if (delta < std::numeric_limits<std::int32_t>::min() || delta > std::numeric_limits<std::int32_t>::max()) {
        return false;
    }
    const auto displacement = static_cast<std::int32_t>(delta);
    std::array<std::byte, 7> jump{std::byte{0xe9}, {}, {}, {}, {}, std::byte{0x90}, std::byte{0x90}};
    std::memcpy(jump.data() + 1, &displacement, sizeof(displacement));
    if (!platform::windows::write_code_memory(state.address, jump)) {
        return false;
    }
    state.applied = true;
    return true;
}

} // namespace s8::game
