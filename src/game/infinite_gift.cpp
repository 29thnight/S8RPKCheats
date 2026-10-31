#include "infinite_gift.h"
#include "find_bytes.h"
#include "signature_patterns.h"
#include "../platform/windows/process_memory.h"

#include <array>

namespace s8::game {
namespace {

constexpr std::array ORIGINAL_BYTES = signatures::INFINITE_GIFT;
constexpr std::array PATCHED_BYTES = signatures::INFINITE_GIFT_PATCHED;

} // namespace

bool set_infinite_gift(InfiniteGift& state, std::uintptr_t module_base,
                       std::span<const std::byte> image, bool enabled) {
    if (!module_base)
        return false;
    if (!state.code_address) {
        if (state.signature_prepared || image.empty()) {
            return false;
        }
        state.signature_prepared = true;

        std::optional<std::size_t> found = find_bytes(image, ORIGINAL_BYTES, 0x3000000);
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
