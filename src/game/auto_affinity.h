#pragma once

#include "../domain/auto_affinity.h"

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

struct AffinityRelationScan {
    std::uintptr_t roster_base = 0;
    std::uintptr_t direct_base = 0;
    std::uintptr_t synergetic_base = 0;
    std::size_t direct_cursor = 0;
    std::size_t synergetic_cursor = 0;
    bool direct_done = false;
    bool synergetic_done = false;
    std::vector<std::uint32_t> excluded_pairs;
};

struct AffinityRelationBases {
    std::uintptr_t direct = 0;
    std::uintptr_t synergetic = 0;
};

struct AffinityRelationSearch {
    std::uintptr_t game_base = 0;
    std::uintptr_t roster_base = 0;
    std::uintptr_t cursor = 0;
    std::uintptr_t result = 0;
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

enum class AutoAffinityPhase { Idle, Candidates, SearchRelations, ScanRelations, Apply };
enum class AutoAffinityOutcome { Completed, InsufficientCandidates, NoRelationTable, DateChanged };

struct AutoAffinityReport {
    AutoAffinityOutcome outcome;
    std::size_t applied;
    std::size_t stale;
    std::size_t failed;
};

struct AutoAffinity {
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
    AffinityRelationSearch search;
    AffinityRelationScan relations;
    AffinityPairCursor pair_cursor;
    std::size_t applied = 0;
    std::size_t stale = 0;
    std::size_t failed = 0;
    std::optional<AutoAffinityReport> report;
};

// Called from the game-owned context with a roster resolved for the current session.
void collect_affinity_candidates(AffinityCandidateScan& scan, std::size_t max_slots);
void collect_affinity_relations(AffinityRelationScan& scan, std::size_t max_slots);
AffinityRelationBases resolve_affinity_relation_candidates(std::uintptr_t game_base, std::uintptr_t roster_base);
void search_affinity_synergetic_table(AffinityRelationSearch& search, std::size_t max_addresses);
void sort_affinity_candidates(std::vector<AffinityCandidate>& candidates);
// A null result can mean the budget was consumed; cursor.left reaches the candidate count at completion.
std::optional<AffinityPair> next_affinity_pair(const AffinityCandidateScan& candidates,
                                                const AffinityRelationScan& relations,
                                                AffinityPairCursor& cursor, std::size_t max_pairs);
AffinityApplyResult apply_affinity_pair(std::uintptr_t data_center, const AffinityPair& pair);
void tick_auto_affinity(AutoAffinity& state, std::uintptr_t module_base, std::uintptr_t game_base,
                        std::uintptr_t player, std::uint64_t now_ms, bool enabled);

} // namespace s8::game
