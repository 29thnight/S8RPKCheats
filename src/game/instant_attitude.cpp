#include "instant_attitude.h"
#include "find_bytes.h"
#include "../platform/windows/process_memory.h"

#include <array>

namespace s8::game {

bool set_instant_attitude(InstantAttitude& state, std::uintptr_t module_base,
                          std::span<const std::byte> image, bool enabled) {
    if (!module_base)
        return false;
    constexpr std::array ORIGINAL{
        std::byte{0x44}, std::byte{0x88}, std::byte{0x44}, std::byte{0x07}, std::byte{0x70}};
    constexpr std::array PATCHED{
        std::byte{0xc6}, std::byte{0x44}, std::byte{0x07}, std::byte{0x70}, std::byte{0x64}};
    if (!state.address) {
        const auto found = find_bytes(image, ORIGINAL, 0x2000000);
        if (!found)
            return false;
        state.address = module_base + *found;
    }
    return platform::windows::write_code_memory(state.address, enabled ? PATCHED : ORIGINAL);
}

} // namespace s8::game
