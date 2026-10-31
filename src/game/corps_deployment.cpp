#include "corps_deployment.h"

#include "city_management.h"
#include "corps_mutation.h"
#include "corps_plan.h"
#include "corps_relationships.h"
#include "officer_roster.h"
#include "../platform/windows/process_memory.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <span>

namespace s8::game {
namespace {

constexpr std::uintptr_t CITY_STRIDE = 0x2a0;
constexpr std::uintptr_t OFFICER_STRIDE = 0x3d0;
constexpr std::size_t MAX_GOVERNOR_ITERATIONS = 512;

template<class T> bool read_value(std::uintptr_t address, T& value) {
    return platform::windows::read_memory(address, std::as_writable_bytes(std::span(&value, 1)));
}

template<class T, std::size_t N> T field(const std::array<std::byte, N>& bytes, std::size_t offset) {
    T result{};

    std::memcpy(&result, bytes.data() + offset, sizeof(result));
    return result;
}

std::uint8_t status(const CorpsRosterEntry& row) {
    return static_cast<std::uint8_t>(row.status_pair);
}

bool employed(std::uint8_t value) {
    return value == 0x18 || value == 0x28 || value == 0x38 || value == 0x48 || value == 0xc8 || value == 0xd8 || value == 0xe8;
}

bool fixed_leader(const CorpsRosterEntry& row) {
    return status(row) == 0xc8 || status(row) == 0xd8;
}

std::uint16_t index_for(const CorpsDeployment& state, std::uint16_t id) {
    return id != 0 && id < state.id_to_row.size() ? state.id_to_row[id] : NO_CORPS_ROW;
}

std::uint16_t id_at(const CorpsDeployment& state, std::uintptr_t address) {
    if (address < state.roster || (address - state.roster) % OFFICER_STRIDE != 0 ||
        (address - state.roster) / OFFICER_STRIDE >= state.rows.size()) { return domain::NO_CITY_OFFICER; }
    std::uint16_t id = 0;

    return read_value(address + 8, id) && id != 0 && id <= domain::CORPS_OFFICER_CAPACITY ? id : domain::NO_CITY_OFFICER;
}

std::uintptr_t current_player_force(std::uintptr_t player) {
    std::uintptr_t city = 0;

    return player > 0x10000 && read_value(player + 0x20, city) ? read_city_force(city) : 0;
}

void rebuild_page(CorpsDeployment& state) {
    domain::CorpsSnapshot& view = state.snapshot;

    view.officer_count = static_cast<std::uint16_t>(state.count);
    view.page_count = static_cast<std::uint16_t>((state.count + domain::CORPS_PAGE_SIZE - 1) / domain::CORPS_PAGE_SIZE);
    const std::uint16_t last_page = view.page_count == 0 ? 0 : static_cast<std::uint16_t>(view.page_count - 1);

    view.page = std::min(view.page, last_page);
    view.visible_count = 0;
    for (std::size_t slot = std::size_t(view.page) * domain::CORPS_PAGE_SIZE;
        slot < state.count && view.visible_count < view.officers.size(); ++slot) {
        const CorpsRosterEntry& row = state.rows[state.order[slot]];

        view.officers[view.visible_count++] = {row.id, status(row), row.loyalty, row.attributes, row.city,
            row.goal, row.reason, row.grouped, row.governor};
    }
}

void finish(CorpsDeployment& state, domain::CorpsStatus result) {
    state.stage = CorpsStage::Idle;
    state.snapshot.status = result;
    state.snapshot.busy = false;
    if (result != domain::CorpsStatus::Complete && result != domain::CorpsStatus::Ready &&
        result != domain::CorpsStatus::RelationshipUnavailable) {
        state.snapshot.plan_valid = false;
    }
    rebuild_page(state);
    ++state.snapshot.revision;
}

void fail(CorpsDeployment& state, domain::CorpsStatus result) {
    if ((state.stage >= CorpsStage::Cities && state.stage <= CorpsStage::Roster) ||
        (state.stage == CorpsStage::Sort && state.sort_kind == CorpsSort::Roster)) {
        state.count = 0;
    }
    state.failure = result;
    state.snapshot.plan_valid = false;
    if (state.journal_count != 0) {
        state.stage = CorpsStage::Rollback;
        state.snapshot.busy = true;
        state.snapshot.status = domain::CorpsStatus::Restoring;
    } else { finish(state, result); }
}

void begin_read(CorpsDeployment& state, bool build, bool apply) {
    state.building = build;
    state.apply_all = apply;
    state.snapshot.status = domain::CorpsStatus::Reading;
    state.snapshot.busy = true;
    state.snapshot.plan_valid = false;
    state.snapshot.relationships_complete = false;
    state.snapshot.completed = 0;
    state.snapshot.total = domain::CORPS_OFFICER_CAPACITY;
    state.snapshot.visible_count = 0;
    state.snapshot.officer_count = 0;
    state.snapshot.moved = 0;
    state.snapshot.swapped = 0;
    state.snapshot.appointed = 0;
    state.snapshot.deferred = 0;
    state.snapshot.cities = {};
    state.id_to_row.fill(NO_CORPS_ROW);
    state.count = 0;
    state.cursor = 0;
    state.stage = CorpsStage::Cities;
    read_value(state.game_root + 0xd0, state.work_phase);
    ++state.snapshot.revision;
}

bool bind(CorpsDeployment& state, std::uintptr_t module, std::uintptr_t root,
    std::uintptr_t player, domain::GameEpoch epoch) {
    const std::uintptr_t table = module && root > 0x10000 && player > 0x10000 ? resolve_city_data(module) : 0;
    const std::uintptr_t roster = table ? resolve_officer_roster_base(module) : 0;
    const std::uintptr_t force = table ? current_player_force(player) : 0;
    const bool same = state.epoch == epoch && state.module_base == module && state.game_root == root && state.player == player;

    if (same && (table == 0 || roster == 0 || force == 0)) {
        if (state.snapshot.available) { state.snapshot.available = false; ++state.snapshot.revision; }
        return false;
    }
    if (same && table == state.city_table && roster == state.roster && force == state.player_force) {
        if (!state.snapshot.available) { state.snapshot.available = true; ++state.snapshot.revision; }
        return true;
    }
    state.module_base = module;
    state.game_root = root;
    state.player = player;
    state.city_table = table;
    state.roster = roster;
    state.player_force = force;
    state.epoch = epoch;
    state.journal_count = 0;
    state.baseline_valid = false;
    state.auto_targets = {};
    state.auto_running = false;
    state.auto_pending = false;
    state.observed_year = 0;
    state.observed_month = 0;
    state.observed_phase = 0;
    state.next_observation_ms = 0;
    state.count = 0;
    state.corps = 0;
    state.snapshot.epoch = epoch;
    state.snapshot.options = {};
    state.snapshot.cities = {};
    state.snapshot.selected_city = domain::NO_CITY;
    state.snapshot.available = table != 0 && roster != 0 && force != 0;
    finish(state, domain::CorpsStatus::SessionChanged);
    if (state.snapshot.available) { begin_read(state, false, false); }
    return state.snapshot.available;
}

bool same_binding(const CorpsDeployment& state) {
    return resolve_city_data(state.module_base) == state.city_table &&
        resolve_officer_roster_base(state.module_base) == state.roster && current_player_force(state.player) == state.player_force;
}

bool read_city(CorpsDeployment& state, std::size_t city) {
    const std::uintptr_t address = state.city_table + city * CITY_STRIDE;
    domain::CorpsCity row;
    std::uintptr_t general = 0;
    std::uintptr_t corps = 0;
    std::uintptr_t governor = 0;

    if (!read_value(address + 0x90, corps) || !read_value(address + 0x98, governor)) { return false; }
    state.city_forces[city] = read_city_force(address);
    state.city_corps[city] = corps;
    state.city_governors[city] = governor;
    row.player_owned = state.city_forces[city] == state.player_force;
    row.current_governor = id_at(state, governor);
    if (corps > 0x10000) {
        if (!read_value(corps + 0x18, row.number) || !read_value(corps + 0x20, general)) { return false; }
        row.general = id_at(state, general);
    }
    for (const CorpsAutoTarget& target : state.auto_targets) {
        row.auto_target = row.auto_target || (target.corps != 0 && target.corps == corps && target.force == state.player_force);
    }
    state.snapshot.cities[city] = row;
    return true;
}

bool read_frontier(CorpsDeployment& state, std::size_t city) {
    domain::CorpsCity& row = state.snapshot.cities[city];
    std::array<std::uintptr_t, 6> neighbors{};

    row.selected = row.player_owned && state.city_corps[city] == state.corps &&
        (state.corps != 0 || city == state.snapshot.selected_city);
    if (!row.player_owned) { return true; }
    if (!read_value(state.city_table + city * CITY_STRIDE + 0x20, neighbors)) { return false; }
    for (const std::uintptr_t pointer : neighbors) {
        const std::uint8_t neighbor = city_index_from_pointer(state.city_table, pointer);

        row.frontline = row.frontline || (neighbor != domain::NO_CITY && state.city_forces[neighbor] != state.player_force);
    }
    return true;
}

bool read_officer(CorpsDeployment& state, std::size_t slot) {
    std::array<std::byte, 0xf0> bytes{};
    CorpsRosterEntry row;

    row.address = state.roster + slot * OFFICER_STRIDE;
    state.parents[slot] = static_cast<std::uint16_t>(slot);
    if (!read_value(row.address, bytes)) { state.rows[slot] = row; return false; }
    row.id = field<std::uint16_t>(bytes, 8);
    row.status_pair = field<std::uint16_t>(bytes, 0x10);
    row.force = field<std::uintptr_t>(bytes, 0x18);
    row.city_pointer = field<std::uintptr_t>(bytes, 0x20);
    row.city = city_index_from_pointer(state.city_table, row.city_pointer);
    row.loyalty = field<std::uint8_t>(bytes, 0xec);
    std::memcpy(row.attributes.data(), bytes.data() + 0xaa, row.attributes.size());
    if (row.id == 0 || row.id > domain::CORPS_OFFICER_CAPACITY) { state.rows[slot] = row; return true; }
    if (row.city != domain::NO_CITY && row.force == state.player_force && employed(status(row)) &&
        state.snapshot.cities[row.city].player_owned) {
        ++state.snapshot.cities[row.city].current_count;
        row.selected = state.snapshot.cities[row.city].selected;
        if (row.selected) { state.work_order[state.count++] = static_cast<std::uint16_t>(slot); }
    }
    const std::uint16_t previous = state.id_to_row[row.id];

    if (previous != NO_CORPS_ROW && state.rows[previous].selected && row.selected) { return false; }
    if (previous == NO_CORPS_ROW || row.selected) { state.id_to_row[row.id] = static_cast<std::uint16_t>(slot); }
    state.rows[slot] = row;
    return true;
}

void observe_calendar(CorpsDeployment& state, std::uint64_t now) {
    if (now < state.next_observation_ms) { return; }
    state.next_observation_ms = now + 500;
    std::uintptr_t manager = 0;
    std::uint16_t year = 0;
    std::uint8_t month = 0;
    std::uint8_t phase = 0;

    if (!read_value(state.module_base + 0x2e98bc8, manager) || manager <= 0x10000 ||
        !read_value(manager + 0x72d0, year) || !read_value(manager + 0x72d2, month) ||
        !read_value(state.game_root + 0xd0, phase) || year == 0 || month == 0 || month > 12) { return; }
    if (state.observed_year != 0 && (year < state.observed_year || (year == state.observed_year && month < state.observed_month))) {
        state.auto_targets = {};
        state.snapshot.options = {};
        state.auto_pending = false;
        state.baseline_valid = false;
        if (state.auto_running) { state.auto_running = false; fail(state, domain::CorpsStatus::SessionChanged); }
        ++state.snapshot.revision;
    }
    state.observed_year = year;
    state.observed_month = month;
    if (phase != 5 && phase != 7) { return; }
    if (state.observed_phase == 7 && phase == 5) {
        state.baseline_valid = false;
        if (state.snapshot.options.each_council) {
            state.auto_pending = true;
            state.auto_cursor = 0;
            state.saved_city = state.snapshot.selected_city;
        }
    }
    state.observed_phase = phase;
}

void next_auto_target(CorpsDeployment& state) {
    if (!state.auto_pending || !state.snapshot.options.each_council || state.observed_phase != 5) {
        state.auto_pending = false;
        state.auto_running = false;
        return;
    }
    if (state.auto_cursor == state.auto_targets.size()) {
        state.auto_pending = false;
        state.auto_running = false;
        state.snapshot.selected_city = state.saved_city;
        begin_read(state, false, false);
        return;
    }
    CorpsAutoTarget& target = state.auto_targets[state.auto_cursor++];
    std::uintptr_t force = 0;
    std::uintptr_t general = 0;
    std::uint64_t number = 0;

    if (target.corps == 0) { return; }
    if (!read_value(target.corps + 0x10, force) || !read_value(target.corps + 0x18, number) ||
        !read_value(target.corps + 0x20, general) || force != state.player_force || force != target.force ||
        number != target.number || id_at(state, general) != target.general) {
        target = {};
        return;
    }
    for (std::size_t city = 0; city < domain::CITY_COUNT; ++city) {
        std::uintptr_t corps = 0;
        const std::uintptr_t address = state.city_table + city * CITY_STRIDE;

        if (read_value(address + 0x90, corps) && corps == target.corps && read_city_force(address) == state.player_force) {
            state.snapshot.selected_city = static_cast<std::uint8_t>(city);
            state.auto_running = true;
            begin_read(state, true, true);
            return;
        }
    }
    target = {};
}

void begin_moves(CorpsDeployment& state) {
    begin_corps_journal(state);
    state.cursor = 0;
    state.stage = CorpsStage::ValidateMoves;
    state.snapshot.busy = true;
    state.snapshot.status = domain::CorpsStatus::ApplyingMoves;
    read_value(state.game_root + 0xd0, state.work_phase);
}

void begin_governors(CorpsDeployment& state) {
    begin_corps_journal(state);
    state.cursor = 0;
    state.iterations = 0;
    state.stage = CorpsStage::RefreshLive;
    state.snapshot.busy = true;
    state.snapshot.status = domain::CorpsStatus::ApplyingGovernors;
    for (domain::CorpsCity& city : state.snapshot.cities) { city.deferred = false; }
    read_value(state.game_root + 0xd0, state.work_phase);
}

void restart_governor_sweep(CorpsDeployment& state) {
    if (++state.iterations > MAX_GOVERNOR_ITERATIONS) { fail(state, domain::CorpsStatus::MissingCycleBuffer); return; }
    state.cursor = 0;
    state.progress = false;
    state.stage = CorpsStage::GovernorMoveSweep;
}

bool planned_mover(const CorpsRosterEntry& row) {
    return (status(row) == 0x18 || status(row) == 0x28) && row.goal != domain::NO_CITY && row.city != row.goal;
}

bool validate_planned_row(CorpsDeployment& state, std::uint16_t index) {
    const std::uint16_t previous_pair = state.rows[index].status_pair;

    if (!refresh_corps_live_row(state, index) || state.rows[index].status_pair != previous_pair) { return false; }
    const CorpsRosterEntry& row = state.rows[index];
    std::uintptr_t source = 0;
    std::uintptr_t destination = 0;

    return row.goal < domain::CITY_COUNT && read_value(row.city_pointer + 0x90, source) && source == state.corps &&
        read_value(state.city_table + row.goal * CITY_STRIDE + 0x90, destination) && destination == state.corps &&
        read_city_force(state.city_table + row.goal * CITY_STRIDE) == state.player_force;
}

void step_moves(CorpsDeployment& state) {
    if (state.cursor == state.count) {
        state.cursor = 0;
        if (state.stage == CorpsStage::ValidateMoves) { state.stage = CorpsStage::ApplyMoves; }
        else if (state.stage == CorpsStage::ApplyMoves) { state.stage = CorpsStage::VerifyMoves; }
        else {
            begin_corps_journal(state);
            if (state.apply_all) { begin_governors(state); }
            else { finish(state, domain::CorpsStatus::Complete); }
        }
        return;
    }
    const std::uint16_t index = state.recommendations[state.cursor++];
    CorpsRosterEntry& row = state.rows[index];

    if (state.stage == CorpsStage::ValidateMoves) {
        if (planned_mover(row) && !validate_planned_row(state, index)) { fail(state, domain::CorpsStatus::OwnershipChanged); }
    } else if (state.stage == CorpsStage::ApplyMoves) {
        if (planned_mover(row)) {
            const domain::CorpsStatus result = move_corps_officer(state, index, row.goal, false);

            if (result != domain::CorpsStatus::Complete) { fail(state, result); }
        }
    } else if ((status(row) == 0x18 || status(row) == 0x28) &&
        (!validate_planned_row(state, index) || row.city != row.goal)) {
        fail(state, domain::CorpsStatus::OwnershipChanged);
    }
}

void step_governors(CorpsDeployment& state) {
    if (state.stage == CorpsStage::RefreshLive) {
        if (state.cursor == state.count) { state.cursor = 0; state.stage = CorpsStage::DeferCities; return; }
        if (!validate_planned_row(state, state.recommendations[state.cursor++])) { fail(state, domain::CorpsStatus::OwnershipChanged); }
    } else if (state.stage == CorpsStage::DeferCities) {
        if (state.cursor == domain::CITY_COUNT) {
            state.cursor = 0;
            state.progress = false;
            state.stage = CorpsStage::PropagateDeferred;
            return;
        }
        const std::uint8_t city_index = static_cast<std::uint8_t>(state.cursor++);
        domain::CorpsCity& city = state.snapshot.cities[city_index];
        const std::uint16_t desired = index_for(state, city.recommended_governor);

        if (!city.selected || desired == NO_CORPS_ROW || fixed_leader(state.rows[desired])) { return; }
        std::uintptr_t governor = 0;

        if (!read_value(state.city_table + city_index * CITY_STRIDE + 0x98, governor)) { fail(state, domain::CorpsStatus::ReadFailed); return; }
        state.city_governors[city_index] = governor;
        city.current_governor = id_at(state, governor);
        if (governor == state.rows[desired].address) { return; }
        const std::uint16_t previous = index_for(state, city.current_governor);

        city.deferred = governor == 0 ? state.rows[desired].status_pair != 0xd328 || state.rows[desired].loyalty != 100 ||
            state.rows[desired].city != city_index : previous == NO_CORPS_ROW || status(state.rows[previous]) != 0xe8;
    } else if (state.stage == CorpsStage::PropagateDeferred) {
        if (state.cursor == domain::CITY_COUNT) {
            state.cursor = 0;
            if (state.progress) { state.progress = false; }
            else { state.stage = CorpsStage::ValidateGovernors; }
            return;
        }
        const domain::CorpsCity& blocked = state.snapshot.cities[state.cursor++];

        if (!blocked.deferred) { return; }
        for (domain::CorpsCity& city : state.snapshot.cities) {
            if (!city.selected || city.deferred) { continue; }
            if ((blocked.current_governor != domain::NO_CITY_OFFICER && city.recommended_governor == blocked.current_governor) ||
                (blocked.recommended_governor != domain::NO_CITY_OFFICER && city.current_governor == blocked.recommended_governor)) {
                city.deferred = true;
                state.progress = true;
            }
        }
    } else if (state.stage == CorpsStage::ValidateGovernors) {
        if (state.cursor < state.count) {
            const CorpsRosterEntry& row = state.rows[state.recommendations[state.cursor++]];

            if ((status(row) == 0x18 || status(row) == 0x28) && row.city != row.goal &&
                (row.goal >= domain::CITY_COUNT || !state.snapshot.cities[row.goal].deferred)) {
                fail(state, domain::CorpsStatus::OwnershipChanged);
            }
            return;
        }
        state.snapshot.deferred = 0;
        for (const domain::CorpsCity& city : state.snapshot.cities) {
            if (city.deferred) { ++state.snapshot.deferred; continue; }
            if (!city.selected || city.target_count == 0) { continue; }
            const std::uint16_t candidate = index_for(state, city.recommended_governor);

            if (candidate == NO_CORPS_ROW || (!fixed_leader(state.rows[candidate]) && state.rows[candidate].loyalty != 100)) {
                fail(state, domain::CorpsStatus::MissingGovernor);
                return;
            }
        }
        restart_governor_sweep(state);
    } else if (state.stage == CorpsStage::GovernorMoveSweep) {
        if (state.cursor == state.count) {
            state.cursor = 0;
            if (state.progress) { restart_governor_sweep(state); }
            else { state.stage = CorpsStage::GovernorSwapSweep; }
            return;
        }
        const std::uint16_t index = state.recommendations[state.cursor++];
        const CorpsRosterEntry& row = state.rows[index];

        if (status(row) != 0x28 || row.goal >= domain::CITY_COUNT || row.city == row.goal || state.snapshot.cities[row.goal].deferred) { return; }
        const domain::CorpsStatus result = move_corps_officer(state, index, row.goal, false);

        if (result != domain::CorpsStatus::Complete) { fail(state, result); }
        else { state.progress = true; }
    } else if (state.stage == CorpsStage::GovernorSwapSweep) {
        if (state.cursor == domain::CITY_COUNT) {
            state.first_candidate = NO_CORPS_ROW;
            for (std::size_t city = 0; city < domain::CITY_COUNT; ++city) {
                const domain::CorpsCity& target = state.snapshot.cities[city];
                const std::uint16_t desired = index_for(state, target.recommended_governor);

                if (!target.selected || target.deferred || desired == NO_CORPS_ROW || fixed_leader(state.rows[desired]) ||
                    state.city_governors[city] == state.rows[desired].address || status(state.rows[desired]) != 0xe8) { continue; }
                state.first_candidate = desired;
                state.group_city = state.rows[desired].city;
                break;
            }
            state.cursor = 0;
            state.stage = state.first_candidate == NO_CORPS_ROW ? CorpsStage::VerifyGroups : CorpsStage::CycleBuffer;
            return;
        }
        const std::uint8_t city = static_cast<std::uint8_t>(state.cursor++);
        const domain::CorpsCity& target = state.snapshot.cities[city];
        const std::uint16_t desired = index_for(state, target.recommended_governor);

        if (!target.selected || target.deferred || desired == NO_CORPS_ROW || fixed_leader(state.rows[desired]) ||
            state.city_governors[city] == state.rows[desired].address) { return; }
        if (status(state.rows[desired]) != 0x28 || state.rows[desired].city != city) { return; }
        if (!refresh_corps_live_row(state, desired) || state.rows[desired].loyalty != 100) { fail(state, domain::CorpsStatus::OwnershipChanged); return; }
        if (state.city_governors[city] == 0) {
            state.first_candidate = desired;
            state.plan_city = city;
            state.cursor = 0;
            state.stage = CorpsStage::FirstGovernorCheck;
            return;
        }
        const domain::CorpsStatus result = swap_corps_governor(state, city, desired);

        if (result != domain::CorpsStatus::Complete) { fail(state, result); }
        else { restart_governor_sweep(state); }
    } else if (state.stage == CorpsStage::CycleBuffer) {
        if (state.cursor == state.count) { fail(state, domain::CorpsStatus::MissingCycleBuffer); return; }
        const std::uint16_t candidate = state.recommendations[state.cursor++];
        CorpsRosterEntry& row = state.rows[candidate];

        if (row.governor || row.grouped || row.loyalty != 100 || status(row) != 0x28 || row.city != row.goal) { return; }
        if (!refresh_corps_live_row(state, candidate) || row.loyalty != 100 || status(row) != 0x28 || row.city != row.goal ||
            state.group_city >= domain::CITY_COUNT || state.city_governors[state.group_city] != state.rows[state.first_candidate].address) {
            fail(state, domain::CorpsStatus::OwnershipChanged);
            return;
        }
        domain::CorpsStatus result = move_corps_officer(state, candidate, state.group_city, false);

        if (result == domain::CorpsStatus::Complete) { result = swap_corps_governor(state, state.group_city, candidate); }
        if (result != domain::CorpsStatus::Complete) { fail(state, result); }
        else { restart_governor_sweep(state); }
    } else if (state.stage == CorpsStage::FirstGovernorCheck) {
        if (state.cursor < state.count) {
            const std::uint16_t index = state.recommendations[state.cursor++];

            if (!refresh_corps_live_row(state, index)) { fail(state, domain::CorpsStatus::OwnershipChanged); return; }
            const CorpsRosterEntry& row = state.rows[index];

            if (row.city == state.plan_city && (fixed_leader(row) || status(row) == 0xe8)) { fail(state, domain::CorpsStatus::OwnershipChanged); }
            return;
        }
        const domain::CorpsStatus result = appoint_corps_governor(state, state.plan_city, state.first_candidate);

        if (result != domain::CorpsStatus::Complete) { fail(state, result); }
        else { restart_governor_sweep(state); }
    } else if (state.stage == CorpsStage::VerifyGroups) {
        if (state.cursor == state.count) { state.cursor = 0; state.stage = CorpsStage::VerifyGovernors; return; }
        const std::uint16_t index = state.recommendations[state.cursor++];

        if (state.rows[index].grouped && (!refresh_corps_live_row(state, index) || state.rows[index].city != state.rows[index].goal)) {
            fail(state, domain::CorpsStatus::OwnershipChanged);
        }
    } else if (state.stage == CorpsStage::VerifyGovernors) {
        if (state.cursor == domain::CITY_COUNT) {
            begin_corps_journal(state);
            finish(state, domain::CorpsStatus::Complete);
            return;
        }
        const std::uint8_t city = static_cast<std::uint8_t>(state.cursor++);
        const domain::CorpsCity& target = state.snapshot.cities[city];
        const std::uint16_t desired = index_for(state, target.recommended_governor);
        std::uintptr_t governor = 0;

        if (!target.selected || target.deferred || desired == NO_CORPS_ROW || fixed_leader(state.rows[desired])) { return; }
        if (!refresh_corps_live_row(state, desired) || !read_value(state.city_table + city * CITY_STRIDE + 0x98, governor) ||
            governor != state.rows[desired].address || status(state.rows[desired]) != 0xe8 || state.rows[desired].loyalty != 100 ||
            state.rows[desired].city != city) { fail(state, domain::CorpsStatus::OwnershipChanged); }
    }
}

void step_read(CorpsDeployment& state) {
    if (state.stage == CorpsStage::Cities) {
        if (state.cursor < domain::CITY_COUNT) {
            if (!read_city(state, state.cursor++)) { fail(state, domain::CorpsStatus::ReadFailed); }
            return;
        }
        if (state.snapshot.selected_city >= domain::CITY_COUNT || !state.snapshot.cities[state.snapshot.selected_city].player_owned) {
            std::uintptr_t current = 0;

            read_value(state.player + 0x20, current);
            state.snapshot.selected_city = city_index_from_pointer(state.city_table, current);
            if (state.snapshot.selected_city >= domain::CITY_COUNT || !state.snapshot.cities[state.snapshot.selected_city].player_owned) {
                for (std::size_t city = 0; city < domain::CITY_COUNT; ++city) {
                    if (state.snapshot.cities[city].player_owned) { state.snapshot.selected_city = static_cast<std::uint8_t>(city); break; }
                }
            }
        }
        if (state.snapshot.selected_city >= domain::CITY_COUNT) { fail(state, domain::CorpsStatus::InvalidRequest); return; }
        state.corps = state.city_corps[state.snapshot.selected_city];
        if (state.building && state.corps <= 0x10000) { fail(state, domain::CorpsStatus::InvalidRequest); return; }
        state.cursor = 0;
        state.stage = CorpsStage::Frontiers;
    } else if (state.stage == CorpsStage::Frontiers) {
        if (state.cursor == domain::CITY_COUNT) { state.cursor = 0; state.stage = CorpsStage::Roster; return; }
        if (!read_frontier(state, state.cursor++)) { fail(state, domain::CorpsStatus::ReadFailed); }
    } else if (state.stage == CorpsStage::Roster) {
        if (state.cursor == domain::CORPS_OFFICER_CAPACITY) {
            if (state.building && state.count == 0) { fail(state, domain::CorpsStatus::InvalidRequest); return; }
            state.sort_count = state.count;
            begin_corps_sort(state, CorpsSort::Roster, state.building ? CorpsStage::RelationCandidate : CorpsStage::Idle);
            return;
        }
        if (!read_officer(state, state.cursor++)) { fail(state, domain::CorpsStatus::ReadFailed); }
    }
}

} // namespace

domain::CorpsStatus request_corps_deployment(CorpsDeployment& state, std::uintptr_t module_base,
    std::uintptr_t game_root, std::uintptr_t player, domain::GameEpoch epoch, const domain::CorpsRequest& request) {
    if (state.stopping) { return domain::CorpsStatus::Cancelled; }
    if (request.action == domain::CorpsAction::Cancel) {
        state.auto_pending = false;
        state.auto_running = false;
        fail(state, domain::CorpsStatus::Cancelled);
        return domain::CorpsStatus::Cancelled;
    }
    if (!bind(state, module_base, game_root, player, epoch)) { return domain::CorpsStatus::ReadFailed; }
    if (request.action == domain::CorpsAction::Refresh && !state.building &&
        ((state.stage >= CorpsStage::Cities && state.stage <= CorpsStage::Roster) ||
            (state.stage == CorpsStage::Sort && state.sort_kind == CorpsSort::Roster))) {
        return domain::CorpsStatus::Reading;
    }
    if (state.stage != CorpsStage::Idle || state.journal_count != 0) { return domain::CorpsStatus::Busy; }
    if (request.action == domain::CorpsAction::SetPage) {
        state.snapshot.page = request.page;
        rebuild_page(state);
        ++state.snapshot.revision;
        return domain::CorpsStatus::Complete;
    }
    if (request.action != domain::CorpsAction::SetOptions && request.action != domain::CorpsAction::ToggleAutoTarget) {
        state.auto_pending = false;
        state.auto_running = false;
    }
    state.request = request;
    if (request.action == domain::CorpsAction::SetOptions) {
        if (state.snapshot.options != request.options) { state.snapshot.plan_valid = false; }
        state.snapshot.options = request.options;
        if (!request.options.each_council) { state.auto_pending = false; }
        ++state.snapshot.revision;
        return domain::CorpsStatus::Complete;
    }
    if (request.action == domain::CorpsAction::SelectCity) {
        if (request.city >= domain::CITY_COUNT || !state.snapshot.cities[request.city].player_owned) { return domain::CorpsStatus::InvalidRequest; }
        state.snapshot.selected_city = request.city;
        state.snapshot.page = 0;
        begin_read(state, false, false);
        return domain::CorpsStatus::Reading;
    }
    if (request.action == domain::CorpsAction::Refresh || request.action == domain::CorpsAction::BuildPlan || request.action == domain::CorpsAction::ResetTargets) {
        if (request.action == domain::CorpsAction::ResetTargets) { state.baseline_valid = false; }
        begin_read(state, request.action != domain::CorpsAction::Refresh, false);
        return domain::CorpsStatus::Reading;
    }
    if (request.action == domain::CorpsAction::ToggleAutoTarget) {
        if (state.corps <= 0x10000 || state.snapshot.selected_city >= domain::CITY_COUNT) { return domain::CorpsStatus::InvalidRequest; }
        CorpsAutoTarget* empty = nullptr;
        bool removed = false;

        for (CorpsAutoTarget& target : state.auto_targets) {
            if (target.corps == state.corps) { target = {}; removed = true; break; }
            if (target.corps == 0 && !empty) { empty = &target; }
        }
        if (!removed) {
            if (!empty) { return domain::CorpsStatus::CapacityExceeded; }
            const domain::CorpsCity& city = state.snapshot.cities[state.snapshot.selected_city];

            *empty = {state.corps, state.player_force, city.number, city.general};
        }
        for (std::size_t city = 0; city < domain::CITY_COUNT; ++city) {
            if (state.city_corps[city] == state.corps) { state.snapshot.cities[city].auto_target = !removed; }
        }
        ++state.snapshot.revision;
        return domain::CorpsStatus::Complete;
    }
    if (request.action == domain::CorpsAction::ApplyMoves || request.action == domain::CorpsAction::ApplyGovernors || request.action == domain::CorpsAction::ApplyAll) {
        if (!state.snapshot.plan_valid) {
            if (request.action != domain::CorpsAction::ApplyAll) { return domain::CorpsStatus::InvalidRequest; }
            begin_read(state, true, true);
            return domain::CorpsStatus::Reading;
        }
        state.apply_all = request.action == domain::CorpsAction::ApplyAll;
        if (request.action == domain::CorpsAction::ApplyGovernors) { begin_governors(state); }
        else { begin_moves(state); }
        return state.snapshot.status;
    }
    if (!same_binding(state)) { return domain::CorpsStatus::OwnershipChanged; }
    const std::uint16_t index = index_for(state, request.officer);
    const std::uint16_t other = index_for(state, request.other_officer);

    if (index == NO_CORPS_ROW || !state.rows[index].selected) { return domain::CorpsStatus::InvalidRequest; }
    begin_corps_journal(state);
    domain::CorpsStatus result = domain::CorpsStatus::InvalidRequest;

    if (request.action == domain::CorpsAction::MoveOfficer) {
        result = move_corps_officer(state, index, request.target_city, true);
    } else if (request.action == domain::CorpsAction::SwapGeneral) {
        result = swap_corps_general(state, index, other);
    } else if (request.action == domain::CorpsAction::SwapGovernor && request.city < domain::CITY_COUNT &&
        state.city_governors[request.city] == state.rows[index].address) {
        result = swap_corps_governor(state, request.city, other);
    }
    state.snapshot.plan_valid = false;
    if (result == domain::CorpsStatus::Complete) {
        begin_corps_journal(state);
        finish(state, result);
        begin_read(state, false, false);
    }
    else { fail(state, result); }
    return result;
}

void tick_corps_deployment(CorpsDeployment& state, std::uintptr_t module_base, std::uintptr_t game_root,
    std::uintptr_t player, domain::GameEpoch epoch, std::uint64_t real_milliseconds, std::size_t max_units) {
    if (state.stopping || !bind(state, module_base, game_root, player, epoch)) { return; }
    observe_calendar(state, real_milliseconds);
    if (state.stage == CorpsStage::Idle) { next_auto_target(state); }
    if (state.stage != CorpsStage::Idle && state.stage != CorpsStage::Rollback) {
        std::uint8_t phase = 0;

        if (!read_value(state.game_root + 0xd0, phase) || phase != state.work_phase) {
            state.auto_pending = false;
            state.auto_running = false;
            fail(state, domain::CorpsStatus::OwnershipChanged);
        }
    }
    const bool restoring = state.stage == CorpsStage::Rollback;
    const std::uint64_t write_sequence = state.write_sequence;
    const std::size_t budget = std::min<std::size_t>(max_units, restoring ? 32 : 256);

    for (std::size_t unit = 0; unit < budget && state.stage != CorpsStage::Idle; ++unit) {
        if (state.stage >= CorpsStage::Cities && state.stage <= CorpsStage::Roster) {
            step_read(state);
        } else if (state.stage == CorpsStage::Sort) {
            const CorpsSort kind = state.sort_kind;

            if (step_corps_sort(state) && kind == CorpsSort::Roster) {
                if (state.building) { begin_corps_relationships(state); }
                else { finish(state, domain::CorpsStatus::Ready); }
            }
        } else if (state.stage >= CorpsStage::RelationCandidate && state.stage <= CorpsStage::RelationMerge) {
            if (step_corps_relationships(state)) { begin_corps_plan(state); }
            else if (state.stage == CorpsStage::Idle) { finish(state, state.failure); }
        } else if (state.stage >= CorpsStage::PlanInitialize && state.stage <= CorpsStage::PlanFinish) {
            if (step_corps_plan(state)) {
                if (state.apply_all) { begin_moves(state); }
                else { finish(state, state.snapshot.relationships_complete ? domain::CorpsStatus::Complete : domain::CorpsStatus::RelationshipUnavailable); }
            } else if (state.stage == CorpsStage::Idle) { finish(state, state.failure); }
        } else if (state.stage >= CorpsStage::ValidateMoves && state.stage <= CorpsStage::VerifyMoves) {
            step_moves(state);
        } else if (state.stage >= CorpsStage::RefreshLive && state.stage <= CorpsStage::VerifyGovernors) {
            step_governors(state);
        } else if (state.stage == CorpsStage::Rollback) {
            if (!restore_corps_journal_unit(state)) {
                state.snapshot.status = domain::CorpsStatus::RollbackFailed;
                break;
            }
            if (state.journal_count == 0) { finish(state, state.failure); }
        }
        state.snapshot.completed = state.cursor;
        if (!restoring && state.write_sequence != write_sequence) { break; }
    }
    if (state.snapshot.busy) { ++state.snapshot.revision; }
}

bool stop_corps_deployment(CorpsDeployment& state, std::uintptr_t module_base, domain::GameEpoch epoch) {
    state.stopping = true;
    state.auto_pending = false;
    state.auto_running = false;
    if (state.journal_count != 0) {
        const std::uintptr_t table = resolve_city_data(module_base);
        const std::uintptr_t roster = resolve_officer_roster_base(module_base);

        if (epoch != state.epoch || module_base != state.module_base || (table != 0 && table != state.city_table) || (roster != 0 && roster != state.roster)) {
            state.journal_count = 0;
        } else if (table == 0 || roster == 0) { return false; }
        for (std::size_t count = 0; count < 32 && state.journal_count != 0; ++count) {
            if (!restore_corps_journal_unit(state)) { return false; }
        }
        if (state.journal_count != 0) { return false; }
    }
    finish(state, domain::CorpsStatus::Cancelled);
    return true;
}

} // namespace s8::game
