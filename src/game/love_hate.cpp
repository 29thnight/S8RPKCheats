#include "love_hate.h"
#include "love_hate_targets.h"
#include "../platform/windows/code_memory.h"
#include "../platform/windows/process_memory.h"

#include <cstring>
#include <initializer_list>
#include <limits>
#include <vector>

namespace s8::game {
namespace {

bool install_jump(std::uintptr_t target, std::uintptr_t cave, std::size_t length) {
    const auto delta = static_cast<std::int64_t>(cave) - static_cast<std::int64_t>(target + 5);
    if (delta < std::numeric_limits<std::int32_t>::min() || delta > std::numeric_limits<std::int32_t>::max()) {
        return false;
    }
    std::vector<std::byte> jump(length, std::byte{0x90});
    jump[0] = std::byte{0xe9};
    const auto displacement = static_cast<std::int32_t>(delta);
    std::memcpy(jump.data() + 1, &displacement, sizeof(displacement));
    return platform::windows::write_code_memory(target, jump);
}

} // namespace

bool set_love_hate(LoveHate& state, std::uintptr_t module_base, std::span<const std::byte> image, bool enabled) {
    if (!module_base) {
        return false;
    }
    if (!enabled) {
        bool restored = true;
        if (state.affinity_applied) {
            if (platform::windows::write_code_memory(state.affinity_address, state.affinity_original)) {
                state.affinity_applied = false;
            } else {
                restored = false;
            }
        }
        if (state.relationship_applied) {
            if (platform::windows::write_code_memory(state.relationship_address, state.relationship_original)) {
                state.relationship_applied = false;
            } else {
                restored = false;
            }
        }
        return restored;
    }
    if (!state.signature_prepared && !image.empty()) {
        const LoveHateTargets targets = find_love_hate_targets(image);

        state.affinity_address = targets.affinity ? module_base + *targets.affinity : 0;
        state.relationship_address = targets.relationship ? module_base + *targets.relationship : 0;
        state.relationship_offset = targets.relationship_offset.value_or(0);
        state.signature_prepared = true;
    }
    if (!state.affinity_address || !state.relationship_address) {
        return false;
    }
    if (!state.relationship_applied) {
        if (platform::windows::read_memory(state.relationship_address, state.relationship_original)) {
            if (!state.relationship_cave) {
                state.relationship_cave = platform::windows::allocate_code_near(state.relationship_address, 256);
            }
            if (state.relationship_cave) {
                std::vector<std::byte> code(state.relationship_original.begin(), state.relationship_original.begin() + 9);
                const auto emit = [&](std::initializer_list<unsigned char> bytes) {
                    for (const auto byte : bytes) { code.push_back(static_cast<std::byte>(byte)); }
                };
                emit({0x74, 0x09, 0x42, 0xc6, 0x84, 0x07});
                const auto* offset = reinterpret_cast<const std::byte*>(&state.relationship_offset);
                code.insert(code.end(), offset, offset + sizeof(state.relationship_offset));
                emit({0x64, 0xff, 0x25, 0, 0, 0, 0});
                const auto back = state.relationship_address + 11;
                const auto* back_bytes = reinterpret_cast<const std::byte*>(&back);
                code.insert(code.end(), back_bytes, back_bytes + sizeof(back));
                if (platform::windows::write_code_memory(state.relationship_cave, code) &&
                    install_jump(state.relationship_address, state.relationship_cave, 11)) {
                    state.relationship_applied = true;
                }
            }
        }
    }
    if (!state.affinity_applied) {
        if (platform::windows::read_memory(state.affinity_address, state.affinity_original)) {
            if (!state.affinity_cave) {
                state.affinity_cave = platform::windows::allocate_code_near(state.affinity_address, 512);
            }
            if (state.affinity_cave) {
                std::vector<std::byte> code;
                const auto emit = [&](std::initializer_list<unsigned char> bytes) {
                    for (const auto byte : bytes) { code.push_back(static_cast<std::byte>(byte)); }
                };
                emit({0x4c,0x8b,0xd1,0x0f,0xb6,0x8c,0x18,0xf0,0,0,0,
                      0x4c,0x8d,0x9c,0x18,0xf0,0,0,0,0x49,0xbb});
                const auto capture = reinterpret_cast<std::uintptr_t>(&state.captured_affinity);
                const auto* capture_bytes = reinterpret_cast<const std::byte*>(&capture);
                code.insert(code.end(), capture_bytes, capture_bytes + sizeof(capture));
                emit({0x4d,0x89,0x1b,0x83,0xf9,0x08,0x76,0});
                const auto first_branch = code.size() - 1;
                emit({0x83,0xf9,0x63,0x72,0});
                const auto second_branch = code.size() - 1;
                emit({0xb9,0x64,0,0,0,
                      0x88,0x8c,0x18,0xf0,0,0,0,0x88,0x8c,0x18});
                const auto* offset = reinterpret_cast<const std::byte*>(&state.relationship_offset);
                code.insert(code.end(), offset, offset + sizeof(state.relationship_offset));
                code[first_branch] = static_cast<std::byte>(code.size() - first_branch - 1);
                code[second_branch] = static_cast<std::byte>(code.size() - second_branch - 1);
                emit({0x0f,0xb6,0x84,0x18,0xf0,0,0,0,0x4c,0x8b,0xca,0xff,0x25,0,0,0,0});
                const auto back = state.affinity_address + 8;
                const auto* back_bytes = reinterpret_cast<const std::byte*>(&back);
                code.insert(code.end(), back_bytes, back_bytes + sizeof(back));
                if (platform::windows::write_code_memory(state.affinity_cave, code) &&
                    install_jump(state.affinity_address, state.affinity_cave, 8)) {
                    state.affinity_applied = true;
                }
            }
        }
    }
    return state.affinity_applied && state.relationship_applied;
}

} // namespace s8::game
