#include "infinite_gift.h"
#include "find_bytes.h"
#include "../platform/windows/process_memory.h"

#include <array>

namespace s8::game {
namespace {

constexpr std::array ORIGINAL_BYTES{
    std::byte{0x81}, std::byte{0x8e}, std::byte{0x20}, std::byte{0x03}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x04}, std::byte{0x00}, std::byte{0x00}};
constexpr std::array PATCHED_BYTES{
    std::byte{0x81}, std::byte{0xa6}, std::byte{0x20}, std::byte{0x03}, std::byte{0x00},
    std::byte{0x00}, std::byte{0xff}, std::byte{0xfb}, std::byte{0xff}, std::byte{0xff}};

} // namespace

bool set_infinite_gift(InfiniteGift& state, std::uintptr_t module_base,
                       std::span<const std::byte> image, bool enabled) {
    if (!module_base)
        return false;
    if (!state.code_address) {
        auto found = find_bytes(image, ORIGINAL_BYTES, 0x3000000);
        if (!found)
            found = find_bytes(image, PATCHED_BYTES, 0x3000000);
        if (found)
            state.code_address = module_base + *found;
    }
    if (!state.code_address)
        return false;

    std::array<std::byte, 10> current{};
    if (!platform::windows::read_memory(state.code_address, current))
        return false;
    if (enabled) {
        if (current == PATCHED_BYTES) {
            state.applied = true;
        } else if (current == ORIGINAL_BYTES &&
                   platform::windows::write_code_memory(state.code_address, PATCHED_BYTES)) {
            state.applied = true;
        } else {
            return false;
        }
    } else {
        if (current == PATCHED_BYTES && !platform::windows::write_code_memory(state.code_address, ORIGINAL_BYTES))
            return false;
        if (current != PATCHED_BYTES && current != ORIGINAL_BYTES)
            return false;
        state.applied = false;
    }
    return state.applied == enabled;
}

} // namespace s8::game
