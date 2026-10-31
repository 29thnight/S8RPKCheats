#include "domestic_reward.h"
#include "../platform/windows/code_memory.h"
#include "../platform/windows/process_memory.h"

#include <cstring>
#include <initializer_list>
#include <limits>
#include <vector>

namespace s8::game {

bool set_domestic_reward(DomesticReward& state, std::uintptr_t module_base,
                         domain::DomesticRewardMode mode) {
    if (!module_base) {
        return false;
    }
    constexpr std::array original{std::byte{0x48}, std::byte{0x89}, std::byte{0x74},
                                  std::byte{0x24}, std::byte{0x30}};
    const auto hook = module_base + 0x01c813d1;
    std::array<std::byte, 5> current{};
    if (mode == domain::DomesticRewardMode::Off) {
        if (!state.applied) {
            state.mode.store(0);
            return true;
        }
        if (!platform::windows::read_memory(hook, current) || current != state.jump) {
            return false;
        }
        const auto previous = state.mode.exchange(0);
        if (!platform::windows::write_code_memory(hook, original) ||
            !platform::windows::read_memory(hook, current) || current != original) {
            state.mode.store(previous);
            return false;
        }
        state.applied = false;
        return true;
    }
    if (state.applied) {
        state.mode.store(static_cast<std::int32_t>(mode));
        return true;
    }
    if (!platform::windows::read_memory(hook, current) || current != original) {
        return false;
    }
    if (!state.cave) {
        state.cave = platform::windows::allocate_code_near(hook, 256);
        if (!state.cave) {
            return false;
        }
    }
    const auto distance = static_cast<std::int64_t>(state.cave) - static_cast<std::int64_t>(hook + 5);
    if (distance < std::numeric_limits<std::int32_t>::min() ||
        distance > std::numeric_limits<std::int32_t>::max()) {
        return false;
    }
    state.jump[0] = std::byte{0xe9};
    const auto relative = static_cast<std::int32_t>(distance);
    std::memcpy(state.jump.data() + 1, &relative, sizeof(relative));
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
    const auto branch = [&](unsigned char opcode) {
        emit({0x0f, opcode, 0, 0, 0, 0});
        return code.size() - 4;
    };
    const auto patch = [&](std::size_t position) {
        const auto offset = static_cast<std::int32_t>(code.size() - position - 4);
        std::memcpy(code.data() + position, &offset, sizeof(offset));
    };
    emit({0x9c,0x50,0x4d,0x85,0xed});
    const auto null_target = branch(0x84);
    emit({0x4c,0x3b,0x6d,0xb8});
    const auto self_target = branch(0x84);
    emit({0x49,0x8b,0x45,0x20,0x48,0x3b,0x45,0x98});
    const auto wrong_owner = branch(0x85);
    emit({0x41,0x80,0xbd,0xf8,0,0,0,0});
    const auto inactive = branch(0x84);
    emit({0x41,0x81,0xbd,0x48,0x03,0,0,0xc8,0,0,0});
    const auto insufficient = branch(0x8c);
    emit({0x48,0xb8});
    address(reinterpret_cast<std::uintptr_t>(&state.mode));
    emit({0x8b,0x00,0x83,0xf8,0x02});
    const auto any_rank = branch(0x84);
    emit({0x83,0xf8,0x01});
    const auto wrong_mode = branch(0x85);
    emit({0x41,0x8b,0x85,0x4c,0x03,0,0,0xff,0xc8,0x83,0xf8,0x02});
    const auto wrong_rank = branch(0x87);
    patch(any_rank);
    emit({0x4c,0x89,0xee});
    patch(null_target);
    patch(self_target);
    patch(wrong_owner);
    patch(inactive);
    patch(insufficient);
    patch(wrong_mode);
    patch(wrong_rank);
    emit({0x58,0x9d});
    code.insert(code.end(), original.begin(), original.end());
    emit({0xff,0x25,0,0,0,0});
    address(hook + original.size());
    if (!platform::windows::write_code_memory(state.cave, code)) {
        return false;
    }
    state.mode.store(static_cast<std::int32_t>(mode));
    if (!platform::windows::read_memory(hook, current) || current != original) {
        state.mode.store(0);
        return false;
    }
    if (!platform::windows::write_code_memory(hook, state.jump)) {
        if (platform::windows::read_memory(hook, current) && current == state.jump) {
            platform::windows::write_code_memory(hook, original);
        }
        state.mode.store(0);
        return false;
    }
    if (!platform::windows::read_memory(hook, current) || current != state.jump) {
        state.mode.store(0);
        return false;
    }
    state.applied = true;
    return true;
}

} // namespace s8::game
