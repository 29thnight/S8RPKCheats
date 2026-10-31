#include "instant_attitude.h"
#include "find_bytes.h"
#include "signature_patterns.h"
#include "../platform/windows/process_memory.h"

#include <array>

namespace s8::game {

bool set_instant_attitude(InstantAttitude& state, std::uintptr_t module_base,
                          std::span<const std::byte> image, bool enabled) {
    if (!module_base) {
        return false;
    }
    constexpr std::array ORIGINAL = signatures::INSTANT_ATTITUDE;
    constexpr std::array PATCHED{
        std::byte{0xc6}, std::byte{0x44}, std::byte{0x07}, std::byte{0x70}, std::byte{0x64}};

    if (!state.address) {
        if (state.signature_prepared || image.empty()) {
            return false;
        }
        state.signature_prepared = true;

        const std::optional<std::size_t> found = find_bytes(image, ORIGINAL, 0x2000000);

        if (!found) {
            return false;
        }
        state.address = module_base + *found;
    }
    std::array<std::byte, ORIGINAL.size()> current{};
    const std::array& target = enabled ? PATCHED : ORIGINAL;

    if (!platform::windows::read_memory(state.address, current) ||
        (current != ORIGINAL && current != PATCHED)) {
        return false;
    }
    if (current == target) {
        return true;
    }
    return platform::windows::write_code_memory(state.address, target) &&
        platform::windows::read_memory(state.address, current) && current == target;
}

} // namespace s8::game
