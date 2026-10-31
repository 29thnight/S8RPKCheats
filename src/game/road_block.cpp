#include "road_block.h"

#include "../platform/windows/process_memory.h"

#include <span>

namespace s8::game {
namespace {

std::uintptr_t road_root(std::uintptr_t module_base) {
    if (!module_base) {
        return 0;
    }
    auto root = module_base + 0x034c8630;
    constexpr std::array<std::uintptr_t, 5> CHAIN{0, 0, 8, 0x10, 0};
    for (const auto offset : CHAIN) {
        std::uintptr_t next = 0;
        if (!platform::windows::read_memory(root + offset, std::as_writable_bytes(std::span(&next, 1))) || !next) {
            return 0;
        }
        root = next;
    }
    return root;
}

constexpr std::array<std::uintptr_t, 2> offsets(bool second) {
    return second ? std::array<std::uintptr_t, 2>{0x8370, 0x7648}
                  : std::array<std::uintptr_t, 2>{0x7e30, 0x8368};
}

} // namespace

bool set_road_block(RoadBlock& state, std::uintptr_t module_base, bool second, bool enabled) {
    if (state.applied == enabled) {
        return true;
    }
    const auto root = road_root(module_base);
    if (!root) {
        return false;
    }
    const auto addresses = offsets(second);
    if (enabled) {
        std::array<std::uint64_t, 2> original{};
        for (std::size_t i = 0; i < addresses.size(); ++i) {
            if (!platform::windows::read_memory(root + addresses[i], std::as_writable_bytes(std::span(&original[i], 1)))) {
                return false;
            }
        }
        state.original = original;
        state.applied = true;
        state.last_tick_ms = 0;
        return true;
    }
    for (std::size_t i = 0; i < addresses.size(); ++i) {
        if (!platform::windows::write_data_memory(root + addresses[i], std::as_bytes(std::span(&state.original[i], 1)))) {
            return false;
        }
    }
    state.applied = false;
    return true;
}

void tick_road_block(RoadBlock& state, std::uintptr_t module_base, bool second, std::uint64_t now_ms) {
    if (!state.applied || now_ms - state.last_tick_ms < 100) {
        return;
    }
    state.last_tick_ms = now_ms;
    const auto root = road_root(module_base);
    if (!root) {
        return;
    }
    constexpr std::uint64_t ZERO = 0;
    for (const auto offset : offsets(second)) {
        std::uint64_t value = 0;
        if (platform::windows::read_memory(root + offset, std::as_writable_bytes(std::span(&value, 1))) && value) {
            platform::windows::write_data_memory(root + offset, std::as_bytes(std::span(&ZERO, 1)));
        }
    }
}

} // namespace s8::game
