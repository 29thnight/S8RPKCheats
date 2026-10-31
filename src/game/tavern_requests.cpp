#include "tavern_requests.h"

#include "../platform/windows/process_memory.h"

#include <array>
#include <span>

namespace s8::game {
namespace {

template <typename T>
bool read_value(std::uintptr_t address, T& value) {
    return platform::windows::read_memory(address, std::as_writable_bytes(std::span(&value, 1)));
}

std::uintptr_t slot_address(std::uintptr_t base, int city, int slot) {
    return base + static_cast<std::uintptr_t>(city) * 0x2a0 + 0x178 + static_cast<std::uintptr_t>(slot) * 0x28;
}

bool usable(const TavernRequest& request) {
    return request.name > 0x10000 && request.data > 0x10000 &&
        platform::windows::is_committed_memory(request.name, 1) &&
        platform::windows::is_committed_memory(request.data, 1);
}

bool read_pair(std::uintptr_t address, TavernRequest& request) {
    return read_value(address, request.name) && read_value(address + 8, request.data);
}

std::uintptr_t city_root(std::uintptr_t module_base) {
    std::uintptr_t root = 0;

    if (!module_base || !read_value(module_base + 0x034c8630, root) || root <= 0x10000 ||
        !read_value(root, root) || root <= 0x10000 || !read_value(root, root) || root <= 0x10000) {
        return 0;
    }
    return root;
}

bool source_live(const TavernRequests& state, int city, int slot, const TavernRequest& expected) {
    TavernRequest actual{};
    return read_pair(slot_address(state.city_base, city, slot), actual) && actual == expected && usable(actual) &&
           !(state.injected[city][slot] == actual);
}

constexpr std::size_t SLOT_COUNT = 4;
constexpr std::size_t TOTAL_SLOTS = 51 * SLOT_COUNT;
constexpr std::size_t WORK_ITEMS_PER_SLICE = 16;

void begin_maintenance(TavernRequests& state, bool prefer_live) {
    state.maintenance = TavernMaintenancePhase::Scan;
    state.fill = TavernFillPhase::Inspect;
    state.cursor = 0;
    state.source_cursor = 0;
    state.source_count = 0;
    state.prefer_live = prefer_live;
}

void advance_slot(TavernRequests& state) {
    ++state.cursor;
    state.source_cursor = 0;
    state.fill = TavernFillPhase::Inspect;
    if (state.cursor == TOTAL_SLOTS) {
        state.maintenance = TavernMaintenancePhase::Idle;
    }
}

void settle_pending_pair(TavernRequests& state, bool committed) {
    const TavernPendingPair& pending = state.pending;

    if (committed) {
        state.current[pending.city][pending.slot] = pending.attempted;
        state.injected[pending.city][pending.slot] = pending.attempted;
        if (pending.replace_backup) {
            state.backups[pending.city][pending.slot] = pending.committed_backup;
        }
        advance_slot(state);
    } else if (state.fill == TavernFillPhase::Backup) {
        state.fill = TavernFillPhase::LiveFallback;
        state.source_cursor = 0;
    } else if (state.fill == TavernFillPhase::LiveFirst) {
        state.fill = TavernFillPhase::Backup;
    } else {
        advance_slot(state);
    }
    state.pending = {};
}

bool owned_partial_pair(const TavernPendingPair& pending, const TavernRequest& actual) {
    const std::array<std::uintptr_t, 2> before{pending.before.name, pending.before.data};
    const std::array<std::uintptr_t, 2> attempted{pending.attempted.name, pending.attempted.data};
    const std::array<std::uintptr_t, 2> current{actual.name, actual.data};
    const std::span<const std::byte> original = std::as_bytes(std::span(before));
    const std::span<const std::byte> desired = std::as_bytes(std::span(attempted));
    const std::span<const std::byte> observed = std::as_bytes(std::span(current));

    for (std::size_t index = 0; index < observed.size(); ++index) {
        if (observed[index] != original[index] && observed[index] != desired[index]) {
            return false;
        }
    }
    return true;
}

bool recover_pending_pair(TavernRequests& state, std::uintptr_t module_base) {
    const TavernPendingPair& pending = state.pending;

    if (!pending.active) {
        return true;
    }
    if (city_root(module_base) != pending.city_base) {
        return false;
    }

    TavernRequest actual{};

    if (!read_pair(pending.address, actual)) {
        return false;
    }
    if (actual == pending.attempted) {
        settle_pending_pair(state, true);
        return true;
    }
    if (actual == pending.before) {
        settle_pending_pair(state, false);
        return true;
    }
    if (!owned_partial_pair(pending, actual) || city_root(module_base) != pending.city_base) {
        return false;
    }

    // Only bytes from this transaction may be rolled back. A foreign pair or
    // an unreadable target keeps its record and is never overwritten.
    const std::array<std::uintptr_t, 2> original{pending.before.name, pending.before.data};

    platform::windows::write_data_memory(pending.address, std::as_bytes(std::span(original)));
    if (city_root(module_base) != pending.city_base || !read_pair(pending.address, actual)) {
        return false;
    }
    if (actual == pending.before) {
        settle_pending_pair(state, false);
        return true;
    }
    if (actual == pending.attempted) {
        settle_pending_pair(state, true);
        return true;
    }
    return false;
}

enum class PairWriteResult { Rejected, Settled, Pending };

PairWriteResult write_pair(TavernRequests& state, std::uintptr_t module_base, int city, int slot,
                           const TavernRequest& expected, const TavernRequest& request,
                           const TavernBackup* committed_backup) {
    if (state.pending.active) {
        return PairWriteResult::Pending;
    }
    if (!usable(request) || city_root(module_base) != state.city_base) {
        return PairWriteResult::Rejected;
    }

    const std::uintptr_t address = slot_address(state.city_base, city, slot);
    TavernRequest previous{};

    if (!read_pair(address, previous) || previous != expected) {
        return PairWriteResult::Rejected;
    }

    TavernPendingPair& pending = state.pending;

    pending.city_base = state.city_base;
    pending.address = address;
    pending.before = previous;
    pending.attempted = request;
    pending.city = city;
    pending.slot = slot;
    pending.replace_backup = committed_backup != nullptr;
    if (committed_backup != nullptr) {
        pending.committed_backup = *committed_backup;
    }
    // Publish recovery identity before writing either pointer of the pair.
    pending.active = true;

    const std::array<std::uintptr_t, 2> desired{request.name, request.data};

    platform::windows::write_data_memory(address, std::as_bytes(std::span(desired)));
    return recover_pending_pair(state, module_base) ? PairWriteResult::Settled : PairWriteResult::Pending;
}

bool duplicate_in_city(TavernRequests& state, int city, int slot, std::uintptr_t data) {
    for (int other = 0; other < static_cast<int>(SLOT_COUNT); ++other) {
        if (other == slot) {
            continue;
        }
        TavernRequest actual{};

        if (!read_pair(slot_address(state.city_base, city, other), actual)) {
            return true;
        }
        state.current[city][other] = actual;
        if (actual.data == data) {
            return true;
        }
    }
    return false;
}

void scan_slot(TavernRequests& state, int city, int slot) {
    TavernRequest& request = state.current[city][slot];
    const bool read = read_pair(slot_address(state.city_base, city, slot), request);

    state.readable[city][slot] = read;
    if (read && usable(request) && state.injected[city][slot] != request) {
        state.backups[city][slot] = {request, -1, -1, false};
        state.injected[city][slot] = {};
        state.sources[state.source_count++] = {city, slot, request};
    } else if (read && !request.name && !request.data) {
        state.injected[city][slot] = {};
    }
    ++state.cursor;
    if (state.cursor == TOTAL_SLOTS) {
        state.cursor = 0;
        state.maintenance = TavernMaintenancePhase::Fill;
    }
}

void fill_slot(TavernRequests& state, std::uintptr_t module_base, int city, int slot) {
    TavernRequest& request = state.current[city][slot];
    const std::uintptr_t address = slot_address(state.city_base, city, slot);
    TavernRequest actual{};

    // A slot may change while the scan is yielded. Never replace a new game
    // request using an earlier snapshot, and recheck neighbouring duplicates.
    if (!state.readable[city][slot] || !read_pair(address, actual) || actual != request) {
        advance_slot(state);
        return;
    }
    if (state.fill == TavernFillPhase::Inspect) {
        if (!request.name && !request.data) {
            state.fill = state.prefer_live ? TavernFillPhase::LiveFirst : TavernFillPhase::Backup;
        } else {
            const TavernBackup& backup = state.backups[city][slot];

            if (!usable(request) || state.injected[city][slot] != request || !backup.borrowed ||
                source_live(state, backup.source_city, backup.source_slot, backup.request)) {
                advance_slot(state);
                return;
            }
            state.fill = TavernFillPhase::LiveFallback;
        }
    }
    if (state.fill == TavernFillPhase::Backup) {
        const TavernBackup& backup = state.backups[city][slot];

        if (usable(backup.request) &&
            (!backup.borrowed || source_live(state, backup.source_city, backup.source_slot, backup.request)) &&
            !duplicate_in_city(state, city, slot, backup.request.data)) {
            const PairWriteResult result = write_pair(state, module_base, city, slot,
                request, backup.request, nullptr);

            if (result != PairWriteResult::Rejected) {
                return;
            }
        }
        state.fill = TavernFillPhase::LiveFallback;
        state.source_cursor = 0;
        return;
    }
    if (state.source_cursor == state.source_count) {
        if (state.fill == TavernFillPhase::LiveFirst) {
            state.fill = TavernFillPhase::Backup;
        } else {
            advance_slot(state);
        }
        return;
    }
    const TavernSource& source = state.sources[state.source_cursor++];

    if (source.city == city || !source_live(state, source.city, source.slot, source.request) ||
        duplicate_in_city(state, city, slot, source.request.data)) {
        return;
    }
    const TavernBackup committed_backup{source.request, source.city, source.slot, true};
    const PairWriteResult result = write_pair(state, module_base, city, slot,
        request, source.request, &committed_backup);

    if (result != PairWriteResult::Rejected) {
        return;
    }
    if (state.fill == TavernFillPhase::LiveFirst) {
        state.fill = TavernFillPhase::Backup;
    } else {
        advance_slot(state);
    }
}

void maintain(TavernRequests& state, std::uintptr_t module_base, const domain::WorkBudget& work_budget) {
    for (std::size_t item = 0; item < WORK_ITEMS_PER_SLICE &&
        state.maintenance != TavernMaintenancePhase::Idle && !state.pending.active &&
        !work_budget.exhausted(); ++item) {
        const int city = static_cast<int>(state.cursor / SLOT_COUNT);
        const int slot = static_cast<int>(state.cursor % SLOT_COUNT);

        if (state.maintenance == TavernMaintenancePhase::Scan) {
            scan_slot(state, city, slot);
        } else {
            fill_slot(state, module_base, city, slot);
        }
    }
}

} // namespace

bool recover_tavern_requests(TavernRequests& state, std::uintptr_t module_base) {
    return recover_pending_pair(state, module_base);
}

void update_tavern_requests(TavernRequests& state, std::uintptr_t module_base,
                            std::uintptr_t game_base, std::uint64_t now_ms, bool enabled,
                            const domain::WorkBudget& work_budget) {
    if (state.pending.active &&
        (work_budget.exhausted() || !recover_tavern_requests(state, module_base))) {
        return;
    }
    if (!enabled) {
        if (state.enabled) {
            state = {};
        }
        return;
    }
    if (work_budget.exhausted() || (state.maintenance == TavernMaintenancePhase::Idle &&
        state.last_tick_ms && now_ms - state.last_tick_ms < 500)) {
        return;
    }
    if (!state.enabled) {
        state = {};
        state.enabled = true;
    }
    const std::uintptr_t city_base = city_root(module_base);

    if (!city_base) {
        state.maintenance = TavernMaintenancePhase::Idle;
        state.last_tick_ms = now_ms;
        return;
    }
    if (state.city_base && state.city_base != city_base) {
        state = {};
        state.city_base = city_base;
        state.enabled = true;
        state.last_tick_ms = now_ms;
        return;
    }
    state.city_base = city_base;
    std::uint8_t phase = 0;

    if (game_base <= 0x10000 || !read_value(game_base + 0xd0, phase) || (phase != 5 && phase != 7)) {
        state.maintenance = TavernMaintenancePhase::Idle;
        state.last_tick_ms = now_ms;
        return;
    }
    if (!state.last_phase) {
        state.last_phase = phase;
        if (phase == 7) {
            begin_maintenance(state, true);
        }
    } else if (phase != state.last_phase) {
        const std::uint8_t previous = state.last_phase;

        state.last_phase = phase;
        state.maintenance = TavernMaintenancePhase::Idle;
        if (previous == 5 && phase == 7) {
            state.phase_change_ms = now_ms;
            state.waiting = true;
        } else if (phase == 5) {
            state.waiting = false;
        }
    }
    if (phase != 7) {
        state.last_tick_ms = now_ms;
        return;
    }
    if (state.waiting) {
        if (now_ms - state.phase_change_ms < 2500) {
            state.last_tick_ms = now_ms;
            return;
        }
        state.waiting = false;
        begin_maintenance(state, true);
    } else if (state.maintenance == TavernMaintenancePhase::Idle) {
        begin_maintenance(state, false);
    }
    maintain(state, module_base, work_budget);
    if (state.maintenance == TavernMaintenancePhase::Idle) {
        state.last_tick_ms = now_ms;
    }
}

} // namespace s8::game
