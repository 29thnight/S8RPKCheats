#include "battle_map_shuffle.h"
#include "battle_map_offsets.h"
#include "../platform/windows/process_memory.h"

#include <algorithm>
#include <array>
#include <span>

namespace s8::game {
namespace {

std::uintptr_t resolve_base(BattleMapShuffle& state, std::uintptr_t module_base, std::uint64_t now_ms) {
    if (state.cached_base && now_ms - state.last_resolve_ms < 2000) {
        return state.cached_base;
    }
    state.cached_base = 0;
    state.last_resolve_ms = now_ms;
    if (!module_base) {
        return 0;
    }
    auto address = module_base + 0x034c8630;
    constexpr std::array<std::uintptr_t, 5> CHAIN{0, 0, 8, 0x10, 0};
    for (const auto offset : CHAIN) {
        std::uintptr_t next = 0;
        if (!platform::windows::read_memory(address + offset, std::as_writable_bytes(std::span(&next, 1))) || !next) {
            return 0;
        }
        address = next;
    }
    state.cached_base = address;
    return address;
}

} // namespace

void disable_battle_map_shuffle(BattleMapShuffle& state) {
    if (!state.active) {
        return;
    }
    for (const auto& record : state.records) {
        platform::windows::write_data_memory(record.address, std::as_bytes(std::span(&record.original, 1)));
    }
    state.active = false;
}

void update_battle_map_shuffle(BattleMapShuffle& state, std::uintptr_t module_base,
                               std::uint64_t now_ms, bool enabled, bool council, std::mt19937& random) {
    if (!enabled) {
        disable_battle_map_shuffle(state);
        return;
    }
    if (!council && state.last_council) {
        disable_battle_map_shuffle(state);
        state.applied_this_council = false;
    }
    if (council && !state.applied_this_council) {
        disable_battle_map_shuffle(state);
        state.cached_base = 0;
        const auto base = resolve_base(state, module_base, now_ms);
        if (base) {
            state.records.clear();
            state.pool.clear();
            for (const auto offset : BATTLE_MAP_OFFSETS) {
                std::uint64_t original = 0;
                if (platform::windows::read_memory(base + offset, std::as_writable_bytes(std::span(&original, 1)))) {
                    state.records.push_back({base + offset, original});
                    if (std::find(state.pool.begin(), state.pool.end(), original) == state.pool.end()) {
                        state.pool.push_back(original);
                    }
                }
            }
            if (!state.pool.empty()) {
                for (const auto& record : state.records) {
                    const auto value = state.pool[random() % state.pool.size()];
                    platform::windows::write_data_memory(record.address, std::as_bytes(std::span(&value, 1)));
                }
                state.active = true;
                state.applied_this_council = true;
            }
        }
    }
    state.last_council = council;
    if (state.active && !resolve_base(state, module_base, now_ms)) {
        state.active = false;
        state.cached_base = 0;
    }
}

} // namespace s8::game
