#include "marriage_condition.h"
#include "find_bytes.h"
#include "../platform/windows/process_memory.h"

namespace s8::game {

bool set_marriage_condition(MarriageCondition& state, std::uintptr_t module_base,
                            std::span<const std::byte> image, bool enabled) {
    if (!module_base) {
        return false;
    }
    if (!state.address) {
        constexpr std::array PATTERN{
            std::byte{0x48}, std::byte{0x8b}, std::byte{0x47}, std::byte{0x10},
            std::byte{0x4c}, std::byte{0x3b}, std::byte{0xf0}};
        const auto found = find_bytes(image, PATTERN, 0x3000000);
        if (found && *found >= 2 && (image[*found - 2] == std::byte{0x75} || image[*found - 2] == std::byte{0xeb}))
            state.address = module_base + *found - 2;
        if (!state.address) {
            return false;
        }
    }

    std::array<std::byte, 2> current{};
    if (!platform::windows::read_memory(state.address, current)) {
        state.address = 0;
        state.original_saved = false;
        return false;
    }
    const bool actual = current[0] == std::byte{0xeb};
    if (actual == enabled) {
        state.applied = actual;
        return true;
    }
    if (!state.original_saved) {
        if (current[0] != std::byte{0x75}) {
            state.applied = actual;
            return false;
        }
        state.original = current;
        state.original_saved = true;
    }
    const auto patch = enabled ? std::array{std::byte{0xeb}, current[1]} : state.original;
    if (!platform::windows::write_code_memory(state.address, patch)) {
        state.applied = actual;
        return false;
    }
    if (!platform::windows::read_memory(state.address, current)) {
        return false;
    }
    state.applied = current[0] == std::byte{0xeb};
    return state.applied == enabled;
}

} // namespace s8::game
