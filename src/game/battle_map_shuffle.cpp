#include "battle_map_shuffle.h"
#include "../platform/windows/process_memory.h"

#include <algorithm>
#include <array>
#include <limits>
#include <span>

namespace s8::game {
namespace {

constexpr std::uintptr_t MAP_ROOT_OFFSET = 0x034c8630;
constexpr std::array<std::uintptr_t, 5> ROOT_CHAIN{0, 0, 8, 0x10, 0};
constexpr std::uint64_t FAILURE_RETRY_MS = 250;

std::uintptr_t add_address(std::uintptr_t base, std::uintptr_t offset) {
    constexpr std::uintptr_t LIMIT = (std::numeric_limits<std::uintptr_t>::max)();

    if (!base || offset > LIMIT - base || base + offset > LIMIT - (sizeof(std::uint64_t) - 1)) {
        return 0;
    }
    return base + offset;
}

bool resolve_identity(std::uintptr_t module_base, std::array<std::uintptr_t, 5>& chain) {
    std::uintptr_t address = add_address(module_base, MAP_ROOT_OFFSET);

    for (std::size_t index = 0; index < ROOT_CHAIN.size(); ++index) {
        std::uintptr_t next = 0;
        const std::uintptr_t source = add_address(address, ROOT_CHAIN[index]);

        if (!source || !platform::windows::read_memory(source,
                std::as_writable_bytes(std::span(&next, 1))) || !next) {
            return false;
        }
        chain[index] = next;
        address = next;
    }
    return true;
}

bool validate_identity(BattleMapShuffle& state, std::uintptr_t module_base) {
    if (state.phase == BattleMapShufflePhase::Stale) {
        state.result = BattleMapShuffleResult::StaleIdentity;
        return false;
    }
    if (!module_base) {
        state.result = BattleMapShuffleResult::RootUnavailable;
        return false;
    }
    if (module_base != state.module_base) {
        state.phase = BattleMapShufflePhase::Stale;
        state.result = BattleMapShuffleResult::StaleIdentity;
        return false;
    }

    std::array<std::uintptr_t, 5> chain{};

    if (!resolve_identity(module_base, chain)) {
        state.result = BattleMapShuffleResult::RootUnavailable;
        return false;
    }
    if (chain != state.identity_chain) {
        // A transient read failure is retryable, but an observed different
        // object is not permission to replay this journal at an old address.
        state.phase = BattleMapShufflePhase::Stale;
        state.result = BattleMapShuffleResult::StaleIdentity;
        return false;
    }
    return true;
}

void clear_work(BattleMapShuffle& state) {
    // The fixed arrays are reused. Their counted prefixes are overwritten by
    // the next capture; no full-buffer clearing is needed on a frame boundary.
    state.identity_chain = {};
    state.record_count = 0;
    state.pool_count = 0;
    state.capture_cursor = 0;
    state.assign_cursor = 0;
    state.restore_cursor = 0;
    state.pending_restore_count = 0;
    state.skipped_capture_count = 0;
    state.module_base = 0;
    state.bound_base = 0;
    state.phase = BattleMapShufflePhase::Idle;
    state.active = false;
}

void begin_restore(BattleMapShuffle& state) {
    if (state.phase != BattleMapShufflePhase::Restore) {
        state.phase = BattleMapShufflePhase::Restore;
        state.restore_cursor = state.record_count;
    }
}

bool owned_value(const BattleMapRecord& record, std::uint64_t current) {
    if (current == record.original || current == record.replacement) {
        return true;
    }
    if (!record.uncertain_write) {
        return false;
    }

    constexpr std::uint64_t BYTE_MASK = 0xff;

    // A failed native write may have copied only some bytes. Only that record
    // permits a bytewise mixture of its original and attempted replacement.
    for (std::size_t index = 0; index < sizeof(current); ++index) {
        const std::size_t shift = index * 8;
        const std::uint64_t byte = (current >> shift) & BYTE_MASK;

        if (byte != ((record.original >> shift) & BYTE_MASK) &&
            byte != ((record.replacement >> shift) & BYTE_MASK)) {
            return false;
        }
    }
    return true;
}

bool restore_slice(BattleMapShuffle& state, const domain::WorkBudget& work_budget) {
    std::size_t visited = 0;

    state.result = BattleMapShuffleResult::Pending;
    while (state.restore_cursor && visited < BattleMapShuffle::SLICE_CAPACITY &&
            (!visited || !work_budget.exhausted())) {
        BattleMapRecord& record = state.records[state.restore_cursor - 1];

        ++visited;
        if (record.touched) {
            std::uint64_t current = 0;

            if (!platform::windows::read_memory(record.address,
                    std::as_writable_bytes(std::span(&current, 1)))) {
                state.result = BattleMapShuffleResult::ReadFailed;
                return false;
            }
            if (!owned_value(record, current)) {
                state.result = BattleMapShuffleResult::ValueChanged;
                return false;
            }
            if (current != record.original) {
                record.uncertain_write = true;
                if (!platform::windows::write_data_memory(record.address,
                        std::as_bytes(std::span(&record.original, 1)))) {
                    state.result = BattleMapShuffleResult::WriteFailed;
                    return false;
                }
                if (!platform::windows::read_memory(record.address,
                        std::as_writable_bytes(std::span(&current, 1)))) {
                    state.result = BattleMapShuffleResult::ReadFailed;
                    return false;
                }
                if (current != record.original) {
                    state.result = BattleMapShuffleResult::ValueChanged;
                    return false;
                }
            }
            record.touched = false;
            record.uncertain_write = false;
            --state.pending_restore_count;
        }
        --state.restore_cursor;
    }
    state.active = state.pending_restore_count != 0;
    if (state.restore_cursor) {
        return false;
    }

    clear_work(state);
    state.result = BattleMapShuffleResult::Restored;
    return true;
}

void capture_slice(BattleMapShuffle& state, std::uint64_t now_ms,
                   const domain::WorkBudget& work_budget) {
    std::size_t visited = 0;

    state.result = BattleMapShuffleResult::Pending;
    while (state.capture_cursor < BATTLE_MAP_OFFSETS.size() &&
            visited < BattleMapShuffle::SLICE_CAPACITY && (!visited || !work_budget.exhausted())) {
        const std::uintptr_t offset = BATTLE_MAP_OFFSETS[state.capture_cursor];
        const std::uintptr_t address = add_address(state.bound_base, offset);
        std::uint64_t original = 0;

        ++visited;
        ++state.capture_cursor;
        if (!address || !platform::windows::read_memory(address,
                std::as_writable_bytes(std::span(&original, 1)))) {
            ++state.skipped_capture_count;
            continue;
        }
        state.records[state.record_count] = {address, original, 0, false, false};
        ++state.record_count;

        const std::array<std::uint64_t, BATTLE_MAP_OFFSETS.size()>::iterator pool_end =
            state.pool.begin() + state.pool_count;

        if (std::find(state.pool.begin(), pool_end, original) == pool_end) {
            state.pool[state.pool_count] = original;
            ++state.pool_count;
        }
    }
    if (state.capture_cursor != BATTLE_MAP_OFFSETS.size()) {
        return;
    }
    if (!state.pool_count) {
        clear_work(state);
        state.result = BattleMapShuffleResult::ReadFailed;
        state.next_apply_ms = now_ms + FAILURE_RETRY_MS;
        return;
    }
    state.phase = BattleMapShufflePhase::Assign;
}

void fail_assignment(BattleMapShuffle& state, BattleMapShuffleResult result, std::uint64_t now_ms) {
    begin_restore(state);
    state.result = result;
    state.next_apply_ms = now_ms + FAILURE_RETRY_MS;
    // Start rollback on the next admitted slice. Only a failed rollback is
    // throttled; a failed assignment must not delay undoing earlier writes.
    state.retry_after_ms = 0;
}

void assign_slice(BattleMapShuffle& state, std::uint64_t now_ms, std::mt19937& random,
                  const domain::WorkBudget& work_budget) {
    std::size_t visited = 0;

    state.result = BattleMapShuffleResult::Pending;
    while (state.assign_cursor < state.record_count && visited < BattleMapShuffle::SLICE_CAPACITY &&
            (!visited || !work_budget.exhausted())) {
        BattleMapRecord& record = state.records[state.assign_cursor];
        std::uint64_t current = 0;

        ++visited;
        if (!platform::windows::read_memory(record.address,
                std::as_writable_bytes(std::span(&current, 1)))) {
            fail_assignment(state, BattleMapShuffleResult::ReadFailed, now_ms);
            return;
        }
        if (current != record.original) {
            fail_assignment(state, BattleMapShuffleResult::ValueChanged, now_ms);
            return;
        }

        const std::uint64_t replacement = state.pool[random() % state.pool_count];

        record.replacement = replacement;
        if (replacement != record.original) {
            // Publish rollback ownership before even a potentially partial
            // write. A false write result never removes this original value.
            record.touched = true;
            record.uncertain_write = true;
            ++state.pending_restore_count;
            state.active = true;
            if (!platform::windows::write_data_memory(record.address,
                    std::as_bytes(std::span(&replacement, 1)))) {
                fail_assignment(state, BattleMapShuffleResult::WriteFailed, now_ms);
                return;
            }
            if (!platform::windows::read_memory(record.address,
                    std::as_writable_bytes(std::span(&current, 1)))) {
                fail_assignment(state, BattleMapShuffleResult::ReadFailed, now_ms);
                return;
            }
            if (current != replacement) {
                fail_assignment(state, BattleMapShuffleResult::ValueChanged, now_ms);
                return;
            }
            record.uncertain_write = false;
        }
        ++state.assign_cursor;
    }
    if (state.assign_cursor == state.record_count) {
        state.phase = BattleMapShufflePhase::Applied;
        state.applied_this_council = true;
        state.result = BattleMapShuffleResult::Applied;
    }
}

bool failed(BattleMapShuffleResult result) {
    return result == BattleMapShuffleResult::RootUnavailable ||
        result == BattleMapShuffleResult::ReadFailed || result == BattleMapShuffleResult::WriteFailed ||
        result == BattleMapShuffleResult::ValueChanged || result == BattleMapShuffleResult::StaleIdentity;
}

} // namespace

void reset_battle_map_shuffle(BattleMapShuffle& state, domain::GameEpoch next_epoch) {
    if (state.epoch == next_epoch) {
        return;
    }
    const bool retired = state.pending_restore_count != 0;

    if (retired) {
        state.retired_record_count = state.pending_restore_count;
        state.retired_epoch = state.epoch;
    }
    clear_work(state);
    state.epoch = next_epoch;
    state.retry_after_ms = 0;
    state.next_apply_ms = 0;
    state.last_council = false;
    state.applied_this_council = false;
    state.result = retired ? BattleMapShuffleResult::SessionRetired : BattleMapShuffleResult::Idle;
}

bool disable_battle_map_shuffle(BattleMapShuffle& state, const domain::WorkBudget& work_budget) {
    if (state.phase == BattleMapShufflePhase::Stale) {
        state.result = BattleMapShuffleResult::StaleIdentity;
        return false;
    }
    if (!state.active) {
        if (state.phase != BattleMapShufflePhase::Idle) {
            clear_work(state);
            state.result = BattleMapShuffleResult::Idle;
        }
        return true;
    }
    begin_restore(state);
    if (work_budget.exhausted()) {
        state.result = BattleMapShuffleResult::Pending;
        return false;
    }
    if (!validate_identity(state, state.module_base)) {
        return false;
    }
    // Identity resolution is part of the first admitted item. Finish that
    // item even if its native reads used the remaining cooperative budget.
    return restore_slice(state, work_budget);
}

void update_battle_map_shuffle(BattleMapShuffle& state, std::uintptr_t module_base,
                               std::uint64_t now_ms, bool enabled, bool council, std::mt19937& random,
                               const domain::WorkBudget& work_budget) {
    state.last_council = council;
    if (!council) {
        state.applied_this_council = false;
    }
    if (work_budget.exhausted() || now_ms < state.retry_after_ms ||
        state.phase == BattleMapShufflePhase::Stale) {
        return;
    }
    if (!enabled || !council || state.phase == BattleMapShufflePhase::Restore) {
        bool restored = false;

        if (state.active && module_base != state.module_base) {
            validate_identity(state, module_base);
        } else {
            restored = disable_battle_map_shuffle(state, work_budget);
        }
        if (!restored && failed(state.result)) {
            state.retry_after_ms = now_ms + FAILURE_RETRY_MS;
        }
        return;
    }
    if (state.phase == BattleMapShufflePhase::Idle) {
        if (state.applied_this_council || now_ms < state.next_apply_ms) {
            return;
        }

        std::array<std::uintptr_t, 5> chain{};

        if (!resolve_identity(module_base, chain)) {
            state.result = BattleMapShuffleResult::RootUnavailable;
            state.retry_after_ms = now_ms + FAILURE_RETRY_MS;
            return;
        }
        clear_work(state);
        state.identity_chain = chain;
        state.module_base = module_base;
        state.bound_base = chain.back();
        state.phase = BattleMapShufflePhase::Capture;
    } else if (!validate_identity(state, module_base)) {
        state.retry_after_ms = now_ms + FAILURE_RETRY_MS;
        return;
    }

    if (state.phase == BattleMapShufflePhase::Capture) {
        capture_slice(state, now_ms, work_budget);
    } else if (state.phase == BattleMapShufflePhase::Assign) {
        assign_slice(state, now_ms, random, work_budget);
    }
}

} // namespace s8::game
