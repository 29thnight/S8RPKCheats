#include "corps_plan.h"

#include <algorithm>
#include <limits>

namespace s8::game {
namespace {

std::uint8_t status(const CorpsRosterEntry& row) {
    return static_cast<std::uint8_t>(row.status_pair);
}

bool movable(const CorpsRosterEntry& row) {
    return status(row) == 0x18 || status(row) == 0x28 || status(row) == 0xe8;
}

bool eligible_governor(const CorpsRosterEntry& row) {
    return row.loyalty == 100 && (status(row) == 0x28 || status(row) == 0xe8);
}

unsigned combat_score(const CorpsRosterEntry& row) {
    return unsigned(row.attributes[0]) + row.attributes[1];
}

unsigned rear_score(const CorpsRosterEntry& row) {
    return unsigned(row.attributes[3]) + row.attributes[4];
}

unsigned front_score(const CorpsRosterEntry& row) {
    return std::max(combat_score(row), unsigned(row.attributes[2]) * 2);
}

unsigned governor_score(const CorpsRosterEntry& row, bool frontline) {
    return frontline ? combat_score(row) : rear_score(row);
}

unsigned status_rank(std::uint8_t value) {
    switch (value) {
    case 0xc8: return 0;
    case 0xd8: return 1;
    case 0xe8: return 2;
    case 0x18: return 3;
    case 0x38: return 4;
    case 0x48: return 5;
    default: return 6;
    }
}

bool precedes(const CorpsRosterEntry& first, const CorpsRosterEntry& second, CorpsSort kind) {
    if (kind == CorpsSort::Roster) {
        const unsigned left = status_rank(status(first));
        const unsigned right = status_rank(status(second));

        if (left != right) { return left < right; }
        if (first.attributes[0] != second.attributes[0]) { return first.attributes[0] > second.attributes[0]; }
        if (first.attributes[1] != second.attributes[1]) { return first.attributes[1] > second.attributes[1]; }
    } else if (kind == CorpsSort::Rear) {
        if (first.loyalty != second.loyalty) { return first.loyalty < second.loyalty; }
        if (rear_score(first) != rear_score(second)) { return rear_score(first) > rear_score(second); }
    } else if (kind == CorpsSort::Adviser) {
        if (first.attributes[2] != second.attributes[2]) { return first.attributes[2] > second.attributes[2]; }
    } else if (kind == CorpsSort::Front) {
        if (front_score(first) != front_score(second)) { return front_score(first) > front_score(second); }
        if (combat_score(first) != combat_score(second)) { return combat_score(first) > combat_score(second); }
    } else if (rear_score(first) != rear_score(second)) {
        return rear_score(first) > rear_score(second);
    }
    return first.id < second.id;
}

std::uint8_t best_available_city(const CorpsDeployment& state, bool frontline) {
    std::uint8_t best = domain::NO_CITY;
    unsigned remaining = 0;

    for (std::size_t city = 0; city < domain::CITY_COUNT; ++city) {
        const domain::CorpsCity& row = state.snapshot.cities[city];
        const unsigned available = row.target_count > row.assigned_count ? row.target_count - row.assigned_count : 0;

        if (!row.selected || row.frontline != frontline || available == 0) { continue; }
        if (best == domain::NO_CITY || available > remaining ||
            (available == remaining && row.assigned_count < state.snapshot.cities[best].assigned_count)) {
            best = static_cast<std::uint8_t>(city);
            remaining = available;
        }
    }
    return best;
}

void assign(CorpsDeployment& state, std::uint16_t index, std::uint8_t city, domain::CorpsReason reason) {
    CorpsRosterEntry& row = state.rows[index];

    if (row.used || state.assigned_count >= state.recommendations.size()) {
        state.failure = domain::CorpsStatus::CapacityExceeded;
        state.stage = CorpsStage::Idle;
        return;
    }
    if (city >= domain::CITY_COUNT || !state.snapshot.cities[city].selected) { city = row.city; }
    row.goal = city;
    row.used = true;
    row.reason = reason;
    row.assignment_order = static_cast<std::uint16_t>(state.assigned_count);
    state.recommendations[state.assigned_count++] = index;
    if (city < domain::CITY_COUNT) { ++state.snapshot.cities[city].assigned_count; }
}

std::uint8_t group_destination(const CorpsDeployment& state, bool frontline) {
    std::uint8_t best = domain::NO_CITY;
    int remaining = std::numeric_limits<int>::min();
    unsigned present = 0;

    for (std::size_t city = 0; city < domain::CITY_COUNT; ++city) {
        const domain::CorpsCity& row = state.snapshot.cities[city];
        const int available = int(row.target_count) - row.assigned_count;

        if (!row.selected || row.frontline != frontline || row.current_count == 0) { continue; }
        if (best == domain::NO_CITY || state.group_members[city] > present ||
            (state.group_members[city] == present && available > remaining)) {
            best = static_cast<std::uint8_t>(city);
            present = state.group_members[city];
            remaining = available;
        }
    }
    return best;
}

void grow_group_target(CorpsDeployment& state, std::uint8_t city) {
    domain::CorpsCity& target = state.snapshot.cities[city];
    const std::size_t needed = target.assigned_count + state.group_movable;
    std::size_t shortage = needed > target.target_count ? needed - target.target_count : 0;

    target.target_count = static_cast<std::uint16_t>(std::max<std::size_t>(target.target_count, needed));
    for (unsigned pass = 0; pass < 2 && shortage != 0; ++pass) {
        for (std::size_t donor = 0; donor < domain::CITY_COUNT && shortage != 0; ++donor) {
            domain::CorpsCity& source = state.snapshot.cities[donor];
            const unsigned minimum = pass == 0 ? std::max<unsigned>(source.assigned_count, 1) : source.assigned_count;

            if (!source.selected || donor == city || source.target_count <= minimum) { continue; }
            const std::size_t take = std::min<std::size_t>(shortage, source.target_count - minimum);

            source.target_count -= static_cast<std::uint16_t>(take);
            shortage -= take;
        }
    }
}

void begin_bucket(CorpsDeployment& state, std::size_t bucket) {
    state.secondary = bucket;
    state.cursor = 0;
    state.sort_count = 0;
    state.stage = CorpsStage::Buckets;
}

void next_plan_city(CorpsDeployment& state) {
    state.plan_city = state.plan_city == domain::NO_CITY ? 0 : static_cast<std::uint8_t>(state.plan_city + 1);
    state.cursor = 0;
    state.best_row = NO_CORPS_ROW;
}

bool better_governor(const CorpsDeployment& state, std::uint16_t candidate, std::uint16_t best, bool frontline) {
    return best == NO_CORPS_ROW || governor_score(state.rows[candidate], frontline) > governor_score(state.rows[best], frontline) ||
        (governor_score(state.rows[candidate], frontline) == governor_score(state.rows[best], frontline) &&
            state.rows[candidate].id < state.rows[best].id);
}

} // namespace

std::uint16_t corps_group_root(CorpsDeployment& state, std::uint16_t row) {
    std::uint16_t root = row;

    while (state.parents[root] != root) { root = state.parents[root]; }
    while (state.parents[row] != row) {
        const std::uint16_t next = state.parents[row];

        state.parents[row] = root;
        row = next;
    }
    return root;
}

void unite_corps_group(CorpsDeployment& state, std::uint16_t first, std::uint16_t second) {
    const std::uint16_t left = corps_group_root(state, first);
    const std::uint16_t right = corps_group_root(state, second);

    if (left != right) { state.parents[right] = left; }
}

void begin_corps_sort(CorpsDeployment& state, CorpsSort kind, CorpsStage next) {
    state.sort_kind = kind;
    state.after_sort = next;
    state.sort_width = 1;
    state.sort_start = 0;
    state.sort_left = 0;
    state.sort_right = std::min<std::size_t>(1, state.sort_count);
    state.sort_output = 0;
    state.sort_source_buffer = false;
    state.stage = CorpsStage::Sort;
}

bool step_corps_sort(CorpsDeployment& state) {
    if (state.sort_width >= state.sort_count) {
        if (state.sort_source_buffer) {
            std::copy_n(state.sort_buffer.begin(), state.sort_count, state.work_order.begin());
        }
        if (state.sort_kind == CorpsSort::Roster) {
            std::copy_n(state.work_order.begin(), state.sort_count, state.order.begin());
        }
        state.cursor = 0;
        state.stage = state.after_sort;
        return true;
    }
    std::array<std::uint16_t, domain::CORPS_OFFICER_CAPACITY>& source =
        state.sort_source_buffer ? state.sort_buffer : state.work_order;
    std::array<std::uint16_t, domain::CORPS_OFFICER_CAPACITY>& destination =
        state.sort_source_buffer ? state.work_order : state.sort_buffer;
    const std::size_t middle = std::min(state.sort_start + state.sort_width, state.sort_count);
    const std::size_t end = std::min(state.sort_start + state.sort_width * 2, state.sort_count);

    if (state.sort_left < middle && (state.sort_right == end ||
        !precedes(state.rows[source[state.sort_right]], state.rows[source[state.sort_left]], state.sort_kind))) {
        destination[state.sort_output++] = source[state.sort_left++];
    } else {
        destination[state.sort_output++] = source[state.sort_right++];
    }
    if (state.sort_output == end) {
        state.sort_start = end;
        if (end == state.sort_count) {
            state.sort_width *= 2;
            state.sort_start = 0;
            state.sort_source_buffer = !state.sort_source_buffer;
        }
        state.sort_left = state.sort_start;
        state.sort_right = std::min(state.sort_start + state.sort_width, state.sort_count);
        state.sort_output = state.sort_start;
    }
    return false;
}

void begin_corps_plan(CorpsDeployment& state) {
    std::size_t city_count = 0;
    bool same_baseline = state.baseline_valid && state.baseline_corps == state.corps;

    state.assigned_count = 0;
    for (std::size_t city = 0; city < domain::CITY_COUNT; ++city) {
        same_baseline = same_baseline && state.snapshot.cities[city].selected == state.baseline_cities[city];
        city_count += state.snapshot.cities[city].selected ? 1 : 0;
    }
    for (std::size_t city = 0; city < domain::CITY_COUNT; ++city) {
        domain::CorpsCity& row = state.snapshot.cities[city];

        row.assigned_count = 0;
        row.recommended_governor = domain::NO_CITY_OFFICER;
        row.target_count = row.selected ? (same_baseline ? state.baseline[city] : row.current_count) : 0;
    }
    if (!same_baseline && state.count >= city_count) {
        for (std::size_t city = 0; city < domain::CITY_COUNT; ++city) {
            domain::CorpsCity& row = state.snapshot.cities[city];
            std::uint8_t donor = domain::NO_CITY;

            if (!row.selected || row.target_count != 0) { continue; }
            for (std::size_t source = 0; source < domain::CITY_COUNT; ++source) {
                const domain::CorpsCity& candidate = state.snapshot.cities[source];

                if (candidate.selected && candidate.target_count > 1 &&
                    (donor == domain::NO_CITY || candidate.target_count > state.snapshot.cities[donor].target_count)) {
                    donor = static_cast<std::uint8_t>(source);
                }
            }
            if (donor != domain::NO_CITY) { --state.snapshot.cities[donor].target_count; ++row.target_count; }
        }
    }
    state.cursor = 0;
    state.group_head.fill(NO_CORPS_ROW);
    state.group_tail.fill(NO_CORPS_ROW);
    state.stage = CorpsStage::PlanInitialize;
    state.snapshot.status = domain::CorpsStatus::Planning;
}

bool step_corps_plan(CorpsDeployment& state) {
    if (state.stage == CorpsStage::PlanInitialize) {
        if (state.cursor == state.count) { state.cursor = 0; state.stage = CorpsStage::GroupBuild; return false; }
        const std::uint16_t index = state.order[state.cursor++];
        CorpsRosterEntry& row = state.rows[index];

        row.used = false;
        row.grouped = false;
        row.governor = false;
        row.goal = domain::NO_CITY;
        if (!movable(row)) {
            assign(state, index, row.city, domain::CorpsReason::Fixed);
            if (status(row) == 0xc8 || status(row) == 0xd8) {
                state.snapshot.cities[row.city].recommended_governor = row.id;
                row.governor = true;
            }
        }
    } else if (state.stage == CorpsStage::GroupBuild) {
        if (state.cursor == state.count) { state.cursor = 0; state.stage = CorpsStage::GroupSelect; return false; }
        const std::uint16_t index = state.order[state.cursor++];
        const std::uint16_t root = corps_group_root(state, index);

        state.group_next[index] = NO_CORPS_ROW;
        if (state.group_head[root] == NO_CORPS_ROW) { state.group_head[root] = index; }
        else { state.group_next[state.group_tail[root]] = index; }
        state.group_tail[root] = index;
    } else if (state.stage == CorpsStage::GroupSelect) {
        if (state.cursor == state.count) {
            state.plan_city = domain::NO_CITY;
            next_plan_city(state);
            state.stage = CorpsStage::EmptyGovernor;
            return false;
        }
        const std::uint16_t root = state.order[state.cursor++];

        if (state.parents[root] != root || state.group_head[root] == NO_CORPS_ROW) { return false; }
        state.group_root = root;
        state.member = state.group_head[root];
        state.group_count = 0;
        state.group_movable = 0;
        state.group_city = domain::NO_CITY;
        state.group_members.fill(0);
        state.group_low = false;
        state.group_front = false;
        state.group_conflict = false;
        state.group_anchor = false;
        state.stage = CorpsStage::GroupCollect;
    } else if (state.stage == CorpsStage::GroupCollect) {
        if (state.member != NO_CORPS_ROW) {
            const CorpsRosterEntry& row = state.rows[state.member];

            ++state.group_count;
            ++state.group_members[row.city];
            state.group_low = state.group_low || row.loyalty < 90;
            state.group_front = state.group_front || combat_score(row) >= rear_score(row) ||
                (status(row) == 0x18 && row.loyalty >= 90);
            if (row.used) {
                state.group_conflict = state.group_conflict ||
                    (state.group_city != domain::NO_CITY && state.group_city != row.city);
                state.group_city = row.city;
                state.group_anchor = true;
            } else if (movable(row)) { ++state.group_movable; }
            state.member = state.group_next[state.member];
            return false;
        }
        if (state.group_count < 2 || state.group_movable == 0 || state.group_conflict ||
            (state.group_anchor && state.group_low && state.snapshot.cities[state.group_city].frontline)) {
            state.stage = CorpsStage::GroupSelect;
            return false;
        }
        if (!state.group_anchor) {
            const bool front = !state.group_low && state.group_front;

            state.group_city = group_destination(state, front);
            if (state.group_city == domain::NO_CITY) { state.group_city = group_destination(state, !front); }
        }
        if (state.group_city == domain::NO_CITY) { state.stage = CorpsStage::GroupSelect; return false; }
        grow_group_target(state, state.group_city);
        state.member = state.group_head[state.group_root];
        state.stage = CorpsStage::GroupAssign;
    } else if (state.stage == CorpsStage::GroupAssign) {
        if (state.member == NO_CORPS_ROW) { state.stage = CorpsStage::GroupSelect; return false; }
        CorpsRosterEntry& row = state.rows[state.member];
        const domain::CorpsReason reason = state.group_anchor ? domain::CorpsReason::RelationshipAnchor :
            state.group_low ? domain::CorpsReason::RelationshipRear : domain::CorpsReason::Relationship;

        row.grouped = true;
        if (!row.used && movable(row)) { assign(state, state.member, state.group_city, reason); }
        state.member = state.group_next[state.member];
    } else if (state.stage == CorpsStage::EmptyGovernor) {
        if (state.plan_city >= domain::CITY_COUNT) { begin_bucket(state, 0); return false; }
        domain::CorpsCity& city = state.snapshot.cities[state.plan_city];

        if (!city.selected || city.current_count != 0 || city.current_governor != domain::NO_CITY_OFFICER ||
            city.target_count == 0 || city.assigned_count != 0 || city.recommended_governor != domain::NO_CITY_OFFICER) {
            next_plan_city(state);
            return false;
        }
        if (state.cursor < state.count) {
            const std::uint16_t index = state.order[state.cursor++];
            const CorpsRosterEntry& row = state.rows[index];
            const unsigned score = city.frontline ? front_score(row) : rear_score(row);
            const unsigned best = state.best_row == NO_CORPS_ROW ? 0 : city.frontline ?
                front_score(state.rows[state.best_row]) : rear_score(state.rows[state.best_row]);

            if (!row.used && row.status_pair == 0xd328 && row.loyalty == 100 &&
                (state.best_row == NO_CORPS_ROW || score > best || (score == best && row.id < state.rows[state.best_row].id))) {
                state.best_row = index;
            }
            return false;
        }
        if (state.best_row == NO_CORPS_ROW) { city.target_count = 0; }
        else {
            assign(state, state.best_row, state.plan_city, domain::CorpsReason::EmptyGovernor);
            city.recommended_governor = state.rows[state.best_row].id;
            state.rows[state.best_row].governor = true;
        }
        next_plan_city(state);
    } else if (state.stage == CorpsStage::Buckets) {
        if (state.cursor < state.count) {
            const std::uint16_t index = state.order[state.cursor++];
            const CorpsRosterEntry& row = state.rows[index];
            const bool matches = state.secondary == 0 ? row.loyalty < 90 : state.secondary == 1 ?
                status(row) == 0x18 : (status(row) == 0x28 || status(row) == 0xe8);

            if (!row.used && matches) { state.work_order[state.sort_count++] = index; }
            return false;
        }
        if (state.secondary == 0) {
            state.rear_count = state.sort_count;
            state.stage = CorpsStage::RearCapacity;
        } else if (state.secondary == 1) {
            begin_corps_sort(state, CorpsSort::Adviser, CorpsStage::AssignAdviser);
        } else if (state.secondary == 2) {
            state.front_take = 0;
            for (const domain::CorpsCity& city : state.snapshot.cities) {
                if (city.selected && city.frontline && city.target_count > city.assigned_count) {
                    state.front_take += city.target_count - city.assigned_count;
                }
            }
            state.front_take = std::min(state.front_take, state.sort_count);
            begin_corps_sort(state, CorpsSort::Front, CorpsStage::AssignFront);
        } else {
            begin_corps_sort(state, CorpsSort::FlexibleRear, CorpsStage::AssignFlexibleRear);
        }
    } else if (state.stage == CorpsStage::RearCapacity) {
        std::size_t remaining = 0;
        std::uint8_t donor = domain::NO_CITY;
        std::uint8_t receiver = domain::NO_CITY;

        for (std::size_t city = 0; city < domain::CITY_COUNT; ++city) {
            const domain::CorpsCity& row = state.snapshot.cities[city];

            if (!row.selected) { continue; }
            if (!row.frontline) {
                remaining += row.target_count > row.assigned_count ? row.target_count - row.assigned_count : 0;
                if (receiver == domain::NO_CITY || row.target_count < state.snapshot.cities[receiver].target_count) {
                    receiver = static_cast<std::uint8_t>(city);
                }
            }
        }
        for (unsigned pass = 0; pass < 2 && donor == domain::NO_CITY; ++pass) {
            for (std::size_t city = 0; city < domain::CITY_COUNT; ++city) {
                const domain::CorpsCity& row = state.snapshot.cities[city];

                if (!row.selected || !row.frontline || row.target_count <= row.assigned_count || (pass == 0 && row.target_count <= 1)) { continue; }
                if (donor == domain::NO_CITY || row.target_count > state.snapshot.cities[donor].target_count) {
                    donor = static_cast<std::uint8_t>(city);
                }
            }
        }
        if (remaining < state.rear_count && donor != domain::NO_CITY && receiver != domain::NO_CITY) {
            --state.snapshot.cities[donor].target_count;
            ++state.snapshot.cities[receiver].target_count;
        } else {
            for (std::size_t city = 0; city < domain::CITY_COUNT; ++city) {
                state.baseline[city] = state.snapshot.cities[city].target_count;
                state.baseline_cities[city] = state.snapshot.cities[city].selected;
            }
            state.baseline_corps = state.corps;
            state.baseline_valid = true;
            begin_corps_sort(state, CorpsSort::Rear, CorpsStage::AssignRear);
        }
    } else if (state.stage == CorpsStage::AssignRear || state.stage == CorpsStage::AssignAdviser ||
        state.stage == CorpsStage::AssignFront || state.stage == CorpsStage::AssignFlexibleRear) {
        const std::size_t limit = state.stage == CorpsStage::AssignFront ? state.front_take : state.sort_count;

        if (state.cursor == limit) {
            if (state.stage == CorpsStage::AssignRear) { begin_bucket(state, 1); }
            else if (state.stage == CorpsStage::AssignAdviser) { begin_bucket(state, 2); }
            else if (state.stage == CorpsStage::AssignFront) { begin_bucket(state, 3); }
            else {
                state.cursor = 0;
                state.eligible.fill(0);
                state.swap_out.fill(NO_CORPS_ROW);
                state.stage = CorpsStage::GovernorCounts;
            }
            return false;
        }
        const std::uint16_t index = state.work_order[state.cursor++];
        const bool front = state.stage == CorpsStage::AssignAdviser || state.stage == CorpsStage::AssignFront;
        std::uint8_t city = best_available_city(state, front);
        const domain::CorpsReason reason = state.stage == CorpsStage::AssignRear ? domain::CorpsReason::LowLoyaltyRear :
            state.stage == CorpsStage::AssignAdviser ? domain::CorpsReason::AdviserFront :
            front ? domain::CorpsReason::FrontScore : domain::CorpsReason::RearScore;

        if (city == domain::NO_CITY && state.stage != CorpsStage::AssignRear) { city = best_available_city(state, !front); }
        assign(state, index, city, reason);
    } else if (state.stage == CorpsStage::GovernorCounts) {
        if (state.cursor == state.count) {
            state.plan_city = domain::NO_CITY;
            next_plan_city(state);
            state.stage = CorpsStage::GovernorExchange;
            return false;
        }
        const std::uint16_t index = state.order[state.cursor++];
        const CorpsRosterEntry& row = state.rows[index];

        if (row.goal >= domain::CITY_COUNT) { return false; }
        if (eligible_governor(row)) { ++state.eligible[row.goal]; }
        if (!row.grouped && movable(row) && (state.swap_out[row.goal] == NO_CORPS_ROW ||
            row.assignment_order < state.rows[state.swap_out[row.goal]].assignment_order)) { state.swap_out[row.goal] = index; }
    } else if (state.stage == CorpsStage::GovernorExchange) {
        if (state.plan_city >= domain::CITY_COUNT) { state.cursor = 0; state.stage = CorpsStage::GovernorSelect; return false; }
        const domain::CorpsCity& target = state.snapshot.cities[state.plan_city];

        if (!target.selected || target.target_count == 0 || target.recommended_governor != domain::NO_CITY_OFFICER ||
            state.eligible[state.plan_city] != 0 || state.swap_out[state.plan_city] == NO_CORPS_ROW) {
            next_plan_city(state);
            return false;
        }
        if (state.cursor < state.count) {
            const std::uint16_t index = state.order[state.cursor++];
            const CorpsRosterEntry& row = state.rows[index];

            if (!eligible_governor(row) || row.grouped || row.governor || row.goal >= domain::CITY_COUNT || row.goal == state.plan_city) { return false; }
            const domain::CorpsCity& donor = state.snapshot.cities[row.goal];
            const bool needs_governor = donor.target_count > 0 && donor.recommended_governor == domain::NO_CITY_OFFICER;

            if (donor.frontline == target.frontline && (!needs_governor || state.eligible[row.goal] > 1) &&
                better_governor(state, index, state.best_row, target.frontline)) {
                state.best_row = index;
            }
            return false;
        }
        if (state.best_row != NO_CORPS_ROW) {
            CorpsRosterEntry& incoming = state.rows[state.best_row];
            CorpsRosterEntry& outgoing = state.rows[state.swap_out[state.plan_city]];
            const std::uint8_t donor = incoming.goal;

            incoming.goal = state.plan_city;
            outgoing.goal = donor;
            std::swap(incoming.assignment_order, outgoing.assignment_order);
            incoming.reason = domain::CorpsReason::GovernorExchange;
            outgoing.reason = domain::CorpsReason::GovernorExchange;
            --state.eligible[donor];
            ++state.eligible[state.plan_city];
        }
        next_plan_city(state);
    } else if (state.stage == CorpsStage::GovernorSelect) {
        if (state.cursor == state.count) { state.cursor = 0; state.stage = CorpsStage::PlanFinish; return false; }
        const std::uint16_t index = state.order[state.cursor++];
        const CorpsRosterEntry& row = state.rows[index];

        if (row.goal >= domain::CITY_COUNT || !eligible_governor(row)) { return false; }
        domain::CorpsCity& city = state.snapshot.cities[row.goal];
        const std::uint16_t previous = city.recommended_governor <= domain::CORPS_OFFICER_CAPACITY ?
            state.id_to_row[city.recommended_governor] : NO_CORPS_ROW;

        if (city.target_count == 0 || (previous != NO_CORPS_ROW && state.rows[previous].governor)) { return false; }
        if (better_governor(state, index, previous, city.frontline)) { city.recommended_governor = row.id; }
    } else if (state.stage == CorpsStage::PlanFinish) {
        if (state.cursor == state.count) {
            if (state.assigned_count != state.count) {
                state.failure = domain::CorpsStatus::InvalidRequest;
                state.stage = CorpsStage::Idle;
                return false;
            }
            state.snapshot.plan_valid = true;
            return true;
        }
        CorpsRosterEntry& row = state.rows[state.order[state.cursor++]];

        row.governor = row.goal < domain::CITY_COUNT && state.snapshot.cities[row.goal].recommended_governor == row.id;
    }
    return false;
}

} // namespace s8::game
