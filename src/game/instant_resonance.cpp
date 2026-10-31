#include "instant_resonance.h"
#include "find_bytes.h"
#include "bytecode_buffer.h"
#include "signature_patterns.h"
#include "../platform/windows/code_memory.h"
#include "../platform/windows/process_memory.h"

#include <cstring>
#include <algorithm>
#include <limits>

namespace s8::game {
namespace {

bool install_jump(std::uintptr_t address, std::uintptr_t cave, std::size_t size) {

    const std::int64_t delta = static_cast<std::int64_t>(cave) - static_cast<std::int64_t>(address + 5);

    if (delta < std::numeric_limits<std::int32_t>::min() || delta > std::numeric_limits<std::int32_t>::max()) {
        return false;
    }
    std::array<std::byte, 14> jump;

    if (size < 5 || size > jump.size()) {
        return false;
    }
    jump.fill(std::byte{0x90});
    jump[0] = std::byte{0xe9};

    const std::int32_t relative = static_cast<std::int32_t>(delta);

    std::memcpy(jump.data() + 1, &relative, sizeof(relative));
    return platform::windows::write_code_memory(address, std::span(jump).first(size));
}

} // namespace

bool set_instant_resonance(InstantResonance& state, std::uintptr_t module_base,
                           std::span<const std::byte> image, bool enabled) {
    if (!module_base) {
        return false;
    }
    if (!enabled) {
        bool restored = true;
        if (state.select_applied) {
            if (platform::windows::write_code_memory(state.select_address, state.select_original)) {
                state.select_applied = false;
            } else {
                restored = false;
            }
        }
        if (state.getter_applied) {
            if (platform::windows::write_code_memory(state.getter_address, state.getter_original)) {
                state.getter_applied = false;
            } else {
                restored = false;
            }
        }
        state.target_id = 0;
        state.locked_id = 0;
        return restored;
    }
    if (state.select_applied && state.getter_applied) {
        return true;
    }
    if (state.select_applied || state.getter_applied) {
        set_instant_resonance(state, module_base, {}, false);
        return false;
    }
    constexpr std::array SELECT_PATTERN = signatures::RESONANCE_SELECT;
    constexpr std::array GETTER_PATTERN = signatures::RESONANCE_GETTER;
    if (!state.signature_prepared && !image.empty()) {
        const std::optional<std::size_t> select = find_bytes(image, SELECT_PATTERN, image.size());
        const std::optional<std::size_t> getter = find_bytes(image, GETTER_PATTERN, image.size());

        state.select_address = select ? module_base + *select : 0;
        state.getter_address = getter ? module_base + *getter : 0;
        state.signature_prepared = true;
    }
    if (!state.select_address || !state.getter_address) {
        return false;
    }
    if (!platform::windows::read_memory(state.select_address, state.select_original) ||
        !platform::windows::read_memory(state.getter_address, state.getter_original) ||
        !std::equal(state.select_original.begin(), state.select_original.end(), SELECT_PATTERN.begin()) ||
        state.getter_original != GETTER_PATTERN) {
        return false;
    }
    if (!state.select_cave || !state.getter_cave) {
        state.select_cave = platform::windows::allocate_code_near(state.select_address, 192);
        state.getter_cave = platform::windows::allocate_code_near(state.getter_address, 256);
        if (!state.select_cave || !state.getter_cave) {
            if (state.select_cave) { platform::windows::free_code(state.select_cave); state.select_cave = 0; }
            if (state.getter_cave) { platform::windows::free_code(state.getter_cave); state.getter_cave = 0; }
            return false;
        }

        BytecodeBuffer<192> select_code;

        select_code.emit({0x9c,0x50,0x51,0x52,0x0f,0xb7,0x4e,0x08,0x48,0xb8});
        select_code.address(reinterpret_cast<std::uintptr_t>(&state.target_id));
        select_code.emit({0x0f,0xb7,0x10,0x66,0x89,0x08,0x66,0x3b,0xca,0x0f,0x85,0x0d,0,0,0,0x48,0xb8});
        select_code.address(reinterpret_cast<std::uintptr_t>(&state.locked_id));
        select_code.emit({0x66,0x89,0x08,0x5a,0x59,0x58,0x9d});
        select_code.append(state.select_original);
        select_code.emit({0xff,0x25,0,0,0,0});
        select_code.address(state.select_address + state.select_original.size());

        BytecodeBuffer<256> getter_code;

        getter_code.emit({0x9c,0x41,0x53,0x50,0x49,0xbb});
        getter_code.address(reinterpret_cast<std::uintptr_t>(&state.locked_id));
        getter_code.emit({0x66,0x41,0x83,0x3b,0,0x0f,0x84,0,0,0,0});

        const std::size_t no_lock = getter_code.size() - 4;

        getter_code.emit({0x66,0x41,0x3b,0x13,0x0f,0x85,0,0,0,0});

        const std::size_t different_id = getter_code.size() - 4;

        getter_code.append(state.getter_original);
        getter_code.emit({0x3c,0x04,0x73,0x09,0x41,0xc6,0x84,0x08});
        getter_code.append(std::span(state.getter_original).subspan(5));
        getter_code.emit({0x04,0x49,0xbb});
        getter_code.address(reinterpret_cast<std::uintptr_t>(&state.locked_id));
        getter_code.emit({0x66,0x41,0xc7,0x03,0,0});
        getter_code.patch(no_lock);
        getter_code.patch(different_id);
        getter_code.emit({0x58,0x41,0x5b,0x9d});
        getter_code.append(state.getter_original);
        getter_code.emit({0xff,0x25,0,0,0,0});
        getter_code.address(state.getter_address + state.getter_original.size());

        if (!select_code.valid() || !getter_code.valid() ||
            !platform::windows::write_code_memory(state.select_cave, select_code.bytes()) ||
            !platform::windows::write_code_memory(state.getter_cave, getter_code.bytes())) {
            platform::windows::free_code(state.select_cave);
            platform::windows::free_code(state.getter_cave);
            state.select_cave = 0;
            state.getter_cave = 0;
            return false;
        }
    }
    if (!install_jump(state.select_address, state.select_cave, state.select_original.size())) {
        return false;
    }
    state.select_applied = true;
    if (!install_jump(state.getter_address, state.getter_cave, state.getter_original.size())) {
        set_instant_resonance(state, module_base, {}, false);
        return false;
    }
    state.getter_applied = true;
    return true;
}

} // namespace s8::game
