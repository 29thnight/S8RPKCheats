#include "road_block.h"

#include "../platform/windows/process_memory.h"

#include <span>

namespace s8::game {
namespace {

std::uintptr_t road_root(std::uintptr_t module_base) {
    if (!module_base) {
        return 0;
    }
    std::uintptr_t root = module_base + 0x034c8630;
    constexpr std::array<std::uintptr_t, 5> CHAIN{0, 0, 8, 0x10, 0};

    for (const std::uintptr_t offset : CHAIN) {
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

void abandon_road_backup(RoadBlock& state) {
    state.original = {};
    state.root = 0;
    state.epoch = {};
    state.applied = false;
}

bool capture_road_backup(RoadBlock& state, std::uintptr_t root, bool second, domain::GameEpoch epoch) {
    const std::array<std::uintptr_t, 2> addresses = offsets(second);
    std::array<std::uint64_t, 2> original{};

    for (std::size_t i = 0; i < addresses.size(); ++i) {
        if (!platform::windows::read_memory(root + addresses[i],
                                             std::as_writable_bytes(std::span(&original[i], 1)))) {
            return false;
        }
    }
    state.original = original;
    state.root = root;
    state.epoch = epoch;
    state.applied = true;
    return true;
}

bool restore_road_backup(RoadBlock& state, bool second) {
    const std::array<std::uintptr_t, 2> addresses = offsets(second);
    bool restored = true;

    for (std::size_t i = 0; i < addresses.size(); ++i) {
        const bool written = platform::windows::write_data_memory(state.root + addresses[i],
            std::as_bytes(std::span(&state.original[i], 1)));

        restored = written && restored;
    }
    if (restored) {
        abandon_road_backup(state);
    }
    return restored;
}

bool synchronize_road_backup(RoadBlock& state, std::uintptr_t root, bool second, domain::GameEpoch epoch) {
    if (state.applied && (state.root != root || state.epoch != epoch)) {
        // A new object owns these offsets. Never copy the previous object's values into it.
        abandon_road_backup(state);
    }
    if (!state.requested) {
        return !state.applied || restore_road_backup(state, second);
    }
    if (!root) {
        return false;
    }
    return state.applied || capture_road_backup(state, root, second, epoch);
}

} // namespace

bool set_road_block(RoadBlock& state, std::uintptr_t module_base, bool second, bool enabled,
                    domain::GameEpoch epoch) {
    const std::uintptr_t root = road_root(module_base);
    const bool previous_request = state.requested;

    state.requested = enabled;
    state.last_tick_ms = 0;
    if (!synchronize_road_backup(state, root, second, epoch)) {
        state.requested = previous_request;
        return false;
    }
    return true;
}

void tick_road_block(RoadBlock& state, std::uintptr_t module_base, bool second, std::uint64_t now_ms,
                     domain::GameEpoch epoch) {
    if ((!state.requested && !state.applied) || now_ms - state.last_tick_ms < 100) {
        return;
    }

    const std::uintptr_t root = road_root(module_base);
    constexpr std::uint64_t ZERO = 0;

    state.last_tick_ms = now_ms;
    if (!synchronize_road_backup(state, root, second, epoch) || !state.requested || !state.applied) {
        return;
    }
    for (const std::uintptr_t offset : offsets(second)) {
        std::uint64_t value = 0;

        if (platform::windows::read_memory(root + offset, std::as_writable_bytes(std::span(&value, 1))) && value) {
            platform::windows::write_data_memory(root + offset, std::as_bytes(std::span(&ZERO, 1)));
        }
    }
}

} // namespace s8::game
