#include "domestics.h"
#include "find_bytes.h"
#include "bytecode_buffer.h"
#include "signature_patterns.h"
#include "../platform/windows/code_memory.h"
#include "../platform/windows/process_memory.h"

#include <cstring>
#include <initializer_list>
#include <limits>

namespace s8::game {
namespace {

void emit_multiplier(BytecodeBuffer<512>& code, std::uintptr_t multiplier) {
    code.emit({0x48,0x83,0xec,0x10,0xf3,0x0f,0x7f,0x04,0x24,
               0xf3,0x41,0x0f,0x2a,0xc6,0x48,0xb8});
    code.address(multiplier);
    code.emit({0xf3,0x0f,0x59,0x00,0xf3,0x44,0x0f,0x2c,0xf0,
               0xf3,0x0f,0x6f,0x04,0x24,0x48,0x83,0xc4,0x10});
}

} // namespace

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
    constexpr std::array pattern = signatures::DOMESTICS;
    if (!state.address) {
        if (state.signature_prepared || image.empty()) {
            return false;
        }
        state.signature_prepared = true;
        const std::optional<std::size_t> offset = find_bytes(image, pattern, image.size());

        if (!offset) {
            return false;
        }
        state.address = module_base + *offset;
    }
    if (!platform::windows::read_memory(state.address, state.original) || state.original != pattern) {
        return false;
    }
    state.player = 0;
    state.force = 0;
    if (!state.cave) {
        state.cave = platform::windows::allocate_code_near(state.address, 512);
        if (!state.cave) {
            return false;
        }

        BytecodeBuffer<512> code;

        code.emit({0x44,0x0f,0x4e,0xf7,0x9c,0x50,0x53,0x4d,0x85,0xe4,0x0f,0x84});

        const std::size_t null_player = code.branch();

        code.emit({0x48,0xb8});
        code.address(reinterpret_cast<std::uintptr_t>(&state.player));
        code.emit({0x48,0x83,0x38,0,0x0f,0x85});

        const std::size_t initialized = code.branch();

        code.emit({0x4c,0x89,0x20,0x49,0x8b,0x44,0x24,0x18,0x48,0xbb});
        code.address(reinterpret_cast<std::uintptr_t>(&state.force));
        code.emit({0x48,0x89,0x03});
        code.patch(initialized);
        code.emit({0x48,0xb8});
        code.address(reinterpret_cast<std::uintptr_t>(&state.player));
        code.emit({0x48,0x8b,0x00,0x49,0x39,0xc4,0x0f,0x84});

        const std::size_t player_match = code.branch();

        code.emit({0x49,0x8b,0x44,0x24,0x18,0x48,0xbb});
        code.address(reinterpret_cast<std::uintptr_t>(&state.force));
        code.emit({0x48,0x8b,0x1b,0x48,0x85,0xdb,0x0f,0x84});

        const std::size_t null_force = code.branch();

        code.emit({0x48,0x39,0xd8,0x0f,0x84});

        const std::size_t force_match = code.branch();

        code.emit({0xe9});

        const std::size_t skip = code.branch();

        code.patch(player_match);
        emit_multiplier(code, reinterpret_cast<std::uintptr_t>(&state.multipliers.player));
        code.emit({0xe9});

        const std::size_t player_done = code.branch();

        code.patch(force_match);
        emit_multiplier(code, reinterpret_cast<std::uintptr_t>(&state.multipliers.force));
        code.patch(null_player);
        code.patch(null_force);
        code.patch(skip);
        code.patch(player_done);
        code.emit({0x5b,0x58,0x9d,0x0f,0xb6,0x5c,0x24,0x60,0xff,0x25,0,0,0,0});
        code.address(state.address + state.original.size());
        if (!code.valid() || !platform::windows::write_code_memory(state.cave, code.bytes())) {
            platform::windows::free_code(state.cave);
            state.cave = 0;
            return false;
        }
    }

    const std::int64_t delta = static_cast<std::int64_t>(state.cave) - static_cast<std::int64_t>(state.address + 5);

    if (delta < std::numeric_limits<std::int32_t>::min() || delta > std::numeric_limits<std::int32_t>::max()) {
        return false;
    }
    std::array<std::byte, 9> jump{std::byte{0xe9},std::byte{},std::byte{},std::byte{},std::byte{},
        std::byte{0x90},std::byte{0x90},std::byte{0x90},std::byte{0x90}};

    const std::int32_t relative = static_cast<std::int32_t>(delta);

    std::memcpy(jump.data() + 1, &relative, sizeof(relative));
    if (!platform::windows::write_code_memory(state.address, jump)) {
        return false;
    }
    state.applied = true;
    return true;
}

} // namespace s8::game
