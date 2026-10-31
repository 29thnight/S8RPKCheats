#include "infinite_talk.h"
#include "find_bytes.h"
#include "signature_patterns.h"
#include "../platform/windows/code_memory.h"
#include "../platform/windows/process_memory.h"

#include <cstring>
#include <limits>

namespace s8::game {

bool set_infinite_talk(InfiniteTalk& state, std::uintptr_t module_base,
                       std::span<const std::byte> image, bool enabled) {
    if (!module_base)
        return false;
    constexpr std::array ORIGINAL = signatures::INFINITE_TALK;
    if (!state.hook) {
        if (state.signature_prepared || image.empty()) {
            return false;
        }
        state.signature_prepared = true;

        const std::optional<std::size_t> found = find_bytes(image, ORIGINAL, 0x3000000);
        if (!found)
            return false;
        state.hook = module_base + *found;
    }
    if (enabled == state.applied)
        return true;

    if (enabled) {
        if (!platform::windows::read_memory(state.hook, state.original))
            return false;
        if (!state.cave) {
            state.cave = platform::windows::allocate_code_near(state.hook, 128);
            if (!state.cave)
                return false;
            std::array<std::byte, 24> payload{
                std::byte{0x81}, std::byte{0xa7}, std::byte{0x20}, std::byte{0x03},
                std::byte{0x00}, std::byte{0x00}, std::byte{0xfb}, std::byte{0xff},
                std::byte{0xff}, std::byte{0xff}, std::byte{0xff}, std::byte{0x25},
                std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
            const auto resume = state.hook + 7;
            std::memcpy(payload.data() + 16, &resume, sizeof(resume));
            if (!platform::windows::write_code_memory(state.cave, payload)) {
                platform::windows::free_code(state.cave);
                state.cave = 0;
                return false;
            }
        }

        const auto relative = static_cast<std::int64_t>(state.cave) - static_cast<std::int64_t>(state.hook + 5);
        if (relative < std::numeric_limits<std::int32_t>::min() || relative > std::numeric_limits<std::int32_t>::max())
            return false;
        const auto displacement = static_cast<std::int32_t>(relative);
        std::array<std::byte, 7> jump{std::byte{0xe9}, {}, {}, {}, {}, std::byte{0x90}, std::byte{0x90}};
        std::memcpy(jump.data() + 1, &displacement, sizeof(displacement));
        if (!platform::windows::write_code_memory(state.hook, jump))
            return false;
        state.applied = true;
        return true;
    }

    if (!platform::windows::write_code_memory(state.hook, state.original))
        return false;
    state.applied = false;
    // The cave remains allocated while an already-running callback may return through it.
    return true;
}

} // namespace s8::game
