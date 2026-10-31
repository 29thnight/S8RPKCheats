#include "faction_lord_bonus.h"
#include "../platform/windows/process_memory.h"

#include <algorithm>
#include <array>
#include <span>

namespace s8::game {
namespace {

constexpr std::size_t FACTION_COUNT = 120;
constexpr std::size_t ITEMS_PER_UPDATE = 8;
constexpr std::uint64_t UPDATE_INTERVAL_MS = 5000;
constexpr std::uintptr_t BONUS_OFFSET = 0x4ec6d8;
constexpr std::uintptr_t BONUS_STRIDE = 24;
constexpr std::uintptr_t NAMES_OFFSET = 0x197a774;
constexpr std::uintptr_t LORD_BONUS_OFFSET = 0x330;
constexpr std::array<std::uint8_t, 48> NAMES{
    0x69,0xd6,0x1c,0xc8,0,0,0,0, 0x55,0xc6,0,0,0,0,0,0,
    0xf5,0xac,0,0,0,0,0,0, 0xfc,0xc8,0xa9,0xba,0,0,0,0,
    0x70,0xad,0xfc,0xc8,0,0,0,0, 0x9c,0xcc,0xf5,0xac,0xa5,0xc7,0x70,0xad};
constexpr std::array<std::array<std::uint8_t, 15>, 5> PATCHES{{
    {0x54,0,0x14,0x88,0x13,5,5,5,5,5,0,1,3,1,0},
    {0x56,0,0x14,0xb8,0x0b,4,4,4,4,4,0,1,3,1,0},
    {0x58,0,0x14,0xd0,0x07,3,3,3,3,3,0,1,3,1,0},
    {0x5a,0,0x14,0xe8,0x03,2,2,2,2,2,0,1,3,1,0},
    {0x5c,0,0x14,0,0,1,1,1,1,1,0,1,3,1,0},
}};

template <typename T>
bool read_value(std::uintptr_t address, T& value) {
    return platform::windows::read_memory(address, std::as_writable_bytes(std::span(&value, 1)));
}

std::uintptr_t root_address(std::uintptr_t module_base) {
    if (!module_base) {
        return 0;
    }
    std::uintptr_t address = module_base + 0x034c8630;
    constexpr std::array<std::uintptr_t, 5> OFFSETS{0, 0, 8, 0x10, 0};

    for (const std::uintptr_t offset : OFFSETS) {
        std::uintptr_t next = 0;

        if (!read_value(address + offset, next) || !next) {
            return 0;
        }
        address = next;
    }
    return address;
}

bool same_root(const FactionLordBonus& state) {
    return state.root && root_address(state.module) == state.root;
}

bool same_lord(const FactionLordBonus& state, const FactionLordAssignment& record) {
    std::uint16_t officer_id = 0;

    return same_root(state) && read_value(record.lord + 8, officer_id) && officer_id == record.officer_id;
}

bool same_faction_lord(const FactionLordBonus& state, std::uintptr_t faction,
    const FactionLordAssignment& record) {
    std::uintptr_t lord = 0;
    std::uint8_t exists = 0;
    std::uint8_t wandering = 1;

    return same_lord(state, record) && read_value(faction + 0xc690d, exists) && exists &&
        read_value(faction + 0xc6a28, wandering) && !wandering &&
        read_value(faction + 0xc6908, lord) && lord == record.lord;
}

bool same_bytes(std::span<const std::byte> left, std::span<const std::byte> right) {
    return left.size() == right.size() && std::equal(left.begin(), left.end(), right.begin());
}

bool attempted_bytes(std::span<const std::byte> current, std::span<const std::byte> before,
    std::span<const std::byte> after) {
    for (std::size_t index = 0; index < current.size(); ++index) {
        if (current[index] != before[index] && current[index] != after[index]) {
            return false;
        }
    }
    return true;
}

std::span<const std::byte> desired_data(std::size_t index) {
    if (index == 0) {
        return std::as_bytes(std::span(NAMES));
    }
    return std::as_bytes(std::span(PATCHES[index - 1]));
}

std::uintptr_t data_address(const FactionLordBonus& state, std::size_t index) {
    return state.root + (index == 0 ? NAMES_OFFSET : BONUS_OFFSET + (index - 1) * BONUS_STRIDE + 9);
}

bool apply_data(FactionLordBonus& state, std::size_t index) {
    FactionLordDataBackup& backup = state.data[index];
    const std::span<const std::byte> desired = desired_data(index);
    const std::uintptr_t address = data_address(state, index);
    std::array<std::byte, 48> current{};
    const std::span<std::byte> bytes = std::span(current).first(desired.size());
    const std::span<const std::byte> original = std::span(backup.original).first(desired.size());

    backup.applied = false;
    if (index == 0) {
        state.names_applied = false;
    }
    if (!same_root(state) || !platform::windows::read_memory(address, bytes)) {
        return false;
    }
    if (!backup.captured) {
        std::copy(bytes.begin(), bytes.end(), backup.original.begin());
        backup.captured = true;
        ++state.data_count;
    }
    if (same_bytes(bytes, desired)) {
        backup.applied = same_root(state);
        if (backup.applied) {
            backup.uncertain = false;
        }
    } else {
        if ((!same_bytes(bytes, original) &&
                !(backup.uncertain && attempted_bytes(bytes, original, desired))) || !same_root(state)) {
            return false;
        }
        backup.uncertain = true;
        const bool written = platform::windows::write_data_memory(address, desired);
        const bool observed = platform::windows::read_memory(address, bytes);

        backup.applied = written && observed && same_bytes(bytes, desired) && same_root(state);
        backup.uncertain = !backup.applied;
    }
    if (index == 0) {
        state.names_applied = backup.applied;
    }
    return backup.applied;
}

bool restore_data(FactionLordBonus& state, std::size_t index) {
    FactionLordDataBackup& backup = state.data[index];

    if (!backup.captured) {
        return true;
    }
    const std::span<const std::byte> desired = desired_data(index);
    const std::span<const std::byte> original = std::span(backup.original).first(desired.size());
    const std::uintptr_t address = data_address(state, index);
    std::array<std::byte, 48> current{};
    const std::span<std::byte> bytes = std::span(current).first(desired.size());

    if (!same_root(state) || !platform::windows::read_memory(address, bytes)) {
        return false;
    }
    if (!same_bytes(bytes, original)) {
        if ((!same_bytes(bytes, desired) &&
                !(backup.uncertain && attempted_bytes(bytes, original, desired))) || !same_root(state)) {
            return false;
        }
        backup.uncertain = true;
        const bool written = platform::windows::write_data_memory(address, original);
        const bool observed = platform::windows::read_memory(address, bytes);

        if (!written || !observed || !same_bytes(bytes, original) || !same_root(state)) {
            return false;
        }
    } else if (!same_root(state)) {
        return false;
    }
    backup = {};
    --state.data_count;
    if (index == 0) {
        state.names_applied = false;
    }
    return true;
}

void forget_assignment(FactionLordBonus& state, FactionLordAssignment& record) {
    record = {};
    --state.assigned_count;
}

bool restore_assignment(FactionLordBonus& state, FactionLordAssignment& record) {
    std::uint16_t officer_id = 0;

    if (!same_root(state) || !read_value(record.lord + 8, officer_id)) {
        return false;
    }
    if (officer_id != record.officer_id) {
        // This address now belongs to another officer. Its contents are not ours.
        forget_assignment(state, record);
        return true;
    }
    std::uintptr_t current = 0;
    const std::uintptr_t address = record.lord + LORD_BONUS_OFFSET;

    if (!read_value(address, current)) {
        return false;
    }
    if (current != record.original) {
        const bool partial = record.uncertain && attempted_bytes(std::as_bytes(std::span(&current, 1)),
            std::as_bytes(std::span(&record.write_before, 1)), std::as_bytes(std::span(&record.write_after, 1)));

        if ((current != record.applied && !partial) || !same_lord(state, record)) {
            return false;
        }
        record.write_before = current;
        record.write_after = record.original;
        record.uncertain = true;
        const bool written = platform::windows::write_data_memory(address,
            std::as_bytes(std::span(&record.original, 1)));
        const bool observed = read_value(address, current);

        if (!written || !observed || current != record.original || !same_lord(state, record)) {
            return false;
        }
    } else if (!same_lord(state, record)) {
        return false;
    }
    forget_assignment(state, record);
    return true;
}

FactionLordAssignment* find_assignment(FactionLordBonus& state, std::uintptr_t lord) {
    for (std::size_t index = 0; index < state.assigned_limit; ++index) {
        FactionLordAssignment& record = state.assigned[index];

        if (record.occupied && record.lord == lord) {
            return &record;
        }
    }
    return nullptr;
}

FactionLordAssignment* reserve_assignment(FactionLordBonus& state) {
    for (std::size_t index = 0; index < state.assigned.size(); ++index) {
        FactionLordAssignment& record = state.assigned[index];

        if (!record.occupied) {
            record.occupied = true;
            ++state.assigned_count;
            state.assigned_limit = (std::max)(state.assigned_limit, index + 1);
            return &record;
        }
    }
    // Failed restores retain their backup; capacity exhaustion refuses new writes.
    return nullptr;
}

void apply_assignment(FactionLordBonus& state, std::uintptr_t faction, std::uintptr_t lord,
    std::uint16_t officer_id, std::uintptr_t target) {
    FactionLordAssignment* record = find_assignment(state, lord);
    std::uintptr_t current = 0;

    if (record && record->officer_id != officer_id) {
        forget_assignment(state, *record);
        record = nullptr;
    }
    if (record) {
        record->seen_pass = state.pass;
        if (record->uncertain) {
            return;
        }
    }
    if (!read_value(lord + LORD_BONUS_OFFSET, current)) {
        return;
    }
    if (!record) {
        record = reserve_assignment(state);
        if (!record) {
            return;
        }
        record->lord = lord;
        record->officer_id = officer_id;
        record->original = current;
        record->applied = current;
        record->seen_pass = state.pass;
    }
    if ((current != record->original && current != record->applied) ||
        !same_faction_lord(state, faction, *record)) {
        return;
    }
    if (current == target) {
        record->applied = target;
        return;
    }
    record->write_before = current;
    record->write_after = target;
    record->uncertain = true;
    const bool written = platform::windows::write_data_memory(lord + LORD_BONUS_OFFSET,
        std::as_bytes(std::span(&target, 1)));
    const bool observed = read_value(lord + LORD_BONUS_OFFSET, current);

    if (written && observed && current == target && same_faction_lord(state, faction, *record)) {
        record->applied = target;
        record->uncertain = false;
    }
}

void scan_faction(FactionLordBonus& state, std::size_t index) {
    const std::uintptr_t faction = state.root + index * 0x998;
    std::uint8_t exists = 0;
    std::uint8_t wandering = 1;
    std::uintptr_t lord = 0;
    std::uintptr_t title = 0;
    std::uint16_t officer_id = 0;
    std::uint8_t rank = 0;

    if (!read_value(faction + 0xc690d, exists)) {
        state.scan_reliable = false;
        return;
    }
    if (!exists) {
        return;
    }
    if (!read_value(faction + 0xc6a28, wandering)) {
        state.scan_reliable = false;
        return;
    }
    if (wandering) {
        return;
    }
    if (!read_value(faction + 0xc6908, lord) || !lord ||
        !platform::windows::is_committed_memory(lord, 0x338) ||
        !read_value(lord + 8, officer_id) || officer_id < 1 || officer_id > 5102 ||
        !read_value(faction + 0xc6a20, title) || (title && !read_value(title + 0x0e, rank))) {
        state.scan_reliable = false;
        return;
    }
    const std::uintptr_t target = state.root + BONUS_OFFSET +
        static_cast<std::uintptr_t>(rank >= 1 && rank <= 4 ? rank - 1 : 4) * BONUS_STRIDE;

    apply_assignment(state, faction, lord, officer_id, target);
}

void begin_cleanup(FactionLordBonus& state) {
    state.restoring = true;
    state.cleanup_cursor = 0;
    state.data_cursor = 0;
    state.phase = FactionLordBonusPhase::Cleanup;
}

void start_pass(FactionLordBonus& state) {
    ++state.pass;
    state.data_cursor = 0;
    state.faction_cursor = 0;
    state.cleanup_cursor = 0;
    state.data_ready = true;
    state.scan_reliable = true;
    state.phase = FactionLordBonusPhase::ApplyData;
}

} // namespace

bool faction_lord_bonus_pending(const FactionLordBonus& state) noexcept {
    return state.assigned_count != 0 || state.data_count != 0 || state.restoring;
}

bool set_faction_lord_bonus(FactionLordBonus& state, std::uintptr_t module_base, bool enabled) {
    static_cast<void>(module_base);

    if (state.enabled != enabled) {
        state.enabled = enabled;
        state.last_tick_ms = 0;
    }
    if (enabled) {
        return !state.restoring;
    }
    if (!state.restoring && (state.assigned_count != 0 || state.data_count != 0)) {
        begin_cleanup(state);
    }
    if (!faction_lord_bonus_pending(state)) {
        state.phase = FactionLordBonusPhase::Idle;
        return true;
    }
    return false;
}

void update_faction_lord_bonus(FactionLordBonus& state, std::uintptr_t module_base,
    std::uint64_t now_ms, const domain::WorkBudget& work_budget) {
    if (work_budget.exhausted() || (!state.enabled && !faction_lord_bonus_pending(state))) {
        return;
    }
    const std::uintptr_t root = root_address(module_base);

    if (!root) {
        return;
    }
    if (state.module != module_base || state.root != root) {
        // A temporary root change is not proof that our old writes can be
        // forgotten. Only Runtime's explicit epoch retirement discards them.
        if (faction_lord_bonus_pending(state)) {
            return;
        }
        const bool enabled = state.enabled;

        state = {};
        state.enabled = enabled;
        state.module = module_base;
        state.root = root;
        if (!state.enabled) {
            return;
        }
    }
    if (!state.enabled && !state.restoring) {
        begin_cleanup(state);
    }
    if (state.phase == FactionLordBonusPhase::Idle) {
        if (!state.enabled || (state.last_tick_ms && now_ms >= state.last_tick_ms &&
                now_ms - state.last_tick_ms < UPDATE_INTERVAL_MS)) {
            return;
        }
        start_pass(state);
    }
    for (std::size_t processed = 0; processed < ITEMS_PER_UPDATE; ++processed) {
        if (work_budget.exhausted()) {
            return;
        }
        // A single record's read/write/readback is one non-preemptible unit.
        switch (state.phase) {
        case FactionLordBonusPhase::ApplyData:
            state.data_ready = apply_data(state, state.data_cursor) && state.data_ready;
            ++state.data_cursor;
            if (state.data_cursor == state.data.size()) {
                state.phase = state.data_ready ? FactionLordBonusPhase::Scan : FactionLordBonusPhase::Idle;
                if (!state.data_ready) {
                    state.last_tick_ms = now_ms;
                    return;
                }
            }
            break;
        case FactionLordBonusPhase::Scan:
            if (!same_root(state)) {
                return;
            }
            scan_faction(state, state.faction_cursor);
            ++state.faction_cursor;
            if (state.faction_cursor == FACTION_COUNT) {
                state.phase = FactionLordBonusPhase::Cleanup;
            }
            break;
        case FactionLordBonusPhase::Cleanup:
            if (state.assigned_count && state.cleanup_cursor < state.assigned_limit) {
                FactionLordAssignment& record = state.assigned[state.cleanup_cursor];

                if (record.occupied && (state.restoring || record.uncertain ||
                        (state.scan_reliable && record.seen_pass != state.pass))) {
                    restore_assignment(state, record);
                }
                ++state.cleanup_cursor;
            }
            if (!state.assigned_count || state.cleanup_cursor == state.assigned_limit) {
                if (state.restoring) {
                    if (state.assigned_count) {
                        state.cleanup_cursor = 0;
                        return;
                    }
                    state.data_cursor = 0;
                    state.phase = FactionLordBonusPhase::RestoreData;
                } else {
                    state.phase = FactionLordBonusPhase::Idle;
                    state.last_tick_ms = now_ms;
                    return;
                }
            }
            break;
        case FactionLordBonusPhase::RestoreData: {
            const std::size_t index = (state.data_cursor + 1) % state.data.size();
            const std::size_t names_count = state.data[0].captured ? 1 : 0;

            // Keep the names while even one bonus record still needs restoration.
            if (index != 0 || state.data_count == names_count) {
                restore_data(state, index);
            }
            ++state.data_cursor;
            if (state.data_cursor == state.data.size()) {
                if (state.data_count) {
                    state.data_cursor = 0;
                    return;
                }
                state.restoring = false;
                state.assigned_limit = 0;
                state.phase = FactionLordBonusPhase::Idle;
                state.last_tick_ms = 0;
                return;
            }
            break;
        }
        case FactionLordBonusPhase::Idle:
            return;
        }
    }
}

} // namespace s8::game
