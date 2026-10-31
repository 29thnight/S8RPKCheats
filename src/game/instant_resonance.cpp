#include "instant_resonance.h"
#include "find_bytes.h"
#include "../platform/windows/code_memory.h"
#include "../platform/windows/process_memory.h"

#include <cstring>
#include <initializer_list>
#include <limits>
#include <vector>

namespace s8::game {
namespace {

bool install_jump(std::uintptr_t address, std::uintptr_t cave, std::size_t size) {
    const auto delta = static_cast<std::int64_t>(cave) - static_cast<std::int64_t>(address + 5);
    if (delta < std::numeric_limits<std::int32_t>::min() || delta > std::numeric_limits<std::int32_t>::max()) {
        return false;
    }
    std::vector<std::byte> jump(size, std::byte{0x90});
    jump[0] = std::byte{0xe9};
    const auto relative = static_cast<std::int32_t>(delta);
    std::memcpy(jump.data() + 1, &relative, sizeof(relative));
    return platform::windows::write_code_memory(address, jump);
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
    if (state.select_cave) {
        state.retained_caves.push_back(state.select_cave);
        state.select_cave = 0;
    }
    if (state.getter_cave) {
        state.retained_caves.push_back(state.getter_cave);
        state.getter_cave = 0;
    }
    constexpr std::array SELECT_PATTERN{std::byte{0x8b},std::byte{0x86},std::byte{0x20},std::byte{0x03},
        std::byte{0},std::byte{0},std::byte{0xc1},std::byte{0xe8},std::byte{0x12},std::byte{0xa8},
        std::byte{0x01},std::byte{0x74},std::byte{0x0a}};
    constexpr std::array GETTER_PATTERN{std::byte{0x41},std::byte{0x0f},std::byte{0xb6},std::byte{0x84},
        std::byte{0x08},std::byte{0xda},std::byte{0x5b},std::byte{0},std::byte{0}};
    const auto select = find_bytes(image, SELECT_PATTERN, image.size());
    const auto getter = find_bytes(image, GETTER_PATTERN, image.size());
    if (!select || !getter) {
        return false;
    }
    state.select_address = module_base + *select;
    state.getter_address = module_base + *getter;
    if (!platform::windows::read_memory(state.select_address, state.select_original) ||
        !platform::windows::read_memory(state.getter_address, state.getter_original)) {
        return false;
    }
    state.select_cave = platform::windows::allocate_code_near(state.select_address, 192);
    state.getter_cave = platform::windows::allocate_code_near(state.getter_address, 256);
    if (!state.select_cave || !state.getter_cave) {
        if (state.select_cave) { platform::windows::free_code(state.select_cave); state.select_cave = 0; }
        if (state.getter_cave) { platform::windows::free_code(state.getter_cave); state.getter_cave = 0; }
        return false;
    }
    std::vector<std::byte> select_code;
    const auto emit_select = [&](std::initializer_list<unsigned char> bytes) {
        for (const auto byte : bytes) { select_code.push_back(static_cast<std::byte>(byte)); }
    };
    const auto append_select_address = [&](std::uintptr_t value) {
        const auto* bytes = reinterpret_cast<const std::byte*>(&value);
        select_code.insert(select_code.end(), bytes, bytes + sizeof(value));
    };
    emit_select({0x9c,0x50,0x51,0x52,0x0f,0xb7,0x4e,0x08,0x48,0xb8});
    append_select_address(reinterpret_cast<std::uintptr_t>(&state.target_id));
    emit_select({0x0f,0xb7,0x10,0x66,0x89,0x08,0x66,0x3b,0xca,0x0f,0x85,0x0d,0,0,0,0x48,0xb8});
    append_select_address(reinterpret_cast<std::uintptr_t>(&state.locked_id));
    emit_select({0x66,0x89,0x08,0x5a,0x59,0x58,0x9d});
    select_code.insert(select_code.end(), state.select_original.begin(), state.select_original.end());
    emit_select({0xff,0x25,0,0,0,0});
    append_select_address(state.select_address + state.select_original.size());

    std::vector<std::byte> getter_code;
    const auto emit_getter = [&](std::initializer_list<unsigned char> bytes) {
        for (const auto byte : bytes) { getter_code.push_back(static_cast<std::byte>(byte)); }
    };
    const auto append_getter_address = [&](std::uintptr_t value) {
        const auto* bytes = reinterpret_cast<const std::byte*>(&value);
        getter_code.insert(getter_code.end(), bytes, bytes + sizeof(value));
    };
    emit_getter({0x9c,0x41,0x53,0x50,0x49,0xbb});
    append_getter_address(reinterpret_cast<std::uintptr_t>(&state.locked_id));
    emit_getter({0x66,0x41,0x83,0x3b,0,0x0f,0x84,0,0,0,0});
    const auto no_lock = getter_code.size() - 4;
    emit_getter({0x66,0x41,0x3b,0x13,0x0f,0x85,0,0,0,0});
    const auto different_id = getter_code.size() - 4;
    getter_code.insert(getter_code.end(), state.getter_original.begin(), state.getter_original.end());
    emit_getter({0x3c,0x04,0x73,0x09,0x41,0xc6,0x84,0x08});
    getter_code.insert(getter_code.end(), state.getter_original.begin() + 5, state.getter_original.end());
    emit_getter({0x04,0x49,0xbb});
    append_getter_address(reinterpret_cast<std::uintptr_t>(&state.locked_id));
    emit_getter({0x66,0x41,0xc7,0x03,0,0});
    const auto patch_branch = [&](std::size_t position) {
        const auto delta = static_cast<std::int32_t>(getter_code.size() - position - 4);
        std::memcpy(getter_code.data() + position, &delta, sizeof(delta));
    };
    patch_branch(no_lock);
    patch_branch(different_id);
    emit_getter({0x58,0x41,0x5b,0x9d});
    getter_code.insert(getter_code.end(), state.getter_original.begin(), state.getter_original.end());
    emit_getter({0xff,0x25,0,0,0,0});
    append_getter_address(state.getter_address + state.getter_original.size());

    if (!platform::windows::write_code_memory(state.select_cave, select_code) ||
        !platform::windows::write_code_memory(state.getter_cave, getter_code)) {
        platform::windows::free_code(state.select_cave);
        platform::windows::free_code(state.getter_cave);
        state.select_cave = 0;
        state.getter_cave = 0;
        return false;
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
