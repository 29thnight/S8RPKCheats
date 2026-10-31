#include "battle_observation.h"

#include "../platform/windows/process_memory.h"

#include <array>
#include <span>

namespace s8::game {
namespace {

std::uintptr_t follow_chain(std::uintptr_t address, std::span<const std::uintptr_t> offsets) {
    for (const auto offset : offsets) {
        std::uintptr_t next = 0;
        if (!platform::windows::read_memory(address, std::as_writable_bytes(std::span(&next, 1))) || !next) {
            return 0;
        }
        address = next + offset;
    }
    return address;
}

} // namespace

std::optional<BattleObservation> read_battle_observation(std::uintptr_t module_base) {
    if (!module_base) {
        return std::nullopt;
    }
    constexpr std::array<std::uintptr_t, 2> ROOT_OFFSETS{0x28, 0x250};
    const auto shared = follow_chain(module_base + 0x02e99460, ROOT_OFFSETS);
    if (!shared) {
        return std::nullopt;
    }
    std::uintptr_t root = 0;
    if (!platform::windows::read_memory(shared, std::as_writable_bytes(std::span(&root, 1))) || !root) {
        return std::nullopt;
    }
    constexpr std::array<std::uintptr_t, 3> UNIT_OFFSETS{0, 0x180, 0};
    constexpr std::array<std::uintptr_t, 5> DAY_OFFSETS{0, 0x3d8, 0x478, 0, 0};
    const auto unit_list = follow_chain(root + 0x1d8, UNIT_OFFSETS);
    const auto day_base = follow_chain(root + 0x218, DAY_OFFSETS);
    if (unit_list < 8 || !day_base) {
        return std::nullopt;
    }
    std::uint8_t day = 0;
    std::uint8_t unit_count = 0;
    if (!platform::windows::read_memory(day_base + 0x28, std::as_writable_bytes(std::span(&day, 1))) ||
        !platform::windows::read_memory(unit_list - 8, std::as_writable_bytes(std::span(&unit_count, 1)))) {
        return std::nullopt;
    }
    return BattleObservation{unit_list, day_base, day, unit_count};
}

bool battle_records_ready(std::uintptr_t game_base, const BattleObservation& observation) {
    if (observation.day == 0 || observation.day > 30 || observation.unit_count == 0 ||
        observation.unit_count > 60 || !game_base) {
        return false;
    }
    std::uint8_t game_state = 0;
    std::uintptr_t player = 0;
    if (!platform::windows::read_memory(game_base + 0xd0, std::as_writable_bytes(std::span(&game_state, 1))) ||
        game_state == 0 || game_state == 0xff ||
        !platform::windows::read_memory(game_base + 0xe0, std::as_writable_bytes(std::span(&player, 1))) ||
        !platform::windows::is_committed_memory(player, 0x200)) {
        return false;
    }
    constexpr std::size_t MAX_UNITS = 60;
    std::array<std::uintptr_t, MAX_UNITS * 2> records{};
    const std::size_t record_bytes = (observation.unit_count - 1) * 0x10 + sizeof(std::uintptr_t);

    if (!platform::windows::read_memory(observation.unit_list + 8,
            std::as_writable_bytes(std::span(records)).first(record_bytes))) {
        return false;
    }
    for (std::size_t i = 0; i < observation.unit_count; ++i) {
        const std::uintptr_t unit = records[i * 2];
        std::uintptr_t member = 0;

        if (platform::windows::is_committed_memory(unit, 0x600) &&
            platform::windows::read_memory(unit + 0x18, std::as_writable_bytes(std::span(&member, 1))) &&
            platform::windows::is_committed_memory(member, 0x100)) {
            return true;
        }
    }
    return false;
}

bool update_battle_readiness(BattleReadiness& state, const BattleObservation& observation,
                             bool records_ready, std::uint64_t now_ms) {
    const bool identity_changed = state.unit_list != observation.unit_list || state.day_base != observation.day_base;
    if (!records_ready || identity_changed) {
        state.ready = false;
        state.stable_since_ms = records_ready ? now_ms : 0;
        state.unit_list = observation.unit_list;
        state.day_base = observation.day_base;
    } else if (state.stable_since_ms == 0) {
        state.stable_since_ms = now_ms;
    } else {
        state.ready = now_ms - state.stable_since_ms >= 1000;
    }
    return state.ready;
}

} // namespace s8::game
