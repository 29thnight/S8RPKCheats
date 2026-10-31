#pragma once

#include "family_tools.h"
#include "officer_tool_context.h"
#include "../domain/officer_tools.h"
#include "../domain/ownership.h"
#include "../domain/work_budget.h"
#include "../domain/trait.h"

#include <array>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <random>

namespace s8::game {

enum class RandomTraitStage { Pool, Roster, Vtable, Seed, Search, Compact, Apply };

struct DemoRestoreIdentity {
    std::uintptr_t module = 0;
    std::uintptr_t game_base = 0;
    std::uintptr_t roster = 0;
    std::uintptr_t scenario = 0;
    std::uintptr_t live_officer = 0;
    std::uintptr_t vtable = 0;
    std::uintptr_t first_virtual = 0;
    std::uint16_t officer_id = 0;
};

struct OfficerTools {
    domain::OfficerToolsView view;
    FamilyTools family;
    DemoRestoreIdentity demo;
    domain::BatchTrainingRequest training;
    domain::RandomTraitsRequest random_request;
    own::shared_owner<const domain::CustomTraits> custom_traits;
    domain::CustomTraits::const_iterator custom_cursor;
    domain::GameEpoch job_epoch{};
    std::uintptr_t job_game_base = 0;
    std::uintptr_t job_roster = 0;
    std::uintptr_t trait_vtable = 0;
    std::uintptr_t trait_scan_address = 0;
    std::uint16_t job_hero = 0;
    std::uint16_t active_officer = 0;
    std::size_t cursor = 0;
    std::size_t field_cursor = 0;
    std::size_t pool_count = 0;
    std::size_t pool_cursor = 0;
    std::size_t compact_count = 0;
    std::size_t random_remaining = 0;
    std::uint32_t active_filled = 0;
    std::uint64_t last_date_ms = 0;
    bool running = false;
    bool active_changed = false;
    bool active_failed = false;
    bool changed_pending = false;
    bool family_first = false;
    RandomTraitStage random_stage = RandomTraitStage::Pool;
    std::mt19937 random;
    std::array<std::uint16_t, domain::OFFICER_TOOL_LIMIT + 1> roster_slots{};
    std::array<std::uint16_t, 65535> pool{};
    std::array<std::uintptr_t, 65536> trait_objects{};
    std::bitset<65536> requested_traits;
    std::array<std::byte, 16392> scan_buffer{};
};

void reset_officer_tools(OfficerTools& state, domain::GameEpoch epoch);
void stop_officer_tools(OfficerTools& state);

// The return value is request acceptance. Completion and partial write counts
// are published in view.progress; immediate edits publish request_status.
bool submit_officer_tool(OfficerTools& state, const OfficerToolContext& context,
    const domain::OfficerToolsRequest& request,
    own::shared_owner<const domain::CustomTraits> custom_traits = {});

// Call on the serialized game commit boundary. Uses fixed work budgets and
// returns true only when the value-only view changed. No worker is created.
bool tick_officer_tools(OfficerTools& state, const OfficerToolContext& context,
    const domain::WorkBudget& work_budget = {});

} // namespace s8::game
