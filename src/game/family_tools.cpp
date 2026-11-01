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
    pointer &= OFFICER_POINTER_MASK;
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

std::uintptr_t relationship_candidate(std::uintptr_t game_base, std::size_t candidate) {
    constexpr std::array<std::uintptr_t, 2> POINTER_OFFSETS{0x57ae38, 0x462ba8};
    constexpr std::array<std::uintptr_t, 2> BIASES{0x80, 0xc0};
    std::uintptr_t pointer = 0;

    if (candidate >= 4 || !read_value(game_base + POINTER_OFFSETS[candidate / 2], pointer) ||
        pointer <= 0x10080 || pointer <= BIASES[candidate % 2]) {
        return 0;
    }
    return pointer - BIASES[candidate % 2];
}

std::uintptr_t relationship_candidate(const FamilyTools& state, std::size_t candidate) {
    return relationship_candidate(state.game_base, candidate);
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

void invalidate_spouse_order(FamilyTools& state) {
    if (++state.view.relationship_generation == 0) {
        ++state.view.relationship_generation;
    }
    state.changed = true;
}

bool spouse_recovery_identity(const SpouseOrderRecovery& recovery, domain::GameEpoch epoch) {
    const OfficerToolContext& expected = recovery.context;
    std::uintptr_t game_base = 0;
    std::uintptr_t player = 0;

    if (epoch != expected.epoch || !expected.game_base_pointer ||
        !read_value(expected.game_base_pointer, game_base) || game_base != expected.game_base ||
        !read_value(game_base + 0xe0, player) || player != expected.player ||
        officer_id(player) != recovery.hero || recovery.hero_slot == 0 ||
        resolve_officer_roster_base(expected.module) != recovery.roster ||
        relationship_candidate(game_base, recovery.candidate) != recovery.relationship_table ||
        officer_id(recovery.roster + (recovery.hero_slot - 1) * ROSTER_STRIDE) != recovery.hero) {
        return false;
    }
    for (std::size_t i = 0; i < recovery.before.size(); ++i) {
        if (pointer_slot(recovery.before[i], recovery.roster) == 0 ||
            officer_id(recovery.before[i]) != recovery.officer_ids[i]) {
            return false;
        }
    }
    return true;
}

std::uintptr_t spouse_field_address(const SpouseOrderRecovery& recovery, std::size_t index) {
    return recovery.relationship_table + (recovery.rows[index] - 1) * 0x40 +
        0x10 + (recovery.members[index] - 1) * sizeof(std::uintptr_t);
}

bool read_spouse_recovery(const SpouseOrderRecovery& recovery, domain::GameEpoch epoch,
    std::array<std::uintptr_t, 2>& values) {
    if (!spouse_recovery_identity(recovery, epoch)) {
        return false;
    }
    std::array<std::array<std::byte, 0x40>, 2> current{};

    for (std::size_t i = 0; i < current.size(); ++i) {
        if (i == 1 && recovery.rows[0] == recovery.rows[1]) {
            current[i] = current[0];
        } else if (!platform::windows::read_memory(
                recovery.relationship_table + (recovery.rows[i] - 1) * 0x40, current[i])) {
            return false;
        }
        for (std::size_t offset = 0; offset < current[i].size(); ++offset) {
            bool edited_field = false;

            for (std::size_t target = 0; target < recovery.rows.size(); ++target) {
                const std::size_t begin = 0x10 + (recovery.members[target] - 1) * sizeof(std::uintptr_t);

                edited_field = edited_field || (recovery.rows[i] == recovery.rows[target] &&
                    offset >= begin && offset < begin + sizeof(std::uintptr_t));
            }
            if (!edited_field && current[i][offset] != recovery.records[i][offset]) {
                return false;
            }
        }
        values[i] = field<std::uintptr_t>(current[i],
            0x10 + (recovery.members[i] - 1) * sizeof(std::uintptr_t));
        if (values[i] != recovery.before[i] && values[i] != recovery.before[1 - i]) {
            // A third-party value or a partial pointer write is not ours to
            // replace blindly. Keep the journal until it can be revalidated.
            return false;
        }
    }
    return spouse_recovery_identity(recovery, epoch);
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
    if (state.page_stage != FamilyPageStage::Idle) {
        state.pages_again = true;
        return;
    }
    state.page_view = state.view;
    state.page_view.child_total = 0;
    state.page_view.child_count = 0;
    state.page_view.spouse_count = 0;
    state.page_view.spouse_total = state.spouse_count;
    state.page_cursor = 0;
    state.page_child_index = 0;
    state.page_spouse_index = 0;
    state.page_year = state.current_year;
    state.page_completed_children = true;
    state.page_stage = FamilyPageStage::Count;
    state.view.loading = true;
    state.changed = true;
}

void step_family_pages(FamilyTools& state, const domain::WorkBudget& work_budget) {
    domain::FamilyToolsView& view = state.page_view;

    while (state.page_stage != FamilyPageStage::Idle && !work_budget.exhausted()) {
        if (state.page_stage == FamilyPageStage::Count) {
            if (state.page_cursor < state.child_count) {
                const std::uint16_t id = state.children[state.page_cursor++];

                if (view.child_spouse == 0 || state.child_other_parent[id] == view.child_spouse) {
                    ++view.child_total;
                }
                continue;
            }
            view.child_page = std::min(view.child_page,
                view.child_total == 0 ? 0 : (view.child_total - 1) / domain::FAMILY_CHILD_PAGE);
            view.spouse_page = std::min(view.spouse_page,
                view.spouse_total == 0 ? 0 : (view.spouse_total - 1) / domain::FAMILY_SPOUSE_PAGE);
            state.page_cursor = 0;
            state.page_stage = FamilyPageStage::Children;
        } else if (state.page_stage == FamilyPageStage::Children) {
            if (state.page_cursor == state.child_count || view.child_count == view.children.size()) {
                if (state.page_completed_children && view.child_count < view.children.size()) {
                    state.page_completed_children = false;
                    state.page_cursor = 0;
                } else {
                    state.page_cursor = 0;
                    state.page_stage = FamilyPageStage::Spouses;
                }
                continue;
            }
            const std::uint16_t id = state.children[state.page_cursor++];
            const FamilyOfficer& officer = state.officers[id];
            const ChildSchedule& schedule = state.schedules[id];
            const bool completed = state.page_year != 0 && state.page_year >= officer.appearance;

            // children is collected in ID order. Two passes preserve the old
            // completed-first ordering without an uninterruptible full sort.
            if (completed != state.page_completed_children ||
                (view.child_spouse != 0 && state.child_other_parent[id] != view.child_spouse)) {
                continue;
            }
            if (state.page_child_index++ < view.child_page * domain::FAMILY_CHILD_PAGE) {
                continue;
            }
            view.children[view.child_count++] = {id, state.child_other_parent[id], officer.appearance,
                officer.birth, officer.death, schedule.target_year, schedule.years_later, schedule.selected};
        } else {
            if (state.page_cursor < state.spouse_count && view.spouse_count < view.spouses.size()) {
                const std::size_t order = state.page_cursor++;
                const std::uint16_t id = state.spouse_order[order];

                if (!state.spouses.test(id) ||
                    state.page_spouse_index++ < view.spouse_page * domain::FAMILY_SPOUSE_PAGE) {
                    continue;
                }
                view.spouses[view.spouse_count++] = {id,
                    state.officers[id].birth, state.spouse_child_counts[id],
                    static_cast<std::uint16_t>(order + 1), !state.duplicate_spouses.test(id)};
                continue;
            }
            state.view.child_page = view.child_page;
            state.view.spouse_page = view.spouse_page;
            state.view.child_spouse = view.child_spouse;
            state.view.child_total = view.child_total;
            state.view.spouse_total = view.spouse_total;
            state.view.child_count = view.child_count;
            state.view.spouse_count = view.spouse_count;
            state.view.children = view.children;
            state.view.spouses = view.spouses;
            state.view.loading = state.stage != FamilyScanStage::Idle;
            state.page_stage = FamilyPageStage::Idle;
            state.changed = true;
            if (state.pages_again) {
                state.pages_again = false;
                rebuild_family_pages(state);
            }
        }
    }
}

void collect_child_rows(FamilyTools& state, const domain::WorkBudget& work_budget) {
    for (; state.cursor < state.officers.size() && !work_budget.exhausted(); ++state.cursor) {
        const std::size_t id = state.cursor;
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
    if (state.cursor == state.officers.size()) {
        state.view.hero = state.hero;
        state.ready = true;
        state.stage = FamilyScanStage::Idle;
        rebuild_family_pages(state);
    }
}

void finish_family_scan(FamilyTools& state, std::uint64_t now_ms) {
    state.stage = FamilyScanStage::Children;
    state.last_refresh_ms = now_ms;
    state.child_count = 0;
    state.child_other_parent.fill(0);
    state.spouse_child_counts.fill(0);
    state.cursor = 1;
}

void begin_family_scan(FamilyTools& state) {
    invalidate_spouse_order(state);
    state.officers.fill({});
    state.slot_ids.fill(0);
    state.spouses.reset();
    state.spouse_rows.fill(0);
    state.spouse_members.fill(0);
    state.spouse_count = 0;
    state.duplicate_spouses.reset();
    state.view.loading = true;
    state.view.relationships_ready = false;
    state.view.pregnancy_ready = false;
    state.ready = false;
    state.requested = false;
    state.cursor = 0;
    state.candidate = 0;
    state.hero_slot = 0;
    state.relationship_table = 0;
    state.page_stage = FamilyPageStage::Idle;
    state.pages_again = false;
    state.stage = FamilyScanStage::Roster;
    state.changed = true;
}

void scan_family_roster(FamilyTools& state, const domain::WorkBudget& work_budget) {
    const std::size_t end = std::min(state.cursor + FAMILY_READ_BUDGET, domain::OFFICER_TOOL_LIMIT);

    for (; state.cursor < end && !work_budget.exhausted(); ++state.cursor) {
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

void validate_family_relationships(FamilyTools& state, const domain::WorkBudget& work_budget) {
    const std::uintptr_t candidate = relationship_candidate(state, state.candidate);

    if (candidate == 0 || (state.relationship_table != 0 && state.relationship_table != candidate)) {
        next_relationship_candidate(state);
        return;
    }
    state.relationship_table = candidate;
    for (; state.cursor < 128 && !work_budget.exhausted(); ++state.cursor) {
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
    if (state.cursor < 128) {
        return;
    }
    if (state.valid_relationships == 0) {
        next_relationship_candidate(state);
        return;
    }
    state.cursor = 0;
    state.stage = FamilyScanStage::Relationships;
}

void scan_family_relationships(FamilyTools& state, const domain::WorkBudget& work_budget) {
    const std::size_t end = std::min(state.cursor + FAMILY_READ_BUDGET, RELATION_COUNT);

    if (relationship_candidate(state, state.candidate) != state.relationship_table) {
        begin_family_scan(state);
        return;
    }
    for (; state.cursor < end && !work_budget.exhausted(); ++state.cursor) {
        std::array<std::uintptr_t, 5> members{};
        std::array<std::uint16_t, 5> ids{};
        std::uint8_t kind = 0;
        bool has_hero = false;

        if (!read_relation(state.relationship_table, state.cursor, kind, members)) {
            state.spouses.reset();
            state.spouse_rows.fill(0);
            state.spouse_members.fill(0);
            state.spouse_count = 0;
            state.duplicate_spouses.reset();
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
        for (std::size_t member = 0; member < ids.size(); ++member) {
            const std::uint16_t id = ids[member];

            if (id != 0 && id != state.hero) {
                if (state.spouses.test(id)) {
                    state.duplicate_spouses.set(id);
                    continue;
                }
                state.spouses.set(id);
                state.spouse_rows[id] = static_cast<std::uint16_t>(state.cursor + 1);
                state.spouse_members[id] = static_cast<std::uint8_t>(member + 1);
                state.spouse_order[state.spouse_count++] = id;
            }
        }
    }
    if (state.cursor == RELATION_COUNT) {
        state.view.relationships_ready = true;
        state.stage = FamilyScanStage::Pregnancy;
        state.pregnancy_cursor = 0;
    }
}

void scan_pregnancy_table(FamilyTools& state, std::uint64_t now_ms, const domain::WorkBudget& work_budget) {
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
    const std::size_t first_offset = static_cast<std::size_t>((8 - (address & 7)) & 7);
    std::size_t offset = first_offset;

    if (platform::windows::read_memory(address, std::span(state.scan_buffer).first(length))) {
        for (; offset + sizeof(std::uintptr_t) <= length &&
            (offset == first_offset || !work_budget.exhausted()); offset += sizeof(std::uintptr_t)) {
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

void maintain_family(FamilyTools& state, const OfficerToolContext& context, const domain::WorkBudget& work_budget) {
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

    for (; state.maintenance_cursor < end && !work_budget.exhausted(); ++state.maintenance_cursor) {
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

void maintain_schedule_calendar(FamilyTools& state, const OfficerToolContext& context,
    const domain::WorkBudget& work_budget) {
    if (!state.scanning_schedules) {
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
        state.schedule_cursor = 0;
        state.scanning_schedules = true;
    }
    for (; state.schedule_cursor < state.schedules.size() && !work_budget.exhausted(); ++state.schedule_cursor) {
        ChildSchedule& schedule = state.schedules[state.schedule_cursor];

        if (schedule.selected && schedule.target_year != 0 && state.current_year >= schedule.target_year) {
            schedule = {};
            state.changed = true;
        }
    }
    if (state.schedule_cursor != state.schedules.size()) {
        return;
    }
    state.scanning_schedules = false;
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
    const std::uint64_t generation = state.view.relationship_generation;

    state.view = {};
    state.view.relationship_generation = generation;
    invalidate_spouse_order(state);
    // An epoch change invalidates the right to restore; it does not erase an
    // uncertain write. Retain its evidence and block another swap.
    state.view.spouse_order_recovery_pending = state.spouse_order_recovery.active;
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
    state.spouse_count = 0;
    state.requested = false;
    state.ready = false;
    state.maintain_children = false;
    state.last_refresh_ms = 0;
    state.last_maintenance_ms = 0;
    state.last_schedule_ms = 0;
    state.schedule_cursor = 0;
    state.scanning_schedules = false;
    state.page_stage = FamilyPageStage::Idle;
    state.pages_again = false;
    state.stage = FamilyScanStage::Idle;
    state.schedules.fill({});
    state.spouses.reset();
    state.duplicate_spouses.reset();
    state.changed = true;
}

void request_family_tools_refresh(FamilyTools& state) {
    state.requested = true;
}

bool tick_family_tools(FamilyTools& state, const OfficerToolContext& context,
    const domain::WorkBudget& work_budget) {
    if (work_budget.exhausted()) {
        return false;
    }
    if (state.spouse_order_recovery.active) {
        const OfficerToolContext& expected = state.spouse_order_recovery.context;

        if (context.epoch == expected.epoch && context.module == expected.module &&
            context.game_base == expected.game_base && context.player == expected.player &&
            context.game_base_pointer == expected.game_base_pointer) {
            restore_family_spouse_order(state);
        }
        if (state.spouse_order_recovery.active) {
            if (state.epoch != context.epoch) {
                reset_family_tools(state, context.epoch);
            }
            const bool changed = state.changed;

            state.changed = false;
            return changed;
        }
    }
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
        maintain_schedule_calendar(state, context, work_budget);
        if (!work_budget.exhausted()) {
            switch (state.stage) {
            case FamilyScanStage::Idle: maintain_family(state, context, work_budget); break;
            case FamilyScanStage::Roster: scan_family_roster(state, work_budget); break;
            case FamilyScanStage::ValidateRelationships: validate_family_relationships(state, work_budget); break;
            case FamilyScanStage::Relationships: scan_family_relationships(state, work_budget); break;
            case FamilyScanStage::Pregnancy: scan_pregnancy_table(state, context.now_ms, work_budget); break;
            case FamilyScanStage::Children: collect_child_rows(state, work_budget); break;
            }
        }
        step_family_pages(state, work_budget);
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
    state.page_stage = FamilyPageStage::Idle;
    state.pages_again = false;
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

    if (spouse == 0 || state.spouse_order_recovery.active ||
        !state.view.pregnancy_ready || !state.view.relationships_ready ||
        !state.spouses.test(request.new_spouse) || state.spouse_rows[request.new_spouse] == 0 ||
        relationship_candidate(state, state.candidate) != state.relationship_table ||
        !read_relation(state.relationship_table, state.spouse_rows[request.new_spouse] - 1, kind, members) ||
        kind != 2 || !read_pregnancy_table(state.pregnancy_table, slots)) {
        return domain::OfficerToolStatus::Unavailable;
    }
    for (const std::uintptr_t member : members) {
        const std::uint16_t id = pointer_slot(member, state.roster) == 0 ? 0 : officer_id(member);

        has_hero = has_hero || id == state.hero;
        has_spouse = has_spouse || (id == request.new_spouse && (member & OFFICER_POINTER_MASK) == spouse);
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

bool restore_family_spouse_order(FamilyTools& state) {
    SpouseOrderRecovery& recovery = state.spouse_order_recovery;

    if (!recovery.active) {
        return true;
    }
    state.view.spouse_order_recovery_pending = true;
    state.view.relationships_ready = false;
    std::array<std::uintptr_t, 2> values{};

    for (std::size_t remaining = recovery.before.size(); remaining != 0; --remaining) {
        const std::size_t index = remaining - 1;

        if (!read_spouse_recovery(recovery, state.epoch, values)) {
            return false;
        }
        if (values[index] != recovery.before[index]) {
            // Relation fields are data, not code. Avoid changing page
            // protections and retain the exact tagged pointer for rollback.
            const bool restored = platform::windows::write_memory(spouse_field_address(recovery, index),
                std::as_bytes(std::span(&recovery.before[index], 1)));

            if (!read_spouse_recovery(recovery, state.epoch, values) ||
                values[index] != recovery.before[index]) {
                return false;
            }
            // A failed API return can follow a completed write. Readback is
            // authoritative for restoration of this fixed-size data field.
            (void)restored;
        }
    }
    if (!read_spouse_recovery(recovery, state.epoch, values) || values != recovery.before) {
        return false;
    }
    recovery = {};
    state.view.spouse_order_recovery_pending = false;
    state.view.loading = true;
    state.requested = true;
    invalidate_spouse_order(state);
    return true;
}

domain::OfficerToolStatus swap_spouse_order(FamilyTools& state, const OfficerToolContext& context,
    const domain::SwapSpouseOrder& request) {
    if (request.first_spouse == 0 || request.second_spouse == 0 ||
        request.first_spouse > domain::OFFICER_TOOL_LIMIT || request.second_spouse > domain::OFFICER_TOOL_LIMIT ||
        request.first_spouse == request.second_spouse || request.first_order == 0 || request.second_order == 0 ||
        request.first_order == request.second_order || request.generation == 0) {
        return domain::OfficerToolStatus::InvalidRequest;
    }
    if (state.spouse_order_recovery.active) {
        return domain::OfficerToolStatus::RollbackFailed;
    }
    if (context.epoch != state.epoch) {
        return domain::OfficerToolStatus::SessionChanged;
    }
    if (!state.ready || state.requested || state.view.loading || !state.view.relationships_ready ||
        state.stage != FamilyScanStage::Idle || !same_identity(state, context)) {
        return domain::OfficerToolStatus::Unavailable;
    }
    if (request.generation != state.view.relationship_generation ||
        request.first_order > state.spouse_count || request.second_order > state.spouse_count ||
        state.spouse_order[request.first_order - 1] != request.first_spouse ||
        state.spouse_order[request.second_order - 1] != request.second_spouse ||
        state.duplicate_spouses.test(request.first_spouse) || state.duplicate_spouses.test(request.second_spouse)) {
        return domain::OfficerToolStatus::Conflict;
    }
    SpouseOrderRecovery recovery;

    recovery.context = context;
    recovery.roster = state.roster;
    recovery.relationship_table = state.relationship_table;
    recovery.candidate = state.candidate;
    recovery.hero = state.hero;
    recovery.hero_slot = state.hero_slot;
    recovery.officer_ids = {request.first_spouse, request.second_spouse};
    for (std::size_t i = 0; i < recovery.officer_ids.size(); ++i) {
        const std::uint16_t id = recovery.officer_ids[i];
        const std::uintptr_t officer = resolve_family_officer(state, context, id);

        recovery.rows[i] = state.spouse_rows[id];
        recovery.members[i] = state.spouse_members[id];
        if (!officer || recovery.rows[i] == 0 || recovery.rows[i] > RELATION_COUNT ||
            recovery.members[i] == 0 || recovery.members[i] > 5) {
            return domain::OfficerToolStatus::Conflict;
        }
        if (i == 1 && recovery.rows[0] == recovery.rows[1]) {
            recovery.records[i] = recovery.records[0];
        } else if (!platform::windows::read_memory(
                recovery.relationship_table + (recovery.rows[i] - 1) * 0x40, recovery.records[i])) {
            return domain::OfficerToolStatus::ReadFailed;
        }
        const auto& record = recovery.records[i];
        bool has_hero = false;

        for (std::size_t member = 0; member < 5; ++member) {
            const std::uintptr_t pointer = field<std::uintptr_t>(record, 0x10 + member * sizeof(std::uintptr_t));

            has_hero = has_hero || pointer_slot(pointer, state.roster) == state.hero_slot;
        }
        recovery.before[i] = field<std::uintptr_t>(record,
            0x10 + (recovery.members[i] - 1) * sizeof(std::uintptr_t));
        if (field<std::uint8_t>(record, 8) != 2 || !has_hero ||
            (recovery.before[i] & OFFICER_POINTER_MASK) != officer) {
            return domain::OfficerToolStatus::Conflict;
        }
    }
    if (spouse_field_address(recovery, 0) == spouse_field_address(recovery, 1)) {
        return domain::OfficerToolStatus::Conflict;
    }
    // A newly introduced duplicate in either selected record invalidates the
    // observed order, even if the two selected fields still hold their IDs.
    for (std::size_t row = 0; row < recovery.records.size(); ++row) {
        for (std::size_t member = 0; member < 5; ++member) {
            const std::uintptr_t pointer = field<std::uintptr_t>(recovery.records[row],
                0x10 + member * sizeof(std::uintptr_t)) & OFFICER_POINTER_MASK;

            for (std::size_t target = 0; target < recovery.before.size(); ++target) {
                if (pointer == (recovery.before[target] & OFFICER_POINTER_MASK) &&
                    (recovery.rows[row] != recovery.rows[target] || member + 1 != recovery.members[target])) {
                    return domain::OfficerToolStatus::Conflict;
                }
            }
        }
    }
    std::array<std::uintptr_t, 2> values{};

    if (!read_spouse_recovery(recovery, state.epoch, values) || values != recovery.before) {
        return domain::OfficerToolStatus::Conflict;
    }
    recovery.active = true;
    state.spouse_order_recovery = recovery;
    const bool first_written = platform::windows::write_memory(spouse_field_address(recovery, 0),
        std::as_bytes(std::span(&recovery.before[1], 1)));
    const bool first_verified = read_spouse_recovery(recovery, state.epoch, values) &&
        values[0] == recovery.before[1] && values[1] == recovery.before[1];
    bool second_written = false;

    if (first_written && first_verified) {
        second_written = platform::windows::write_memory(spouse_field_address(recovery, 1),
            std::as_bytes(std::span(&recovery.before[0], 1)));
    }
    if (!second_written || !read_spouse_recovery(recovery, state.epoch, values) ||
        values[0] != recovery.before[1] || values[1] != recovery.before[0]) {
        const bool restored = restore_family_spouse_order(state);

        state.changed = true;
        return restored ? domain::OfficerToolStatus::WriteFailed : domain::OfficerToolStatus::RollbackFailed;
    }
    state.spouse_order_recovery = {};
    std::swap(state.spouse_order[request.first_order - 1], state.spouse_order[request.second_order - 1]);
    std::swap(state.spouse_rows[request.first_spouse], state.spouse_rows[request.second_spouse]);
    std::swap(state.spouse_members[request.first_spouse], state.spouse_members[request.second_spouse]);
    invalidate_spouse_order(state);
    rebuild_family_pages(state);
    return domain::OfficerToolStatus::Completed;
}

} // namespace s8::game
