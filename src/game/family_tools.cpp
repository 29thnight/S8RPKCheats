#include "family_tools.h"

#include "officer_roster.h"
#include "../platform/windows/process_memory.h"

#include <algorithm>
#include <cstring>
#include <span>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

namespace s8::game {
namespace {

// Original: ChildEarlyAppearance.cpp (parent links/year pairs),
// OfficerData_impl.inc (direct spouse records), PregnancyManager.cpp (3 x 0x28).
constexpr std::size_t ROSTER_STRIDE = 0x3d0;
constexpr std::size_t RELATION_COUNT = 3000;
constexpr std::size_t FAMILY_READ_BUDGET = 128;
constexpr std::uintptr_t OFFICER_POINTER_MASK = 0x0000ffffffffffffULL;
constexpr std::uint32_t PREGNANCY_SCAN_SIZE = 0x400000;

template <typename T>
bool read_value(std::uintptr_t address, T& value) {
    return platform::windows::read_memory(address, std::as_writable_bytes(std::span(&value, 1)));
}

template <typename T, std::size_t N>
T field(const std::array<std::byte, N>& bytes, std::size_t offset) {
    T value{};

    std::memcpy(&value, bytes.data() + offset, sizeof(value));
    return value;
}

std::uint16_t pointer_slot(std::uintptr_t pointer, std::uintptr_t roster) {
    if (pointer < roster || pointer - roster >= domain::OFFICER_TOOL_LIMIT * ROSTER_STRIDE ||
        (pointer - roster) % ROSTER_STRIDE != 0) {
        return 0;
    }
    return static_cast<std::uint16_t>((pointer - roster) / ROSTER_STRIDE + 1);
}

std::uint16_t officer_id(std::uintptr_t pointer) {
    const std::uintptr_t normalized = pointer & OFFICER_POINTER_MASK;
    std::uint16_t id = 0;

    return normalized > 0x10000 && read_value(normalized + 8, id) &&
        id > 0 && id <= domain::OFFICER_TOOL_LIMIT ? id : 0;
}

bool same_identity(const FamilyTools& state, const OfficerToolContext& context) {
    return context.epoch == state.epoch && context.game_base == state.game_base &&
        state.roster != 0 && resolve_officer_roster_base(context.module) == state.roster &&
        officer_id(context.player) == state.hero;
}

bool valid_life(const FamilyOfficer& officer) {
    return officer.birth != 0 && officer.appearance != 0 && officer.death != 0 &&
        officer.appearance >= officer.birth && officer.death >= officer.birth;
}

bool is_hero_child(const FamilyTools& state, const FamilyOfficer& officer) {
    return state.hero_slot != 0 && valid_life(officer) &&
        (officer.father_slot == state.hero_slot || officer.mother_slot == state.hero_slot);
}

bool read_family_officer(std::uintptr_t roster, std::size_t slot, std::uint16_t& id, FamilyOfficer& result) {
    std::array<std::byte, 0x58> record{};

    if (!platform::windows::read_memory(roster + slot * ROSTER_STRIDE, record)) {
        return false;
    }
    id = field<std::uint16_t>(record, 8);
    if (id == 0 || id > domain::OFFICER_TOOL_LIMIT) {
        return false;
    }
    result.slot = static_cast<std::uint16_t>(slot + 1);
    result.appearance = field<std::uint16_t>(record, 0x32);
    result.birth = field<std::uint16_t>(record, 0x34);
    result.death = field<std::uint16_t>(record, 0x36);
    result.father_slot = pointer_slot(field<std::uintptr_t>(record, 0x48) & OFFICER_POINTER_MASK, roster);
    result.mother_slot = pointer_slot(field<std::uintptr_t>(record, 0x50) & OFFICER_POINTER_MASK, roster);
    return true;
}

std::uintptr_t relationship_candidate(const FamilyTools& state, std::size_t candidate) {
    constexpr std::array<std::uintptr_t, 2> POINTER_OFFSETS{0x57ae38, 0x462ba8};
    constexpr std::array<std::uintptr_t, 2> BIASES{0x80, 0xc0};
    std::uintptr_t pointer = 0;

    if (candidate >= 4 || !read_value(state.game_base + POINTER_OFFSETS[candidate / 2], pointer) ||
        pointer <= 0x10080 || pointer <= BIASES[candidate % 2]) {
        return 0;
    }
    return pointer - BIASES[candidate % 2];
}

bool read_relation(std::uintptr_t table, std::size_t slot, std::uint8_t& kind,
    std::array<std::uintptr_t, 5>& members) {
    std::array<std::byte, 0x40> bytes{};

    if (!platform::windows::read_memory(table + slot * bytes.size(), bytes)) {
        return false;
    }
    kind = field<std::uint8_t>(bytes, 8);
    for (std::size_t i = 0; i < members.size(); ++i) {
        members[i] = field<std::uintptr_t>(bytes, 0x10 + i * sizeof(std::uintptr_t));
    }
    return true;
}

bool read_pregnancy_table(std::uintptr_t table, std::array<domain::PregnancyToolSlot, 3>& output) {
    std::array<std::byte, 0x78> bytes{};

    if (table <= 0x10000 || !platform::windows::read_memory(table, bytes)) {
        return false;
    }
    for (std::size_t i = 0; i < output.size(); ++i) {
        const std::size_t offset = i * 0x28;
        const std::uintptr_t spouse_pointer = field<std::uintptr_t>(bytes, offset);
        const std::uintptr_t child_pointer = field<std::uintptr_t>(bytes, offset + 0x10);
        domain::PregnancyToolSlot slot;

        slot.spouse = officer_id(spouse_pointer);
        slot.child = officer_id(child_pointer);
        slot.cooldown = field<std::uint8_t>(bytes, offset + 8);
        slot.pregnant = field<std::uint8_t>(bytes, offset + 9);
        slot.remaining_months = field<std::uint8_t>(bytes, offset + 10);
        if (slot.cooldown > 100 || slot.pregnant > 1 || slot.remaining_months > 12 ||
            ((spouse_pointer & OFFICER_POINTER_MASK) != 0 && slot.spouse == 0) ||
            (child_pointer != 0 && slot.child == 0)) {
            return false;
        }
        std::memcpy(slot.child_caps.data(), bytes.data() + offset + 0x1e, slot.child_caps.size());
        slot.safe_to_swap = slot.pregnant == 0 && slot.remaining_months == 0 && child_pointer == 0;
        output[i] = slot;
    }
    return true;
}

void rebuild_family_pages(FamilyTools& state) {
    domain::FamilyToolsView& view = state.view;
    std::size_t child_total = 0;

    std::sort(state.children.begin(), state.children.begin() + state.child_count,
        [&](std::uint16_t left, std::uint16_t right) {
            const bool left_done = state.current_year != 0 && state.current_year >= state.officers[left].appearance;
            const bool right_done = state.current_year != 0 && state.current_year >= state.officers[right].appearance;

            return left_done != right_done ? left_done : left < right;
        });
    view.spouse_total = state.spouses.count();
    for (std::size_t i = 0; i < state.child_count; ++i) {
        const std::uint16_t id = state.children[i];

        if (view.child_spouse == 0 || state.child_other_parent[id] == view.child_spouse) {
            ++child_total;
        }
    }
    view.child_total = child_total;
    view.child_page = std::min(view.child_page, child_total == 0 ? 0 : (child_total - 1) / domain::FAMILY_CHILD_PAGE);
    view.spouse_page = std::min(view.spouse_page,
        view.spouse_total == 0 ? 0 : (view.spouse_total - 1) / domain::FAMILY_SPOUSE_PAGE);
    view.child_count = 0;
    view.spouse_count = 0;

    std::size_t child_index = 0;
    std::size_t spouse_index = 0;

    for (std::size_t i = 0; i < state.child_count; ++i) {
        const std::uint16_t id = state.children[i];
        const FamilyOfficer& officer = state.officers[id];
        const ChildSchedule& schedule = state.schedules[id];

        if (view.child_spouse != 0 && state.child_other_parent[id] != view.child_spouse) {
            continue;
        }
        if (child_index++ < view.child_page * domain::FAMILY_CHILD_PAGE ||
            view.child_count >= view.children.size()) {
            continue;
        }
        view.children[view.child_count++] = {id, state.child_other_parent[id], officer.appearance,
            officer.birth, officer.death, schedule.target_year, schedule.years_later, schedule.selected};
    }
    for (std::size_t id = 1; id < state.officers.size(); ++id) {
        if (!state.spouses.test(id)) {
            continue;
        }
        if (spouse_index++ < view.spouse_page * domain::FAMILY_SPOUSE_PAGE ||
            view.spouse_count >= view.spouses.size()) {
            continue;
        }
        view.spouses[view.spouse_count++] = {static_cast<std::uint16_t>(id),
            state.officers[id].birth, state.spouse_child_counts[id]};
    }
    state.changed = true;
}

void collect_child_rows(FamilyTools& state) {
    state.child_count = 0;
    state.child_other_parent.fill(0);
    state.spouse_child_counts.fill(0);
    for (std::size_t id = 1; id < state.officers.size(); ++id) {
        const FamilyOfficer& officer = state.officers[id];

        if (officer.slot == 0 || !is_hero_child(state, officer)) {
            continue;
        }
        const std::uint16_t other_slot = officer.father_slot == state.hero_slot
            ? officer.mother_slot : officer.father_slot;
        const std::uint16_t other_id = other_slot == 0 ? 0 : state.slot_ids[other_slot - 1];

        state.children[state.child_count++] = static_cast<std::uint16_t>(id);
        state.child_other_parent[id] = other_id;
        if (other_id != 0 && state.spouses.test(other_id)) {
            ++state.spouse_child_counts[other_id];
        }
    }
    rebuild_family_pages(state);
}

void finish_family_scan(FamilyTools& state, std::uint64_t now_ms) {
    state.view.loading = false;
    state.view.hero = state.hero;
    state.ready = true;
    state.stage = FamilyScanStage::Idle;
    state.last_refresh_ms = now_ms;
    collect_child_rows(state);
}

void begin_family_scan(FamilyTools& state) {
    state.officers.fill({});
    state.slot_ids.fill(0);
    state.spouses.reset();
    state.spouse_rows.fill(0);
    state.view.loading = true;
    state.view.relationships_ready = false;
    state.view.pregnancy_ready = false;
    state.ready = false;
    state.requested = false;
    state.cursor = 0;
    state.candidate = 0;
    state.hero_slot = 0;
    state.relationship_table = 0;
    state.stage = FamilyScanStage::Roster;
    state.changed = true;
}

void scan_family_roster(FamilyTools& state) {
    const std::size_t end = std::min(state.cursor + FAMILY_READ_BUDGET, domain::OFFICER_TOOL_LIMIT);

    for (; state.cursor < end; ++state.cursor) {
        FamilyOfficer officer;
        std::uint16_t id = 0;

        if (read_family_officer(state.roster, state.cursor, id, officer) && state.officers[id].slot == 0) {
            state.officers[id] = officer;
            state.slot_ids[state.cursor] = id;
            if (id == state.hero) {
                state.hero_slot = officer.slot;
            }
        }
    }
    if (state.cursor == domain::OFFICER_TOOL_LIMIT) {
        state.cursor = 0;
        state.valid_relationships = 0;
        state.invalid_relationships = 0;
        state.stage = FamilyScanStage::ValidateRelationships;
    }
}

void next_relationship_candidate(FamilyTools& state) {
    ++state.candidate;
    state.cursor = 0;
    state.valid_relationships = 0;
    state.invalid_relationships = 0;
    state.relationship_table = 0;
    if (state.candidate == 4) {
        state.stage = FamilyScanStage::Pregnancy;
        state.pregnancy_cursor = 0;
    }
}

void validate_family_relationships(FamilyTools& state) {
    const std::uintptr_t candidate = relationship_candidate(state, state.candidate);

    if (candidate == 0 || (state.relationship_table != 0 && state.relationship_table != candidate)) {
        next_relationship_candidate(state);
        return;
    }
    state.relationship_table = candidate;
    for (; state.cursor < 128; ++state.cursor) {
        std::array<std::uintptr_t, 5> members{};
        std::uint8_t kind = 0;

        if (!read_relation(candidate, state.cursor, kind, members)) {
            next_relationship_candidate(state);
            return;
        }
        if (kind == 0) {
            continue;
        }
        if (kind > 4 || pointer_slot(members[0], state.roster) == 0 ||
            pointer_slot(members[1], state.roster) == 0) {
            if (++state.invalid_relationships > 3) {
                next_relationship_candidate(state);
                return;
            }
        } else {
            ++state.valid_relationships;
        }
    }
    if (state.valid_relationships == 0) {
        next_relationship_candidate(state);
        return;
    }
    state.cursor = 0;
    state.stage = FamilyScanStage::Relationships;
}

void scan_family_relationships(FamilyTools& state) {
    const std::size_t end = std::min(state.cursor + FAMILY_READ_BUDGET, RELATION_COUNT);

    if (relationship_candidate(state, state.candidate) != state.relationship_table) {
        begin_family_scan(state);
        return;
    }
    for (; state.cursor < end; ++state.cursor) {
        std::array<std::uintptr_t, 5> members{};
        std::array<std::uint16_t, 5> ids{};
        std::uint8_t kind = 0;
        bool has_hero = false;

        if (!read_relation(state.relationship_table, state.cursor, kind, members)) {
            state.spouses.reset();
            state.spouse_rows.fill(0);
            state.stage = FamilyScanStage::Pregnancy;
            state.pregnancy_cursor = 0;
            return;
        }
        if (kind != 2) {
            continue;
        }
        for (std::size_t i = 0; i < members.size(); ++i) {
            const std::uint16_t slot = pointer_slot(members[i], state.roster);

            ids[i] = slot == 0 ? 0 : state.slot_ids[slot - 1];
            has_hero = has_hero || ids[i] == state.hero;
        }
        if (!has_hero) {
            continue;
        }
        for (const std::uint16_t id : ids) {
            if (id != 0 && id != state.hero) {
                state.spouses.set(id);
                state.spouse_rows[id] = static_cast<std::uint16_t>(state.cursor + 1);
            }
        }
    }
    if (state.cursor == RELATION_COUNT) {
        state.view.relationships_ready = true;
        state.stage = FamilyScanStage::Pregnancy;
        state.pregnancy_cursor = 0;
    }
}

void scan_pregnancy_table(FamilyTools& state, std::uint64_t now_ms) {
    if (state.pregnancy_cursor == 0) {
        std::array<domain::PregnancyToolSlot, 3> slots{};
        bool found = read_pregnancy_table(state.game_base + state.pregnancy_offset, slots);

        if (!found && state.pregnancy_offset != 0x5b40) {
            found = read_pregnancy_table(state.game_base + 0x5b40, slots);
            state.pregnancy_offset = 0x5b40;
        }
        if (found) {
            state.pregnancy_table = state.game_base + state.pregnancy_offset;
            state.view.pregnancy = slots;
            state.view.pregnancy_ready = true;
            finish_family_scan(state, now_ms);
            return;
        }
        if (state.spouses.none()) {
            finish_family_scan(state, now_ms);
            return;
        }
    }
    const std::uintptr_t address = state.game_base + state.pregnancy_cursor;
    MEMORY_BASIC_INFORMATION region{};

    if (VirtualQuery(reinterpret_cast<const void*>(address), &region, sizeof(region)) != sizeof(region)) {
        finish_family_scan(state, now_ms);
        return;
    }
    const std::uintptr_t region_start = reinterpret_cast<std::uintptr_t>(region.BaseAddress);
    const std::uintptr_t region_end = region_start + region.RegionSize;

    if (region_end <= region_start || address < region_start || address >= region_end) {
        finish_family_scan(state, now_ms);
        return;
    }
    const std::size_t region_remaining = std::min(static_cast<std::size_t>(region_end - address),
        static_cast<std::size_t>(PREGNANCY_SCAN_SIZE - state.pregnancy_cursor));

    if (region.State != MEM_COMMIT || (region.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0) {
        state.pregnancy_cursor += static_cast<std::uint32_t>(region_remaining);
        if (state.pregnancy_cursor >= PREGNANCY_SCAN_SIZE) {
            finish_family_scan(state, now_ms);
        }
        return;
    }
    // A 4 KiB block starting at an unaligned game root can straddle an
    // unreadable region. Read only this committed region so its valid suffix
    // is still searched rather than discarding the whole block on failure.
    const std::size_t length = std::min(state.scan_buffer.size(), region_remaining);
    std::size_t candidates = 0;
    std::size_t offset = static_cast<std::size_t>((8 - (address & 7)) & 7);

    if (platform::windows::read_memory(address, std::span(state.scan_buffer).first(length))) {
        for (; offset + sizeof(std::uintptr_t) <= length; offset += sizeof(std::uintptr_t)) {
            const std::uintptr_t pointer = field<std::uintptr_t>(state.scan_buffer, offset) & OFFICER_POINTER_MASK;
            const std::uint16_t slot = pointer_slot(pointer, state.roster);
            const std::uint16_t id = slot == 0 ? 0 : state.slot_ids[slot - 1];

            if (id == 0 || !state.spouses.test(id)) {
                continue;
            }
            for (std::size_t pregnancy_slot = 0; pregnancy_slot < 3; ++pregnancy_slot) {
                const std::uintptr_t back = pregnancy_slot * 0x28;
                const std::uintptr_t hit = address + offset;
                std::array<domain::PregnancyToolSlot, 3> slots{};

                if (hit < state.game_base + back || hit - back + 0x78 > state.game_base + PREGNANCY_SCAN_SIZE) {
                    continue;
                }
                ++candidates;
                if (read_pregnancy_table(hit - back, slots)) {
                    state.pregnancy_table = hit - back;
                    state.pregnancy_offset = static_cast<std::uint32_t>(state.pregnancy_table - state.game_base);
                    state.view.pregnancy = slots;
                    state.view.pregnancy_ready = true;
                    finish_family_scan(state, now_ms);
                    return;
                }
            }
            if (candidates >= 15) {
                offset += sizeof(std::uintptr_t);
                break;
            }
        }
    } else {
        offset = length;
    }
    if (offset + sizeof(std::uintptr_t) > length) {
        offset = length;
    }
    state.pregnancy_cursor += static_cast<std::uint32_t>(std::min(offset, length));
    if (state.pregnancy_cursor >= PREGNANCY_SCAN_SIZE) {
        finish_family_scan(state, now_ms);
    }
}

void maintain_family(FamilyTools& state, const OfficerToolContext& context) {
    if (!state.maintain_children && context.now_ms - state.last_maintenance_ms >= 1000) {
        state.last_maintenance_ms = context.now_ms;
        state.maintenance_cursor = 0;
        state.maintain_children = true;
        if (state.view.pregnancy_ready) {
            state.view.pregnancy_ready = read_pregnancy_table(state.pregnancy_table, state.view.pregnancy);
            state.changed = true;
        }
    }
    if (!state.maintain_children) {
        return;
    }
    const std::size_t end = std::min(state.maintenance_cursor + FAMILY_READ_BUDGET, state.child_count);
    std::uint16_t year = 0;
    std::uint8_t month = 0;
    const bool valid_year = read_scenario_date(context.module, year, month) && year >= 171 && year < 270;

    for (; state.maintenance_cursor < end; ++state.maintenance_cursor) {
        const std::uint16_t id = state.children[state.maintenance_cursor];
        FamilyOfficer refreshed;
        std::uint16_t actual_id = 0;

        if (state.officers[id].slot != 0 && read_family_officer(state.roster, state.officers[id].slot - 1,
            actual_id, refreshed) && actual_id == id && is_hero_child(state, refreshed)) {
            state.officers[id] = refreshed;
        }
        ChildSchedule& schedule = state.schedules[id];

        if (valid_year && schedule.selected && schedule.target_year != 0 && year >= schedule.target_year) {
            // Completed early enlistment becomes the child's actual history.
            schedule = {};
        }
    }
    if (state.maintenance_cursor == state.child_count) {
        state.maintain_children = false;
        rebuild_family_pages(state);
    }
}

void maintain_schedule_calendar(FamilyTools& state, const OfficerToolContext& context) {
    if (state.last_schedule_ms != 0 && context.now_ms - state.last_schedule_ms < 1000) {
        return;
    }
    std::uint16_t year = 0;
    std::uint8_t month = 0;

    state.last_schedule_ms = context.now_ms;
    if (!read_scenario_date(context.module, year, month) || year < 171 || year >= 270) {
        state.current_year = 0;
        return;
    }
    state.current_year = year;
    for (ChildSchedule& schedule : state.schedules) {
        if (schedule.selected && schedule.target_year != 0 && year >= schedule.target_year) {
            schedule = {};
            state.changed = true;
        }
    }
    // Calendar completion does not wait for the optional pregnancy search.
    for (std::size_t i = 0; i < state.view.child_count; ++i) {
        domain::ChildToolRow& row = state.view.children[i];
        const ChildSchedule& schedule = state.schedules[row.id];

        row.selected = schedule.selected;
        row.target_year = schedule.target_year;
    }
}

} // namespace

void reset_family_tools(FamilyTools& state, domain::GameEpoch epoch) {
    state.view = {};
    state.epoch = epoch;
    state.game_base = 0;
    state.roster = 0;
    state.hero = 0;
    state.hero_slot = 0;
    state.current_year = 0;
    state.relationship_table = 0;
    state.pregnancy_table = 0;
    state.pregnancy_offset = 0x5b40;
    state.pregnancy_cursor = 0;
    state.cursor = 0;
    state.child_count = 0;
    state.requested = false;
    state.ready = false;
    state.maintain_children = false;
    state.last_refresh_ms = 0;
    state.last_maintenance_ms = 0;
    state.last_schedule_ms = 0;
    state.stage = FamilyScanStage::Idle;
    state.schedules.fill({});
    state.spouses.reset();
    state.changed = true;
}

void request_family_tools_refresh(FamilyTools& state) {
    state.requested = true;
}

bool tick_family_tools(FamilyTools& state, const OfficerToolContext& context) {
    const std::uint16_t hero = officer_id(context.player);
    const std::uintptr_t roster = resolve_officer_roster_base(context.module);

    if (context.game_base <= 0x10000 || roster == 0 || hero == 0) {
        if (state.hero != 0 || state.epoch != context.epoch) {
            reset_family_tools(state, context.epoch);
        }
    } else {
        if (state.epoch != context.epoch || state.hero != hero ||
            state.game_base != context.game_base || state.roster != roster) {
            reset_family_tools(state, context.epoch);
            state.game_base = context.game_base;
            state.roster = roster;
            state.hero = hero;
            begin_family_scan(state);
        }
        if (state.stage == FamilyScanStage::Idle && (state.requested || !state.ready ||
            context.now_ms - state.last_refresh_ms >= 30000)) {
            begin_family_scan(state);
        }
        maintain_schedule_calendar(state, context);
        switch (state.stage) {
        case FamilyScanStage::Idle: maintain_family(state, context); break;
        case FamilyScanStage::Roster: scan_family_roster(state); break;
        case FamilyScanStage::ValidateRelationships: validate_family_relationships(state); break;
        case FamilyScanStage::Relationships: scan_family_relationships(state); break;
        case FamilyScanStage::Pregnancy: scan_pregnancy_table(state, context.now_ms); break;
        }
    }
    const bool changed = state.changed;

    state.changed = false;
    return changed;
}

std::uintptr_t resolve_family_officer(const FamilyTools& state, const OfficerToolContext& context,
    std::uint16_t id) {
    if (id == 0 || id > domain::OFFICER_TOOL_LIMIT || !state.ready || !same_identity(state, context) ||
        state.officers[id].slot == 0) {
        return 0;
    }
    const std::uintptr_t officer = state.roster + (state.officers[id].slot - 1) * ROSTER_STRIDE;

    return officer_id(officer) == id ? officer : 0;
}

void set_family_page(FamilyTools& state, const domain::SetFamilyPage& request) {
    state.view.child_page = request.child_page;
    state.view.spouse_page = request.spouse_page;
    state.view.child_spouse = request.child_spouse;
    if (state.ready) {
        rebuild_family_pages(state);
    }
}

domain::OfficerToolStatus set_child_schedule(FamilyTools& state, const OfficerToolContext& context,
    const domain::SetChildSchedule& request) {
    const std::uintptr_t officer = resolve_family_officer(state, context, request.officer_id);
    FamilyOfficer current;
    std::uint16_t actual_id = 0;
    std::uint16_t year = 0;
    std::uint8_t month = 0;

    if (request.years_later < 1 || request.years_later > 10 || request.officer_id == 0 ||
        request.officer_id > domain::OFFICER_TOOL_LIMIT) {
        return domain::OfficerToolStatus::InvalidRequest;
    }
    if (officer == 0 || !read_family_officer(state.roster, state.officers[request.officer_id].slot - 1,
        actual_id, current) || actual_id != request.officer_id || !is_hero_child(state, current) ||
        !read_scenario_date(context.module, year, month) || year < 171 || year >= 270) {
        return domain::OfficerToolStatus::Unavailable;
    }
    ChildSchedule& schedule = state.schedules[request.officer_id];

    state.officers[request.officer_id] = current;
    if (schedule.selected && schedule.target_year != 0 && year >= schedule.target_year) {
        schedule = {};
        rebuild_family_pages(state);
        return request.enabled ? domain::OfficerToolStatus::Conflict : domain::OfficerToolStatus::Completed;
    }
    if (!request.enabled && !schedule.selected) {
        schedule.years_later = request.years_later;
        rebuild_family_pages(state);
        return domain::OfficerToolStatus::Completed;
    }
    if ((request.enabled && year >= current.appearance) ||
        (schedule.selected && (current.appearance != schedule.target_year ||
            current.birth != schedule.target_year - 15))) {
        return domain::OfficerToolStatus::Conflict;
    }
    const std::uint16_t target_year = static_cast<std::uint16_t>(year + request.years_later);

    if (request.enabled && target_year >= 270) {
        return domain::OfficerToolStatus::InvalidRequest;
    }
    const std::array<std::uint16_t, 2> before{current.appearance, current.birth};
    const std::array<std::uint16_t, 2> after = request.enabled
        ? std::array<std::uint16_t, 2>{target_year, static_cast<std::uint16_t>(target_year - 15)}
        : std::array<std::uint16_t, 2>{schedule.original_appearance, schedule.original_birth};
    std::array<std::uint16_t, 2> verify{};
    const bool wrote = platform::windows::write_data_memory(officer + 0x32, std::as_bytes(std::span(after)));

    if (!wrote || !read_value(officer + 0x32, verify) || verify != after) {
        const bool restored = same_identity(state, context) && officer_id(officer) == request.officer_id &&
            platform::windows::write_data_memory(officer + 0x32, std::as_bytes(std::span(before))) &&
            read_value(officer + 0x32, verify) && verify == before;

        if (!restored && !schedule.selected) {
            schedule = {before[0], before[1], target_year, request.years_later, true};
            rebuild_family_pages(state);
        }
        return restored ? domain::OfficerToolStatus::WriteFailed : domain::OfficerToolStatus::RollbackFailed;
    }
    if (request.enabled) {
        if (!schedule.selected) {
            schedule.original_appearance = before[0];
            schedule.original_birth = before[1];
        }
        schedule.target_year = target_year;
        schedule.years_later = request.years_later;
        schedule.selected = true;
    } else {
        schedule = {};
        schedule.years_later = request.years_later;
    }
    state.officers[request.officer_id].appearance = after[0];
    state.officers[request.officer_id].birth = after[1];
    rebuild_family_pages(state);
    return domain::OfficerToolStatus::Completed;
}

domain::OfficerToolStatus set_pregnancy_spouse(FamilyTools& state, const OfficerToolContext& context,
    const domain::SetPregnancySpouse& request) {
    if (request.slot >= 3 || request.new_spouse == 0 || request.new_spouse > domain::OFFICER_TOOL_LIMIT) {
        return domain::OfficerToolStatus::InvalidRequest;
    }
    const std::uintptr_t spouse = resolve_family_officer(state, context, request.new_spouse);
    std::array<domain::PregnancyToolSlot, 3> slots{};
    std::array<std::uintptr_t, 5> members{};
    std::uint8_t kind = 0;
    bool has_hero = false;
    bool has_spouse = false;

    if (spouse == 0 || !state.view.pregnancy_ready || !state.view.relationships_ready ||
        !state.spouses.test(request.new_spouse) || state.spouse_rows[request.new_spouse] == 0 ||
        relationship_candidate(state, state.candidate) != state.relationship_table ||
        !read_relation(state.relationship_table, state.spouse_rows[request.new_spouse] - 1, kind, members) ||
        kind != 2 || !read_pregnancy_table(state.pregnancy_table, slots)) {
        return domain::OfficerToolStatus::Unavailable;
    }
    for (const std::uintptr_t member : members) {
        const std::uint16_t id = pointer_slot(member, state.roster) == 0 ? 0 : officer_id(member);

        has_hero = has_hero || id == state.hero;
        has_spouse = has_spouse || (id == request.new_spouse && member == spouse);
    }
    if (!has_hero || !has_spouse || slots[request.slot].spouse != request.expected_spouse ||
        !slots[request.slot].safe_to_swap) {
        return domain::OfficerToolStatus::Conflict;
    }
    for (const domain::PregnancyToolSlot& slot : slots) {
        if (slot.spouse == request.new_spouse) {
            return domain::OfficerToolStatus::Conflict;
        }
    }
    const std::uintptr_t address = state.pregnancy_table + request.slot * 0x28;
    std::array<std::byte, 0x28> before{};

    if (!platform::windows::read_memory(address, before)) {
        return domain::OfficerToolStatus::ReadFailed;
    }
    std::array<std::byte, 0x28> after = before;
    std::array<std::byte, 0x28> verify{};
    constexpr std::uintptr_t EMPTY_CHILD = 0;

    std::memcpy(after.data(), &spouse, sizeof(spouse));
    after[8] = std::byte{100};
    after[9] = std::byte{0};
    after[10] = std::byte{0};
    std::memcpy(after.data() + 0x10, &EMPTY_CHILD, sizeof(EMPTY_CHILD));
    std::fill_n(after.begin() + 0x1e, 5, std::byte{0});
    const bool wrote = platform::windows::write_data_memory(address, after);

    if (!wrote || !platform::windows::read_memory(address, verify) || verify != after) {
        const bool restored = same_identity(state, context) &&
            platform::windows::write_data_memory(address, before) &&
            platform::windows::read_memory(address, verify) && verify == before;

        return restored ? domain::OfficerToolStatus::WriteFailed : domain::OfficerToolStatus::RollbackFailed;
    }
    state.view.pregnancy_ready = read_pregnancy_table(state.pregnancy_table, state.view.pregnancy);
    state.requested = true;
    state.changed = true;
    return domain::OfficerToolStatus::Completed;
}

} // namespace s8::game
