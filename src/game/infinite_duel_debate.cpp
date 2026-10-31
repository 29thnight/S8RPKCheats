#include "infinite_duel_debate.h"
#include "find_bytes.h"
#include "../platform/windows/code_memory.h"
#include "../platform/windows/process_memory.h"

#include <array>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <vector>

namespace s8::game {
namespace {

constexpr std::array SEARCH_BYTES{
    std::byte{0x40}, std::byte{0x0f}, std::byte{0xb6}, std::byte{0xc5},
    std::byte{0xc1}, std::byte{0xe0}, std::byte{0x08}, std::byte{0x05},
    std::byte{0x00}, std::byte{0x01}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x09}, std::byte{0x87}, std::byte{0x20}, std::byte{0x03},
    std::byte{0x00}, std::byte{0x00}};
constexpr std::array ORIGINAL_BYTES{
    std::byte{0x09}, std::byte{0x87}, std::byte{0x20},
    std::byte{0x03}, std::byte{0x00}, std::byte{0x00}};
constexpr std::size_t DUEL_FLAG_OFFSET = 0x90;
constexpr std::size_t DEBATE_FLAG_OFFSET = 0x91;

bool write_flags(const InfiniteDuelDebate& state) {
    const std::array values{static_cast<std::byte>(state.duel), static_cast<std::byte>(state.debate)};
    return platform::windows::write_memory(state.cave + DUEL_FLAG_OFFSET, values);
}

bool install(InfiniteDuelDebate& state, std::uintptr_t module_base, std::span<const std::byte> image) {
    if (state.applied)
        return write_flags(state);
    if (!state.hook) {
        const auto found = find_bytes(image, SEARCH_BYTES, 0x3000000);
        if (!found)
            return false;
        state.hook = module_base + *found + 12;
    }
    std::array<std::byte, 6> current{};
    if (!platform::windows::read_memory(state.hook, current) || current != ORIGINAL_BYTES)
        return false;

    if (!state.cave) {
        state.cave = platform::windows::allocate_code_near(state.hook, 160);
        if (!state.cave)
            return false;
        std::vector<std::byte> code;
        const auto emit = [&](std::initializer_list<unsigned char> bytes) {
            for (const auto value : bytes)
                code.push_back(static_cast<std::byte>(value));
        };
        const auto emit_address = [&](std::uintptr_t address) {
            const auto* bytes = reinterpret_cast<const std::byte*>(&address);
            code.insert(code.end(), bytes, bytes + sizeof(address));
        };
        emit({0x50, 0x41, 0x53, 0x49, 0xbb});
        emit_address(state.cave + DUEL_FLAG_OFFSET);
        emit({0x41, 0x80, 0x3b, 0x00, 0x74, 0x05, 0x25, 0xff, 0xfe, 0xff, 0xff});
        emit({0x49, 0xbb});
        emit_address(state.cave + DEBATE_FLAG_OFFSET);
        emit({0x41, 0x80, 0x3b, 0x00, 0x74, 0x05, 0x25, 0xff, 0xfd, 0xff, 0xff});
        emit({0x09, 0x87, 0x20, 0x03, 0x00, 0x00, 0x41, 0x5b, 0x58});
        emit({0xff, 0x25, 0x00, 0x00, 0x00, 0x00});
        emit_address(state.hook + ORIGINAL_BYTES.size());
        if (!platform::windows::write_code_memory(state.cave, code)) {
            platform::windows::free_code(state.cave);
            state.cave = 0;
            return false;
        }
    }

    if (!write_flags(state))
        return false;
    const auto relative = static_cast<std::int64_t>(state.cave) - static_cast<std::int64_t>(state.hook + 5);
    if (relative < std::numeric_limits<std::int32_t>::min() || relative > std::numeric_limits<std::int32_t>::max())
        return false;
    const auto displacement = static_cast<std::int32_t>(relative);
    std::array<std::byte, 6> jump{std::byte{0xe9}, {}, {}, {}, {}, std::byte{0x90}};
    std::memcpy(jump.data() + 1, &displacement, sizeof(displacement));
    if (!platform::windows::write_code_memory(state.hook, jump))
        return false;
    state.applied = true;
    return true;
}

} // namespace

bool set_infinite_duel_debate(InfiniteDuelDebate& state, std::uintptr_t module_base,
    std::span<const std::byte> image, DuelDebateFeature feature, bool enabled) {
    bool& flag = feature == DuelDebateFeature::Duel ? state.duel : state.debate;
    const bool previous = flag;
    flag = enabled;
    if (enabled) {
        if (!module_base || !install(state, module_base, image)) {
            flag = previous;
            return false;
        }
        return true;
    }
    if (state.cave && !write_flags(state)) {
        flag = previous;
        return false;
    }
    if (!state.duel && !state.debate && state.applied) {
        if (!platform::windows::write_code_memory(state.hook, ORIGINAL_BYTES))
            return false;
        state.applied = false;
        // The cave and its flag bytes remain valid for any in-flight execution.
    }
    return true;
}

} // namespace s8::game
