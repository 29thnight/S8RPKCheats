#include "officer_notifications.h"
#include "officer_roster.h"
#include "../platform/windows/process_memory.h"

#include <algorithm>
#include <cstring>

namespace s8::game {
namespace {

// RoninMonitor.cpp at WOOSEOK99/S8RPKCheats@068ca1d: 07->05 announces new
// status-58 ronins; 05->07 compares status-88 deaths and nonzero force changes.
// A scan is published only after all slots complete in one observed phase.
constexpr std::uint8_t COUNCIL = 0x05;
constexpr std::uint8_t DOMESTIC = 0x07;
constexpr std::uint8_t RONIN = 0x58;
constexpr std::uint8_t DEAD = 0x88;
constexpr std::size_t MAX_SLOTS_PER_TICK = 128;

template<typename T>
bool read_value(std::uintptr_t address, T& value) {
    return platform::windows::read_memory(address, std::as_writable_bytes(std::span<T>(&value, 1)));
}

std::uintptr_t resolve_cities(std::uintptr_t base) {
    std::uintptr_t first = 0;
    std::uintptr_t second = 0;
    std::uintptr_t cities = 0;

    return read_value(base + 0x034c8630, first) && first > 0x10000 &&
        read_value(first, second) && second > 0x10000 && read_value(second, cities) && cities > 0x10000
        ? cities : 0;
}

std::uint16_t force_lord(std::uintptr_t force) {
    std::uintptr_t lord = 0;
    std::uint16_t id = 0;

    return force > 0x10000 && read_value(force + 0xc0, lord) && lord > 0x10000 &&
        read_value(lord + 8, id) && id >= 1 && id <= OFFICER_NOTICE_SLOT_COUNT ? id : 0;
}

bool read_officer(std::uintptr_t address, std::uintptr_t cities,
    std::uint16_t& id, OfficerNoticeBaseline& snapshot) {
    std::array<std::byte, 0x20> fields{};
    std::uintptr_t city = 0;

    if (!platform::windows::read_memory(address + 8, fields)) {
        return false;
    }
    std::memcpy(&id, fields.data(), sizeof(id));
    std::memcpy(&snapshot.status, fields.data() + 8, sizeof(snapshot.status));
    std::memcpy(&snapshot.force, fields.data() + 0x10, sizeof(snapshot.force));
    std::memcpy(&city, fields.data() + 0x18, sizeof(city));
    if (!id || id > OFFICER_NOTICE_SLOT_COUNT) {
        return false;
    }
    snapshot.lord = force_lord(snapshot.force);
    snapshot.city = cities && city >= cities && (city - cities) % 0x2a0 == 0 &&
        (city - cities) / 0x2a0 < 51 ? static_cast<std::uint8_t>((city - cities) / 0x2a0) : 0xff;
    snapshot.valid = true;
    return true;
}

void append_history(OfficerNoticeHistory& history, const domain::OfficerNotice& notice) {
    ++history.sequence;
    if (history.count < history.entries.size()) {
        history.entries[(history.begin + history.count) % history.entries.size()] = notice;
        ++history.count;
    } else {
        history.entries[history.begin] = notice;
        history.begin = (history.begin + 1) % history.entries.size();
    }
}

void merge_history(OfficerNoticeHistory& destination, const OfficerNoticeHistory& source) {
    for (std::size_t index = 0; index < source.count; ++index) {
        append_history(destination, source.entries[(source.begin + index) % source.entries.size()]);
    }
}

void start_scan(OfficerNotifications& state, std::uintptr_t base, std::uint8_t phase,
    bool ronins, bool changes, bool announce_ronins, bool announce_changes) {
    state.scanning = ronins || changes;
    state.scan_phase = phase;
    state.refresh_ronins = ronins;
    state.refresh_changes = changes;
    state.notify_ronins = announce_ronins;
    state.notify_changes = announce_changes;
    state.city_base = resolve_cities(base);
    state.next_slot = 0;
    state.staging.fill({});
    state.staging_ronins.fill(false);
    state.pending = {};
    state.pending_recruits.begin = 0;
    state.pending_recruits.count = 0;
    state.pending_deaths.begin = 0;
    state.pending_deaths.count = 0;
}

void collect_changes(OfficerNotifications& state, std::uint16_t id, const OfficerNoticeBaseline& current) {
    if (state.notify_ronins && current.status == RONIN && !state.was_ronin[id]) {
        domain::OfficerNotice notice{};

        notice.kind = domain::OfficerNoticeKind::Ronin;
        notice.officer = id;
        notice.city = current.city;
        if (state.pending.ronin_count < state.pending.ronins.size()) {
            state.pending.ronins[state.pending.ronin_count++] = notice;
        }
        ++state.pending.total_ronins;
    }
    if (!state.notify_changes || !state.baseline[id].valid) {
        return;
    }

    const OfficerNoticeBaseline& before = state.baseline[id];
    domain::OfficerNotice notice{};

    notice.officer = id;
    notice.previous_lord = before.lord;
    notice.current_lord = current.lord;
    notice.previous_has_force = before.force > 0x10000;
    notice.current_has_force = current.force > 0x10000;
    notice.city = current.city;
    if (before.status != DEAD && current.status == DEAD) {
        notice.kind = domain::OfficerNoticeKind::Died;
        notice.city = before.city;
        if (state.pending.death_count < state.pending.deaths.size()) {
            state.pending.deaths[state.pending.death_count++] = notice;
        }
        ++state.pending.total_deaths;
        append_history(state.pending_deaths, notice);
    } else if (current.status != DEAD && before.force != current.force && current.force > 0x10000) {
        notice.kind = domain::OfficerNoticeKind::Recruited;
        if (state.pending.recruit_count < state.pending.recruits.size()) {
            state.pending.recruits[state.pending.recruit_count++] = notice;
        }
        ++state.pending.total_recruits;
        append_history(state.pending_recruits, notice);
    }
}

void finish_scan(OfficerNotifications& state) {
    if (state.refresh_ronins) {
        state.was_ronin = state.staging_ronins;
        state.ronin_baseline_ready = true;
        if (state.notify_ronins && state.pending.total_ronins) {
            state.view.ronins = state.pending.ronins;
            state.view.ronin_count = state.pending.ronin_count;
            state.view.total_ronins = state.pending.total_ronins;
            ++state.view.ronin_sequence;
        }
    }
    if (state.refresh_changes) {
        state.baseline = state.staging;
        state.change_baseline_ready = true;
        if (state.notify_changes && (state.pending.total_recruits || state.pending.total_deaths)) {
            state.view.recruits = state.pending.recruits;
            state.view.deaths = state.pending.deaths;
            state.view.recruit_count = state.pending.recruit_count;
            state.view.death_count = state.pending.death_count;
            state.view.total_recruits = state.pending.total_recruits;
            state.view.total_deaths = state.pending.total_deaths;
            ++state.view.changes_sequence;
            merge_history(state.history, state.pending_recruits);
            merge_history(state.history, state.pending_deaths);
        }
    }
    state.scanning = false;
}

} // namespace

void reset_officer_notifications(OfficerNotifications& state, domain::GameEpoch epoch) {
    const std::uint64_t ronin_sequence = state.view.ronin_sequence + 1;
    const std::uint64_t changes_sequence = state.view.changes_sequence + 1;
    const bool new_epoch = state.epoch != epoch;

    state.epoch = epoch;
    state.hero = 0;
    state.roster = 0;
    state.city_base = 0;
    state.last_phase = 0;
    state.scanning = false;
    state.enabled_ronins = false;
    state.enabled_changes = false;
    state.ronin_baseline_ready = false;
    state.change_baseline_ready = false;
    if (new_epoch) {
        state.history.begin = 0;
        state.history.count = 0;
        state.history.sequence = 0;
    }
    state.view = {};
    state.view.epoch = epoch;
    state.view.ronin_sequence = ronin_sequence;
    state.view.changes_sequence = changes_sequence;
}

void tick_officer_notifications(OfficerNotifications& state, std::uintptr_t module_base,
    domain::GameEpoch epoch, std::uintptr_t hero, std::uint8_t phase, bool in_battle,
    bool monitor_ronins, bool monitor_changes, std::size_t slot_budget,
    const domain::WorkBudget& work_budget) {
    if (!monitor_ronins && !monitor_changes) {
        if (state.enabled_ronins || state.enabled_changes || state.scanning) {
            reset_officer_notifications(state, epoch);
        }
        return;
    }
    if (state.epoch != epoch || (hero && state.hero && state.hero != hero)) {
        reset_officer_notifications(state, epoch);
    }
    if (!module_base || !hero || in_battle || (phase != COUNCIL && phase != DOMESTIC) ||
        !slot_budget || work_budget.exhausted()) {
        return;
    }

    const std::uintptr_t roster = resolve_officer_roster_base(module_base);

    if (!roster) {
        return;
    }
    if (state.roster && state.roster != roster) {
        reset_officer_notifications(state, epoch);
    }
    state.hero = hero;
    state.roster = roster;

    const bool changed_options = state.enabled_ronins != monitor_ronins || state.enabled_changes != monitor_changes;

    state.enabled_ronins = monitor_ronins;
    state.enabled_changes = monitor_changes;
    if (!monitor_ronins) {
        state.ronin_baseline_ready = false;
    }
    if (!monitor_changes) {
        state.change_baseline_ready = false;
    }
    if (phase != state.last_phase) {
        const std::uint8_t previous = state.last_phase;

        state.last_phase = phase;
        if (previous == DOMESTIC && phase == COUNCIL) {
            start_scan(state, module_base, phase, monitor_ronins, monitor_changes,
                monitor_ronins && state.ronin_baseline_ready, false);
        } else if (previous == COUNCIL && phase == DOMESTIC) {
            start_scan(state, module_base, phase, monitor_ronins && !state.ronin_baseline_ready,
                monitor_changes, false, monitor_changes && state.change_baseline_ready);
        } else {
            start_scan(state, module_base, phase, monitor_ronins, monitor_changes, false, false);
        }
    } else if (state.scanning && changed_options) {
        start_scan(state, module_base, phase,
            monitor_ronins && (state.refresh_ronins || !state.ronin_baseline_ready),
            monitor_changes && (state.refresh_changes || !state.change_baseline_ready),
            monitor_ronins && state.notify_ronins, monitor_changes && state.notify_changes);
    } else if (!state.scanning &&
        ((monitor_ronins && !state.ronin_baseline_ready) || (monitor_changes && !state.change_baseline_ready))) {
        start_scan(state, module_base, phase, monitor_ronins && !state.ronin_baseline_ready,
            monitor_changes && !state.change_baseline_ready, false, false);
    }
    if (!state.scanning) {
        return;
    }

    const std::size_t end = std::min(state.next_slot + std::min(slot_budget, MAX_SLOTS_PER_TICK),
        OFFICER_NOTICE_SLOT_COUNT);

    for (; state.next_slot < end && !work_budget.exhausted(); ++state.next_slot) {
        std::uint16_t id = 0;
        OfficerNoticeBaseline current{};

        if (!read_officer(roster + state.next_slot * 0x3d0, state.city_base, id, current) ||
            state.staging[id].valid) {
            continue;
        }
        state.staging[id] = current;
        state.staging_ronins[id] = current.status == RONIN;
        collect_changes(state, id, current);
    }
    if (state.next_slot == OFFICER_NOTICE_SLOT_COUNT && !work_budget.exhausted()) {
        finish_scan(state);
    }
}

void update_ronin_notice_baseline(OfficerNotifications& state, std::uint16_t officer, std::uint8_t status) {
    if (officer && officer < state.was_ronin.size()) {
        state.was_ronin[officer] = status == RONIN;
    }
}

std::size_t copy_officer_notice_history(const OfficerNotifications& state,
    std::span<domain::OfficerNotice> destination) {
    const std::size_t count = std::min(destination.size(), state.history.count);
    const std::size_t begin = (state.history.begin + state.history.count - count) % state.history.entries.size();

    for (std::size_t index = 0; index < count; ++index) {
        destination[index] = state.history.entries[(begin + index) % state.history.entries.size()];
    }
    return count;
}

} // namespace s8::game
