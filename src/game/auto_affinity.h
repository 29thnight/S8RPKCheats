#pragma once

#include "../domain/auto_affinity.h"
#include "../domain/work_budget.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace s8::game {

struct AffinityCandidate {
    std::uint16_t id;
    std::uintptr_t base;
    std::uintptr_t force;
    std::uintptr_t city;
    domain::AffinityTraits traits;
};

struct AffinityCandidateScan {
    std::uintptr_t roster_base = 0;
    std::uint16_t protagonist_id = 0;
    std::size_t cursor = 0;
    std::array<bool, 5103> seen{};
    std::vector<AffinityCandidate> candidates;
};

enum class AffinitySortStage { Build, SiftBuild, Extract, SiftExtract, Complete };

struct AffinitySortState {
    std::size_t heap_end = 0;
    std::size_t build_cursor = 0;
    std::size_t sift_root = 0;
    AffinitySortStage stage = AffinitySortStage::Build;
    bool initialized = false;
};

struct AffinityRelationScan {
    std::uintptr_t roster_base = 0;
    std::uintptr_t direct_base = 0;
    std::uintptr_t synergetic_base = 0;
    std::size_t direct_cursor = 0;
    std::size_t synergetic_cursor = 0;
    bool direct_done = false;
    bool synergetic_done = false;
    bool complete = false;
    AffinitySortState sort;
    std::size_t unique_cursor = 0;
    std::size_t unique_count = 0;
    std::vector<std::uint32_t> excluded_pairs;
};

struct AffinityRelationBases {
    std::uintptr_t direct = 0;
    std::uintptr_t synergetic = 0;
};

struct AffinityTableValidation {
    std::uintptr_t base = 0;
    std::uintptr_t roster_base = 0;
    std::size_t cursor = 0;
    unsigned valid = 0;
    unsigned invalid = 0;
    bool synergetic = false;
    bool complete = false;
    bool accepted = false;
};

struct AffinityRelationResolve {
    AffinityRelationBases bases;
    AffinityTableValidation validation;
    std::size_t candidate = 0;
    bool complete = false;
};

struct AffinityRelationSearch {
    std::uintptr_t game_base = 0;
    std::uintptr_t roster_base = 0;
    std::uintptr_t cursor = 0;
    std::uintptr_t result = 0;
    AffinityTableValidation validation;
    bool complete = false;
};

struct AffinityPair {
    AffinityCandidate left;
    AffinityCandidate right;
    std::uint32_t offset;
};

enum class AffinityApplyResult { Applied, Unchanged, Stale, Failed };

struct AffinityPairCursor {
    std::size_t left = 0;
    std::size_t right = 1;
};

enum class AutoAffinityPhase { Idle, Candidates, SortCandidates, ResolveRelations, SearchRelations, ScanRelations, Apply };
enum class AutoAffinityOutcome { Completed, InsufficientCandidates, NoRelationTable, DateChanged };

struct AutoAffinityReport {
    AutoAffinityOutcome outcome;
    std::size_t applied;
    std::size_t stale;
    std::size_t failed;
};

struct AutoAffinity {
    AutoAffinity();

    domain::AffinityCalendar calendar;
    std::uint64_t last_poll_ms = 0;
    std::uintptr_t data_center = 0;
    std::uintptr_t player = 0;
    std::uintptr_t game_base = 0;
    std::uintptr_t roster_base = 0;
    std::uint16_t job_year = 0;
    std::uint8_t job_month = 0;
    AutoAffinityPhase phase = AutoAffinityPhase::Idle;
    AffinityCandidateScan candidates;
    AffinitySortState candidate_sort;
    AffinityRelationResolve resolve;
    AffinityRelationSearch search;
    AffinityRelationScan relations;
    AffinityPairCursor pair_cursor;
    std::size_t applied = 0;
    std::size_t stale = 0;
    std::size_t failed = 0;
    std::optional<AutoAffinityReport> report;
};

// Preserve the startup allocations across disabling and game-epoch changes.
void reset_auto_affinity(AutoAffinity& state);

// Called from the game-owned context with a roster resolved for the current session.
void collect_affinity_candidates(AffinityCandidateScan& scan, std::size_t max_slots,
    const domain::WorkBudget& work_budget = {});
void collect_affinity_relations(AffinityRelationScan& scan, std::size_t max_slots,
    const domain::WorkBudget& work_budget = {});
AffinityRelationBases resolve_affinity_relation_candidates(std::uintptr_t game_base, std::uintptr_t roster_base);
void search_affinity_synergetic_table(AffinityRelationSearch& search, std::size_t max_addresses,
    const domain::WorkBudget& work_budget = {});
void sort_affinity_candidates(std::vector<AffinityCandidate>& candidates);
// A null result can mean the budget was consumed; cursor.left reaches the candidate count at completion.
std::optional<AffinityPair> next_affinity_pair(const AffinityCandidateScan& candidates,
                                                const AffinityRelationScan& relations,
                                                AffinityPairCursor& cursor, std::size_t max_pairs,
                                                const domain::WorkBudget& work_budget = {});
AffinityApplyResult apply_affinity_pair(std::uintptr_t data_center, const AffinityPair& pair);
void tick_auto_affinity(AutoAffinity& state, std::uintptr_t module_base, std::uintptr_t game_base,
                        std::uintptr_t player, std::uint64_t now_ms, bool enabled,
                        const domain::WorkBudget& work_budget = {});

} // namespace s8::game
