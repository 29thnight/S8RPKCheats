#pragma once

#include "../domain/corps_deployment.h"
#include "../domain/work_budget.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace s8::game {

inline constexpr std::uint16_t NO_CORPS_ROW = 0xffff;
inline constexpr std::size_t CORPS_JOURNAL_CAPACITY = domain::CORPS_OFFICER_CAPACITY * 2 + domain::CITY_COUNT + 1;

enum class CorpsStage : std::uint8_t {
    Idle, Cities, Frontiers, Roster, Sort, RelationCandidate, RelationValidate, RelationScan,
    SynergyCandidate, SynergyValidate, SynergyScan, RelationMerge, PlanInitialize, GroupBuild,
    GroupSelect, GroupCollect, GroupAssign, EmptyGovernor, Buckets, RearCapacity,
    AssignRear, AssignAdviser, AssignFront, AssignFlexibleRear, GovernorCounts,
    GovernorExchange, GovernorSelect, PlanFinish, ValidateMoves, ApplyMoves, VerifyMoves,
    RefreshLive, DeferCities, PropagateDeferred, ValidateGovernors, GovernorMoveSweep,
    GovernorSwapSweep, CycleBuffer, FirstGovernorCheck, VerifyGroups, VerifyGovernors, Rollback
};

enum class CorpsSort : std::uint8_t { Roster, Rear, Adviser, Front, FlexibleRear };

struct CorpsRosterEntry {
    std::uintptr_t address = 0;
    std::uintptr_t force = 0;
    std::uintptr_t city_pointer = 0;
    std::uint16_t id = domain::NO_CITY_OFFICER;
    std::uint16_t status_pair = 0;
    std::uint16_t assignment_order = NO_CORPS_ROW;
    std::array<std::uint8_t, 5> attributes{};
    std::uint8_t loyalty = 0;
    std::uint8_t city = domain::NO_CITY;
    std::uint8_t goal = domain::NO_CITY;
    domain::CorpsReason reason = domain::CorpsReason::Retained;
    bool selected = false;
    bool used = false;
    bool grouped = false;
    bool governor = false;
};

struct CorpsJournalEntry {
    std::uintptr_t address = 0;
    std::uint64_t before = 0;
    std::uint64_t previous = 0;
    std::uint64_t after = 0;
    std::uint16_t officer_id = domain::NO_CITY_OFFICER;
    std::uint8_t width = 0;
};

struct CorpsDataWrite {
    std::uintptr_t address = 0;
    std::uint64_t before = 0;
    std::uint64_t after = 0;
    std::uint16_t row = NO_CORPS_ROW;
    std::uint8_t city = domain::NO_CITY;
    std::uint8_t width = 0;
    bool status = false;
    bool general = false;
};

struct CorpsAutoTarget {
    std::uintptr_t corps = 0;
    std::uintptr_t force = 0;
    std::uint64_t number = 0;
    std::uint16_t general = domain::NO_CITY_OFFICER;
};

struct CorpsRelationshipEdge {
    std::uint32_t next = 0xffffffff;
    std::uint16_t target = NO_CORPS_ROW;
};

