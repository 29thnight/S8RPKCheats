#include "fast_relationship.h"

#include "find_bytes.h"
#include "../platform/windows/process_memory.h"

#include <array>

namespace s8::game {

bool set_fast_relationship(FastRelationship& state, std::uintptr_t module_base,
                           std::span<const std::byte> image, bool enabled) {
    constexpr std::array ORIGINAL{
        std::byte{0x0f}, std::byte{0xb6}, std::byte{0xcb}, std::byte{0x3a},
        std::byte{0xda}, std::byte{0x0f}, std::byte{0x4f}, std::byte{0xca}};
    constexpr std::array PATCHED{
        std::byte{0x0f}, std::byte{0xb6}, std::byte{0xca}, std::byte{0x88},
        std::byte{0xd3}, std::byte{0x90}, std::byte{0x90}, std::byte{0x90}};
    if (!module_base) {
        return false;
    }
    if (!state.address) {
        const auto offset = find_bytes(image, ORIGINAL, image.size());
        if (!offset) {
            return false;
        }
        state.address = module_base + *offset;
    }
    std::array<std::byte, ORIGINAL.size()> current{};
    if (!platform::windows::read_memory(state.address, current)) {
        return false;
    }
    const auto& target = enabled ? PATCHED : ORIGINAL;
    const auto& previous = enabled ? ORIGINAL : PATCHED;
    if (current != target && (current != previous || !platform::windows::write_code_memory(state.address, target))) {
        return false;
    }
    state.applied = enabled;
    return true;
}

} // namespace s8::game
