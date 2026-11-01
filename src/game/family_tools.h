#pragma once

#include "officer_tool_context.h"
#include "../domain/officer_tools.h"
#include "../domain/work_budget.h"

#include <array>
#include <bitset>
#include <cstddef>
#include <cstdint>

namespace s8::game {

struct FamilyOfficer {
    std::uint16_t slot = 0;
    std::uint16_t appearance = 0;
    std::uint16_t birth = 0;
    std::uint16_t death = 0;
    std::uint16_t father_slot = 0;
    std::uint16_t mother_slot = 0;
};

struct ChildSchedule {
    std::uint16_t original_appearance = 0;
    std::uint16_t original_birth = 0;
    std::uint16_t target_year = 0;
    std::uint8_t years_later = 3;
    bool selected = false;
};

enum class FamilyScanStage { Idle, Roster, ValidateRelationships, Relationships, Pregnancy, Children };
enum class FamilyPageStage { Idle, Count, Children, Spouses };

struct SpouseOrderRecovery {
    OfficerToolContext context;
    std::uintptr_t roster = 0;
    std::uintptr_t relationship_table = 0;
    std::size_t candidate = 0;
    std::uint16_t hero = 0;
    std::uint16_t hero_slot = 0;
    std::array<std::uint16_t, 2> rows{};
    std::array<std::uint8_t, 2> members{};
    std::array<std::uint16_t, 2> officer_ids{};
    std::array<std::uintptr_t, 2> before{};
    std::array<std::array<std::byte, 0x40>, 2> records{};
    bool active = false;
};

struct FamilyTools {
    domain::FamilyToolsView view;
    domain::GameEpoch epoch{};
    std::uintptr_t game_base = 0;
    std::uintptr_t roster = 0;
    std::uintptr_t relationship_table = 0;
    std::uintptr_t pregnancy_table = 0;
    std::uint16_t hero = 0;
    std::uint16_t hero_slot = 0;
    std::uint16_t current_year = 0;
    std::size_t cursor = 0;
    std::size_t candidate = 0;
    std::size_t valid_relationships = 0;
    std::size_t invalid_relationships = 0;
    std::size_t maintenance_cursor = 0;
    std::size_t child_count = 0;
    std::size_t spouse_count = 0;
    std::size_t schedule_cursor = 0;
    std::size_t page_cursor = 0;
    std::size_t page_child_index = 0;
    std::size_t page_spouse_index = 0;
    std::uint16_t page_year = 0;
    std::uint32_t pregnancy_offset = 0x5b40;
    std::uint32_t pregnancy_cursor = 0;
    std::uint64_t last_refresh_ms = 0;
    std::uint64_t last_maintenance_ms = 0;
    std::uint64_t last_schedule_ms = 0;
    bool requested = false;
    bool ready = false;
    bool changed = false;
    bool maintain_children = false;
    bool scanning_schedules = false;
    bool page_completed_children = true;
    bool pages_again = false;
    FamilyScanStage stage = FamilyScanStage::Idle;
    FamilyPageStage page_stage = FamilyPageStage::Idle;
    domain::FamilyToolsView page_view;
    // An uncertain two-field write survives refresh, epoch changes, and stop.
    SpouseOrderRecovery spouse_order_recovery;
    std::array<std::uint16_t, domain::OFFICER_TOOL_LIMIT> slot_ids{};
    std::array<FamilyOfficer, domain::OFFICER_TOOL_LIMIT + 1> officers{};
    std::array<ChildSchedule, domain::OFFICER_TOOL_LIMIT + 1> schedules{};
    std::array<std::uint16_t, domain::OFFICER_TOOL_LIMIT> children{};
    std::array<std::uint16_t, domain::OFFICER_TOOL_LIMIT + 1> child_other_parent{};
    std::array<std::uint16_t, domain::OFFICER_TOOL_LIMIT + 1> spouse_child_counts{};
    std::array<std::uint16_t, domain::OFFICER_TOOL_LIMIT + 1> spouse_rows{};
    std::array<std::uint8_t, domain::OFFICER_TOOL_LIMIT + 1> spouse_members{};
    std::array<std::uint16_t, domain::OFFICER_TOOL_LIMIT> spouse_order{};
    std::bitset<domain::OFFICER_TOOL_LIMIT + 1> spouses;
    std::bitset<domain::OFFICER_TOOL_LIMIT + 1> duplicate_spouses;
    std::array<std::byte, 4096> scan_buffer{};
};

void reset_family_tools(FamilyTools& state, domain::GameEpoch epoch);
void request_family_tools_refresh(FamilyTools& state);

// At most 128 roster/relation records or 16 pregnancy candidates per call.
// A completed snapshot contains IDs and values, never game addresses.
bool tick_family_tools(FamilyTools& state, const OfficerToolContext& context,
    const domain::WorkBudget& work_budget = {});

domain::OfficerToolStatus set_child_schedule(FamilyTools& state, const OfficerToolContext& context,
    const domain::SetChildSchedule& request);
domain::OfficerToolStatus set_pregnancy_spouse(FamilyTools& state, const OfficerToolContext& context,
    const domain::SetPregnancySpouse& request);
domain::OfficerToolStatus swap_spouse_order(FamilyTools& state, const OfficerToolContext& context,
    const domain::SwapSpouseOrder& request);
// Retries only against the journal's still-live identity. A failed attempt
// retains the journal; it never writes to a new epoch or another relation.
bool restore_family_spouse_order(FamilyTools& state);
void set_family_page(FamilyTools& state, const domain::SetFamilyPage& request);

// Used by demo stop and family edits. It always revalidates ID and roster root.
std::uintptr_t resolve_family_officer(const FamilyTools& state, const OfficerToolContext& context,
    std::uint16_t id);

} // namespace s8::game