struct CorpsDeployment {
    domain::CorpsSnapshot snapshot;
    std::array<CorpsRosterEntry, domain::CORPS_OFFICER_CAPACITY> rows{};
    std::array<std::uint16_t, domain::CORPS_OFFICER_CAPACITY + 1> id_to_row{};
    std::array<std::uint16_t, domain::CORPS_OFFICER_CAPACITY> order{};
    std::array<std::uint16_t, domain::CORPS_OFFICER_CAPACITY> recommendations{};
    std::array<std::uint16_t, domain::CORPS_OFFICER_CAPACITY> work_order{};
    std::array<std::uint16_t, domain::CORPS_OFFICER_CAPACITY> sort_buffer{};
    std::array<std::uint16_t, domain::CORPS_OFFICER_CAPACITY> parents{};
    std::array<std::uint16_t, domain::CORPS_OFFICER_CAPACITY> group_head{};
    std::array<std::uint16_t, domain::CORPS_OFFICER_CAPACITY> group_tail{};
    std::array<std::uint16_t, domain::CORPS_OFFICER_CAPACITY> group_next{};
    std::array<std::uint32_t, 65536> direct_pairs{};
    std::array<CorpsRelationshipEdge, 70000> relationship_edges{};
    std::array<std::array<std::uint32_t, domain::CORPS_OFFICER_CAPACITY>, 3> relationship_heads{};
    std::array<std::array<std::uint32_t, domain::CORPS_OFFICER_CAPACITY>, 3> relationship_tails{};
    std::array<std::uintptr_t, domain::CITY_COUNT> city_forces{};
    std::array<std::uintptr_t, domain::CITY_COUNT> city_corps{};
    std::array<std::uintptr_t, domain::CITY_COUNT> city_governors{};
    std::array<std::uint16_t, domain::CITY_COUNT> baseline{};
    std::array<bool, domain::CITY_COUNT> baseline_cities{};
    std::array<std::uint16_t, domain::CITY_COUNT> group_members{};
    std::array<std::uint16_t, domain::CITY_COUNT> eligible{};
    std::array<std::uint16_t, domain::CITY_COUNT> swap_out{};
    std::array<CorpsAutoTarget, domain::CITY_COUNT> auto_targets{};
    std::array<CorpsJournalEntry, CORPS_JOURNAL_CAPACITY> journal{};
    std::array<std::uint16_t, domain::CORPS_OFFICER_CAPACITY> city_journal{};
    std::array<std::uint16_t, domain::CORPS_OFFICER_CAPACITY> status_journal{};
    std::array<std::uint16_t, domain::CITY_COUNT> governor_journal{};
    std::uintptr_t module_base = 0;
    std::uintptr_t game_root = 0;
    std::uintptr_t player = 0;
    std::uintptr_t player_force = 0;
    std::uintptr_t city_table = 0;
    std::uintptr_t roster = 0;
    std::uintptr_t roster_node = 0;
    std::uintptr_t corps = 0;
    std::uintptr_t baseline_corps = 0;
    std::uintptr_t relation_table = 0;
    std::uintptr_t synergy_table = 0;
    std::uintptr_t candidate = 0;
    std::uintptr_t synergy_scan_cursor = 0;
    domain::GameEpoch epoch{};
    domain::CorpsRequest request;
    domain::CorpsStatus failure = domain::CorpsStatus::WriteFailed;
    CorpsStage stage = CorpsStage::Idle;
    CorpsStage after_sort = CorpsStage::Idle;
    CorpsSort sort_kind = CorpsSort::Roster;
    std::size_t count = 0;
    std::size_t assigned_count = 0;
    std::size_t cursor = 0;
    std::size_t secondary = 0;
    std::size_t sort_count = 0;
    std::size_t sort_width = 1;
    std::size_t sort_start = 0;
    std::size_t sort_left = 0;
    std::size_t sort_right = 0;
    std::size_t sort_output = 0;
    std::size_t rear_count = 0;
    std::size_t front_take = 0;
    std::size_t journal_count = 0;
    std::size_t journal_moved = 0;
    std::size_t journal_swapped = 0;
    std::size_t journal_appointed = 0;
    std::size_t auto_cursor = 0;
    std::size_t candidate_index = 0;
    std::size_t candidate_valid = 0;
    std::size_t candidate_invalid = 0;
    std::size_t iterations = 0;
    std::size_t group_count = 0;
    std::size_t group_movable = 0;
    std::size_t relationship_count = 0;
    std::uint32_t relationship_cursor = 0xffffffff;
    std::uint64_t next_observation_ms = 0;
    std::uint64_t write_sequence = 0;
    std::uint16_t group_root = NO_CORPS_ROW;
    std::uint16_t member = NO_CORPS_ROW;
    std::uint16_t best_row = NO_CORPS_ROW;
    std::uint16_t first_candidate = NO_CORPS_ROW;
    std::uint16_t general_journal = NO_CORPS_ROW;
    std::uint16_t observed_year = 0;
    std::uint8_t observed_month = 0;
    std::uint8_t observed_phase = 0;
    std::uint8_t work_phase = 0;
    std::uint8_t plan_city = domain::NO_CITY;
    std::uint8_t group_city = domain::NO_CITY;
    std::uint8_t saved_city = domain::NO_CITY;
    bool building = false;
    bool apply_all = false;
    bool auto_running = false;
    bool auto_pending = false;
    bool group_low = false;
    bool group_front = false;
    bool group_conflict = false;
    bool group_anchor = false;
    bool progress = false;
    bool sort_source_buffer = false;
    bool baseline_valid = false;
    bool stopping = false;
};

domain::CorpsStatus request_corps_deployment(CorpsDeployment& state, std::uintptr_t module_base,
    std::uintptr_t game_root, std::uintptr_t player, domain::GameEpoch epoch, const domain::CorpsRequest& request);

// Work is resumable: a dispatch reads/plans at most max_units records and never
// allocates. All mutation runs at the owning game boundary, with live binding
// checks and a retained rollback journal. Successfully completed edits persist.
void tick_corps_deployment(CorpsDeployment& state, std::uintptr_t module_base, std::uintptr_t game_root,
    std::uintptr_t player, domain::GameEpoch epoch, std::uint64_t real_milliseconds, std::size_t max_units = 64,
    const domain::WorkBudget& work_budget = {});
bool stop_corps_deployment(CorpsDeployment& state, std::uintptr_t module_base, domain::GameEpoch epoch);

} // namespace s8::game
