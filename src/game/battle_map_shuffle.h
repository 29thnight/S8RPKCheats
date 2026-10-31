#pragma once

#include "battle_map_offsets.h"
#include "../domain/ids.h"
#include "../domain/work_budget.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <random>

namespace s8::game {

struct BattleMapRecord {
    std::uintptr_t address = 0;
    std::uint64_t original = 0;
    std::uint64_t replacement = 0;
    bool touched = false;
    bool uncertain_write = false;
};

enum class BattleMapShufflePhase : std::uint8_t {
    Idle, Capture, Assign, Applied, Restore, Stale
};

enum class BattleMapShuffleResult : std::uint8_t {
    Idle, Pending, Applied, Restored, SessionRetired,
    RootUnavailable, ReadFailed, WriteFailed, ValueChanged, StaleIdentity
};

struct BattleMapShuffle {
    static constexpr std::size_t SLICE_CAPACITY = 16;

    std::array<BattleMapRecord, BATTLE_MAP_OFFSETS.size()> records{};
    std::array<std::uint64_t, BATTLE_MAP_OFFSETS.size()> pool{};
    std::array<std::uintptr_t, 5> identity_chain{};
    std::size_t record_count = 0;
    std::size_t pool_count = 0;
    std::size_t capture_cursor = 0;
    std::size_t assign_cursor = 0;
    std::size_t restore_cursor = 0;
    std::size_t pending_restore_count = 0;
    std::size_t skipped_capture_count = 0;
    std::size_t retired_record_count = 0;
    std::uintptr_t module_base = 0;
    std::uintptr_t bound_base = 0;
    domain::GameEpoch epoch{};
    domain::GameEpoch retired_epoch{};
    std::uint64_t retry_after_ms = 0;
    std::uint64_t next_apply_ms = 0;
    BattleMapShufflePhase phase = BattleMapShufflePhase::Idle;
    BattleMapShuffleResult result = BattleMapShuffleResult::Idle;
    bool active = false;
    bool last_council = false;
    bool applied_this_council = false;
};

// A confirmed epoch transition ends the old object's lifetime. Retire its
// metadata without dereferencing an address from the previous session.
void reset_battle_map_shuffle(BattleMapShuffle& state, domain::GameEpoch next_epoch);

// False means that bounded restoration remains pending or failed. The journal
// remains owned by state; ordinary root failures never discard it.
bool disable_battle_map_shuffle(BattleMapShuffle& state, const domain::WorkBudget& work_budget = {});
void update_battle_map_shuffle(BattleMapShuffle& state, std::uintptr_t module_base,
                               std::uint64_t now_ms, bool enabled, bool council, std::mt19937& random,
                               const domain::WorkBudget& work_budget = {});

} // namespace s8::game
