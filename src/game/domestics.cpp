#include "domestics.h"
#include "find_bytes.h"
#include "../platform/windows/code_memory.h"
#include "../platform/windows/process_memory.h"

#include <cstring>
#include <initializer_list>
#include <limits>

namespace s8::game {

bool set_domestics(Domestics& state, std::uintptr_t module_base,
                   std::span<const std::byte> image, bool enabled) {
    if (!enabled) {
        if (state.applied && !platform::windows::write_code_memory(state.address, state.original)) {
            return false;
        }
        state.applied = false;
        state.player = 0;
        state.force = 0;
        return true;
    }
    if (state.applied) {
        return true;
    }
    constexpr std::array pattern{std::byte{0x44}, std::byte{0x0f}, std::byte{0x4e},
        std::byte{0xf7}, std::byte{0x0f}, std::byte{0xb6}, std::byte{0x5c},
        std::byte{0x24}, std::byte{0x60}};
    const auto offset = find_bytes(image, pattern, image.size());
    if (!offset) {
        return false;
    }
    state.address = module_base + *offset;
    if (!platform::windows::read_memory(state.address, state.original) || state.original != pattern) {
        return false;
    }
    if (state.cave) {
        state.retained_caves.push_back(state.cave);
    }
    state.cave = platform::windows::allocate_code_near(state.address, 512);
    if (!state.cave) {
        return false;
    }
    state.player = 0;
    state.force = 0;
    std::vector<std::byte> code;
    const auto emit = [&](std::initializer_list<unsigned char> bytes) {
        for (const auto byte : bytes) {
            code.push_back(static_cast<std::byte>(byte));
        }
    };
    const auto address = [&](std::uintptr_t value) {
        const auto* bytes = reinterpret_cast<const std::byte*>(&value);
        code.insert(code.end(), bytes, bytes + sizeof(value));
    };
    const auto branch = [&]() {
        const auto position = code.size();
        emit({0, 0, 0, 0});
        return position;
    };
    const auto patch = [&](std::size_t position) {
        const auto delta = static_cast<std::int32_t>(code.size() - position - 4);
        std::memcpy(code.data() + position, &delta, sizeof(delta));
    };
    emit({0x44,0x0f,0x4e,0xf7,0x9c,0x50,0x53,0x4d,0x85,0xe4,0x0f,0x84});
    const auto null_player = branch();
    emit({0x48,0xb8}); address(reinterpret_cast<std::uintptr_t>(&state.player));
    emit({0x48,0x83,0x38,0,0x0f,0x85});
    const auto initialized = branch();
    emit({0x4c,0x89,0x20,0x49,0x8b,0x44,0x24,0x18,0x48,0xbb});
    address(reinterpret_cast<std::uintptr_t>(&state.force));
    emit({0x48,0x89,0x03});
    patch(initialized);
    emit({0x48,0xb8}); address(reinterpret_cast<std::uintptr_t>(&state.player));
    emit({0x48,0x8b,0x00,0x49,0x39,0xc4,0x0f,0x84});
    const auto player_match = branch();
    emit({0x49,0x8b,0x44,0x24,0x18,0x48,0xbb});
    address(reinterpret_cast<std::uintptr_t>(&state.force));
    emit({0x48,0x8b,0x1b,0x48,0x85,0xdb,0x0f,0x84});
    const auto null_force = branch();
    emit({0x48,0x39,0xd8,0x0f,0x84});
    const auto force_match = branch();
    emit({0xe9});
    const auto skip = branch();
    const auto multiply = [&](std::uintptr_t multiplier) {
        emit({0x48,0x83,0xec,0x10,0xf3,0x0f,0x7f,0x04,0x24,
              0xf3,0x41,0x0f,0x2a,0xc6,0x48,0xb8});
        address(multiplier);
        emit({0xf3,0x0f,0x59,0x00,0xf3,0x44,0x0f,0x2c,0xf0,
              0xf3,0x0f,0x6f,0x04,0x24,0x48,0x83,0xc4,0x10});
    };
    patch(player_match);
    multiply(reinterpret_cast<std::uintptr_t>(&state.multipliers.player));
    emit({0xe9});
    const auto player_done = branch();
    patch(force_match);
    multiply(reinterpret_cast<std::uintptr_t>(&state.multipliers.force));
    patch(null_player);
    patch(null_force);
    patch(skip);
    patch(player_done);
    emit({0x5b,0x58,0x9d,0x0f,0xb6,0x5c,0x24,0x60,0xff,0x25,0,0,0,0});
    address(state.address + state.original.size());
    if (!platform::windows::write_code_memory(state.cave, code)) {
        platform::windows::free_code(state.cave);
        state.cave = 0;
        return false;
    }
    const auto delta = static_cast<std::int64_t>(state.cave) - static_cast<std::int64_t>(state.address + 5);
    if (delta < std::numeric_limits<std::int32_t>::min() || delta > std::numeric_limits<std::int32_t>::max()) {
        return false;
    }
    std::array<std::byte, 9> jump{std::byte{0xe9},std::byte{},std::byte{},std::byte{},std::byte{},
        std::byte{0x90},std::byte{0x90},std::byte{0x90},std::byte{0x90}};
    const auto relative = static_cast<std::int32_t>(delta);
    std::memcpy(jump.data() + 1, &relative, sizeof(relative));
    if (!platform::windows::write_code_memory(state.address, jump)) {
        return false;
    }
    state.applied = true;
    return true;
}

} // namespace s8::game
