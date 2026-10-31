#include "auto_affinity.h"
#include "officer_roster.h"
#include "../platform/windows/process_memory.h"

#include <algorithm>
#include <span>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

namespace s8::game {
namespace {

constexpr std::size_t AFFINITY_CANDIDATE_CAPACITY = 5102;
constexpr std::size_t AFFINITY_PAIR_CAPACITY = 3000 * 10 + 5000;

void reset_candidates(AffinityCandidateScan& scan) {
    scan.roster_base = 0;
    scan.protagonist_id = 0;
    scan.cursor = 0;
    scan.seen.fill(false);
    scan.candidates.clear();
}

void reset_relations(AffinityRelationScan& scan) {
    scan.roster_base = 0;
    scan.direct_base = 0;
    scan.synergetic_base = 0;
    scan.direct_cursor = 0;
    scan.synergetic_cursor = 0;
    scan.direct_done = false;
    scan.synergetic_done = false;
    scan.complete = false;
    scan.sort = {};
    scan.unique_cursor = 0;
    scan.unique_count = 0;
    scan.excluded_pairs.clear();
}

std::uint16_t officer_id(std::uintptr_t pointer, std::uintptr_t roster_base) {
    if (pointer < roster_base) {
        return 0;
    }
    const auto offset = pointer - roster_base;
    if (offset >= 5102 * 0x3d0 || offset % 0x3d0 != 0) {
        return 0;
    }
    std::uint16_t id = 0;
    if (!platform::windows::read_memory(pointer + 8, std::as_writable_bytes(std::span(&id, 1))) ||
        id < 1 || id > 5102) {
        return 0;
    }
    return id;
}

bool valid_roster_pointer(std::uintptr_t pointer, std::uintptr_t roster_base) {
    return pointer >= roster_base && pointer - roster_base < 5102 * 0x3d0 &&
        (pointer - roster_base) % 0x3d0 == 0;
}

void validate_affinity_table(AffinityTableValidation& validation, const domain::WorkBudget& work_budget) {
    const std::size_t limit = validation.synergetic ? 600 : 128;

    while (!validation.complete && validation.cursor < limit && !work_budget.exhausted()) {
        const std::uintptr_t slot = validation.base + validation.cursor * (validation.synergetic ? 0x20 : 0x40);
        std::uint8_t type = 0;

        ++validation.cursor;
        if (!platform::windows::read_memory(slot + (validation.synergetic ? 0x18 : 8),
                std::as_writable_bytes(std::span(&type, 1)))) {
            validation.complete = true;
            return;
        }
        if (type == 0) {
            continue;
        }
        if (type > (validation.synergetic ? 2 : 4)) {
            if (++validation.invalid > 3) {
                validation.complete = true;
                return;
            }
            continue;
        }
        std::array<std::uintptr_t, 2> pointers{};

        if (!platform::windows::read_memory(slot + (validation.synergetic ? 8 : 0x10),
                std::as_writable_bytes(std::span(pointers))) ||
            !valid_roster_pointer(pointers[0], validation.roster_base) ||
            !valid_roster_pointer(pointers[1], validation.roster_base)) {
            if (++validation.invalid > 3) {
                validation.complete = true;
                return;
            }
            continue;
        }
        ++validation.valid;
    }
    if (validation.cursor == limit) {
        validation.complete = true;
        validation.accepted = validation.valid != 0;
    }
}

void resolve_affinity_relations(AffinityRelationResolve& resolve, std::uintptr_t game_base,
    std::uintptr_t roster_base, const domain::WorkBudget& work_budget) {
    constexpr std::array<std::uintptr_t, 2> SYNERGY_OFFSETS{0x547330, 0x433210};
    constexpr std::array<std::uintptr_t, 2> DIRECT_OFFSETS{0x57ae38, 0x462ba8};
    constexpr std::array<std::uintptr_t, 2> BIASES{0x80, 0xc0};

    while (!resolve.complete && !work_budget.exhausted()) {
        if (resolve.candidate == 6) {
            resolve.complete = true;
            return;
        }
        if (resolve.validation.base == 0) {
            std::uintptr_t pointer = 0;
            const bool synergetic = resolve.candidate < 2;
            const std::size_t direct = synergetic ? 0 : resolve.candidate - 2;
            const std::uintptr_t offset = synergetic ? SYNERGY_OFFSETS[resolve.candidate] : DIRECT_OFFSETS[direct / 2];

            if (!platform::windows::read_memory(game_base + offset, std::as_writable_bytes(std::span(&pointer, 1))) ||
                pointer <= (synergetic ? 0x10000u : 0x10080u)) {
                ++resolve.candidate;
                continue;
            }
            resolve.validation.base = synergetic ? pointer + 0xa0 : pointer - BIASES[direct % 2];
            resolve.validation.roster_base = roster_base;
            resolve.validation.synergetic = synergetic;
        }
        validate_affinity_table(resolve.validation, work_budget);
        if (!resolve.validation.complete) {
            return;
        }
        if (resolve.validation.accepted) {
            if (resolve.candidate < 2) {
                resolve.bases.synergetic = resolve.validation.base;
                resolve.candidate = 2;
            } else {
                resolve.bases.direct = resolve.validation.base;
                resolve.complete = true;
            }
        } else {
            ++resolve.candidate;
        }
        resolve.validation = {};
    }
}

bool candidate_less(const AffinityCandidate& left, const AffinityCandidate& right) {
    if (left.force != right.force) {
        return left.force < right.force;
    }
    if (left.city != right.city) {
        return left.city < right.city;
    }
    return left.id < right.id;
}

bool pair_less(std::uint32_t left, std::uint32_t right) {
    return left < right;
}

template<typename T, typename Compare>
bool sort_affinity_values(std::vector<T>& values, AffinitySortState& sort,
    const domain::WorkBudget& work_budget, Compare less) {
    if (!sort.initialized) {
        sort.heap_end = values.size();
        sort.build_cursor = values.size() / 2;
        sort.stage = AffinitySortStage::Build;
        sort.initialized = true;
    }
    while (sort.stage != AffinitySortStage::Complete && !work_budget.exhausted()) {
        if (sort.stage == AffinitySortStage::Build) {
            if (sort.build_cursor == 0) {
                sort.stage = AffinitySortStage::Extract;
            } else {
                sort.sift_root = --sort.build_cursor;
                sort.stage = AffinitySortStage::SiftBuild;
            }
        } else if (sort.stage == AffinitySortStage::Extract) {
            if (sort.heap_end <= 1) {
                sort.stage = AffinitySortStage::Complete;
                break;
            }
            std::swap(values[0], values[--sort.heap_end]);
            sort.sift_root = 0;
            sort.stage = AffinitySortStage::SiftExtract;
        } else {
            std::size_t child = sort.sift_root * 2 + 1;
            const AffinitySortStage next = sort.stage == AffinitySortStage::SiftBuild
                ? AffinitySortStage::Build : AffinitySortStage::Extract;

            if (child >= sort.heap_end) {
                sort.stage = next;
                continue;
            }
            if (child + 1 < sort.heap_end && less(values[child], values[child + 1])) {
                ++child;
            }
            if (!less(values[sort.sift_root], values[child])) {
                sort.stage = next;
            } else {
                std::swap(values[sort.sift_root], values[child]);
                sort.sift_root = child;
            }
        }
    }
    return sort.stage == AffinitySortStage::Complete;
}

} // namespace

AutoAffinity::AutoAffinity() {
    candidates.candidates.reserve(AFFINITY_CANDIDATE_CAPACITY);
    relations.excluded_pairs.reserve(AFFINITY_PAIR_CAPACITY);
}

void reset_auto_affinity(AutoAffinity& state) {
    state.calendar = {};
    state.last_poll_ms = 0;
    state.data_center = 0;
    state.player = 0;
    state.game_base = 0;
    state.roster_base = 0;
    state.job_year = 0;
    state.job_month = 0;
    state.phase = AutoAffinityPhase::Idle;
    reset_candidates(state.candidates);
    state.candidate_sort = {};
    state.resolve = {};
    state.search = {};
    reset_relations(state.relations);
    state.pair_cursor = {};
    state.applied = 0;
    state.stale = 0;
    state.failed = 0;
    state.report.reset();
}

AffinityRelationBases resolve_affinity_relation_candidates(std::uintptr_t game_base, std::uintptr_t roster_base) {
    AffinityRelationResolve resolve;

    resolve_affinity_relations(resolve, game_base, roster_base, {});
    return resolve.bases;
}

void search_affinity_synergetic_table(AffinityRelationSearch& search, std::size_t max_addresses,
    const domain::WorkBudget& work_budget) {
    if (search.complete) {
        return;
    }
    const std::uintptr_t end = search.game_base + 0x800000;
    std::size_t checked = 0;

    if (search.cursor == 0) {
        search.cursor = (search.game_base + 7) & ~std::uintptr_t{7};
    }
    while ((search.validation.base != 0 || search.cursor < end) && checked < max_addresses && !work_budget.exhausted()) {
        if (search.validation.base != 0) {
            validate_affinity_table(search.validation, work_budget);
            if (!search.validation.complete) {
                return;
            }
            if (search.validation.accepted) {
                search.result = search.validation.base;
                search.complete = true;
                return;
            }
            search.validation = {};
            continue;
        }
        MEMORY_BASIC_INFORMATION memory{};

        if (VirtualQuery(reinterpret_cast<const void*>(search.cursor), &memory, sizeof(memory)) != sizeof(memory)) {
            search.complete = true;
            return;
        }
        const std::uintptr_t region_start = reinterpret_cast<std::uintptr_t>(memory.BaseAddress);
        const std::uintptr_t region_end = std::min(region_start + memory.RegionSize, end);

        if (memory.State != MEM_COMMIT || (memory.Protect & PAGE_GUARD) ||
            (memory.Protect & 0xff) == PAGE_NOACCESS) {
            search.cursor = (region_end + 7) & ~std::uintptr_t{7};
            continue;
        }
        while (search.cursor + sizeof(std::uintptr_t) <= region_end && checked < max_addresses && !work_budget.exhausted()) {
            std::uintptr_t pointer = 0;
            const std::uintptr_t address = search.cursor;

            search.cursor += sizeof(std::uintptr_t);
            ++checked;
            if (platform::windows::read_memory(address, std::as_writable_bytes(std::span(&pointer, 1))) && pointer > 0x10000) {
                search.validation = {};
                search.validation.base = pointer + 0xa0;
                search.validation.roster_base = search.roster_base;
                search.validation.synergetic = true;
                break;
            }
        }
        if (search.cursor + sizeof(std::uintptr_t) > region_end) {
            search.cursor = (region_end + 7) & ~std::uintptr_t{7};
        }
    }
    search.complete = search.cursor >= end && search.validation.base == 0;
}

void sort_affinity_candidates(std::vector<AffinityCandidate>& candidates) {
    std::sort(candidates.begin(), candidates.end(), [](const auto& left, const auto& right) {
        if (left.force != right.force) {
            return left.force < right.force;
        }
        if (left.city != right.city) {
            return left.city < right.city;
        }
        return left.id < right.id;
    });
}

std::optional<AffinityPair> next_affinity_pair(const AffinityCandidateScan& candidates,
                                                const AffinityRelationScan& relations,
                                                AffinityPairCursor& cursor, std::size_t max_pairs,
                                                const domain::WorkBudget& work_budget) {
    std::size_t examined = 0;
    while (cursor.left < candidates.candidates.size() && examined < max_pairs && !work_budget.exhausted()) {
        ++examined;
        if (cursor.right >= candidates.candidates.size() ||
            candidates.candidates[cursor.left].force != candidates.candidates[cursor.right].force ||
            candidates.candidates[cursor.left].city != candidates.candidates[cursor.right].city) {
            ++cursor.left;
            cursor.right = cursor.left + 1;
            continue;
        }
        const auto& left = candidates.candidates[cursor.left];
        const auto& right = candidates.candidates[cursor.right++];
        const auto low = std::min(left.id, right.id);
        const auto high = std::max(left.id, right.id);
        const auto key = (static_cast<std::uint32_t>(low) << 16) | high;
        if (std::binary_search(relations.excluded_pairs.begin(), relations.excluded_pairs.end(), key)) {
            continue;
        }
        if (const auto offset = domain::affinity_pair_offset(left.id, right.id)) {
            return AffinityPair{left, right, *offset};
        }
    }
    return std::nullopt;
}

AffinityApplyResult apply_affinity_pair(std::uintptr_t data_center, const AffinityPair& pair) {
    const auto read_candidate = [](const AffinityCandidate& candidate, domain::AffinityTraits& traits) {
        std::uint16_t id = 0;
        std::uint8_t status = 0;
        std::uintptr_t force = 0, city = 0;
        return platform::windows::read_memory(candidate.base + 8, std::as_writable_bytes(std::span(&id, 1))) &&
            platform::windows::read_memory(candidate.base + 0x10, std::as_writable_bytes(std::span(&status, 1))) &&
            platform::windows::read_memory(candidate.base + 0x18, std::as_writable_bytes(std::span(&force, 1))) &&
            platform::windows::read_memory(candidate.base + 0x20, std::as_writable_bytes(std::span(&city, 1))) &&
            platform::windows::read_memory(candidate.base + 0x5d, std::as_writable_bytes(std::span(&traits.compatibility, 1))) &&
            platform::windows::read_memory(candidate.base + 0x83, std::as_writable_bytes(std::span(&traits.interest, 1))) &&
            platform::windows::read_memory(candidate.base + 0xa4, std::as_writable_bytes(std::span(&traits.favored_reputation, 1))) &&
            id == candidate.id && force == candidate.force && city == candidate.city &&
            (status == 0x18 || status == 0x28 || status == 0x38 || status == 0x48 ||
             status == 0xc8 || status == 0xd8 || status == 0xe8) && traits.compatibility <= 149 &&
            traits.favored_reputation >= 1 && traits.favored_reputation <= 6;
    };
    domain::AffinityTraits left{}, right{};
    if (!read_candidate(pair.left, left) || !read_candidate(pair.right, right)) {
        return AffinityApplyResult::Stale;
    }
    const auto address = data_center + 0x24206 + pair.offset;
    std::uint8_t current = 0;
    if (!platform::windows::read_memory(address, std::as_writable_bytes(std::span(&current, 1)))) {
        return AffinityApplyResult::Failed;
    }
    const auto next = domain::grow_affinity(current, left, right);
    if (!next) {
        return AffinityApplyResult::Unchanged;
    }
    std::uint8_t written = 0;
    if (!platform::windows::write_data_memory(address, std::as_bytes(std::span(&*next, 1))) ||
        !platform::windows::read_memory(address, std::as_writable_bytes(std::span(&written, 1))) || written != *next) {
        return AffinityApplyResult::Failed;
    }
    return AffinityApplyResult::Applied;
}

void collect_affinity_candidates(AffinityCandidateScan& scan, std::size_t max_slots,
    const domain::WorkBudget& work_budget) {
    const auto end = std::min(scan.cursor + max_slots, std::size_t{5102});
    for (; scan.cursor < end && !work_budget.exhausted(); ++scan.cursor) {
        const auto base = scan.roster_base + scan.cursor * 0x3d0;
        std::uint16_t id = 0, face = 0, merit = 0, reputation = 0;
        std::uint32_t gold = 0;
        std::uint8_t strategy = 0;
        if (!platform::windows::read_memory(base + 0x08, std::as_writable_bytes(std::span(&id, 1))) ||
            !platform::windows::read_memory(base + 0x2e, std::as_writable_bytes(std::span(&face, 1))) ||
            !platform::windows::read_memory(base + 0xe8, std::as_writable_bytes(std::span(&gold, 1))) ||
            !platform::windows::read_memory(base + 0x100, std::as_writable_bytes(std::span(&merit, 1))) ||
            !platform::windows::read_memory(base + 0x108, std::as_writable_bytes(std::span(&reputation, 1))) ||
            !platform::windows::read_memory(base + 0xed, std::as_writable_bytes(std::span(&strategy, 1))) ||
            id < 1 || id > 5102 || scan.seen[id]) {
            continue;
        }
        scan.seen[id] = true;
        std::uint8_t status = 0;
        AffinityCandidate candidate{id, base, 0, 0, {}};
        if (id == scan.protagonist_id ||
            !platform::windows::read_memory(base + 0x10, std::as_writable_bytes(std::span(&status, 1))) ||
            (status != 0x18 && status != 0x28 && status != 0x38 && status != 0x48 &&
             status != 0xc8 && status != 0xd8 && status != 0xe8) ||
            !platform::windows::read_memory(base + 0x18, std::as_writable_bytes(std::span(&candidate.force, 1))) ||
            !platform::windows::read_memory(base + 0x20, std::as_writable_bytes(std::span(&candidate.city, 1))) ||
            candidate.force <= 0x10000 || candidate.city <= 0x10000 ||
            !platform::windows::read_memory(base + 0x5d, std::as_writable_bytes(std::span(&candidate.traits.compatibility, 1))) ||
            !platform::windows::read_memory(base + 0x83, std::as_writable_bytes(std::span(&candidate.traits.interest, 1))) ||
            !platform::windows::read_memory(base + 0xa4, std::as_writable_bytes(std::span(&candidate.traits.favored_reputation, 1))) ||
            candidate.traits.compatibility > 149 || candidate.traits.favored_reputation < 1 ||
            candidate.traits.favored_reputation > 6) {
            continue;
        }
        scan.candidates.push_back(candidate);
    }
}

void collect_affinity_relations(AffinityRelationScan& scan, std::size_t max_slots,
    const domain::WorkBudget& work_budget) {
    const auto add_pair = [&](std::uint16_t a, std::uint16_t b) {
        if (a && b && a != b) {
            const auto low = std::min(a, b);
            const auto high = std::max(a, b);
            scan.excluded_pairs.push_back((static_cast<std::uint32_t>(low) << 16) | high);
        }
    };
    if (!scan.direct_done) {
        if (!scan.direct_base) {
            scan.direct_done = true;
        } else {
            const auto end = std::min(scan.direct_cursor + max_slots, std::size_t{3000});
            for (; scan.direct_cursor < end && !work_budget.exhausted(); ++scan.direct_cursor) {
                const auto slot = scan.direct_base + scan.direct_cursor * 0x40;
                std::uint8_t relation = 0;
                if (!platform::windows::read_memory(slot + 8, std::as_writable_bytes(std::span(&relation, 1)))) {
                    scan.direct_done = true;
                    break;
                }
                if (relation < 1 || relation > 4) {
                    continue;
                }
                std::array<std::uintptr_t, 5> pointers{};
                if (!platform::windows::read_memory(slot + 0x10, std::as_writable_bytes(std::span(pointers)))) {
                    scan.direct_done = true;
                    break;
                }
                std::array<std::uint16_t, 5> ids{};
                for (std::size_t i = 0; i < ids.size(); ++i) {
                    ids[i] = officer_id(pointers[i], scan.roster_base);
                }
                for (std::size_t a = 0; a < ids.size(); ++a) {
                    for (std::size_t b = a + 1; b < ids.size(); ++b) {
                        add_pair(ids[a], ids[b]);
                    }
                }
            }
            scan.direct_done = scan.direct_done || scan.direct_cursor == 3000;
        }
        return;
    }
    if (!scan.synergetic_done) {
        if (!scan.synergetic_base) {
            scan.synergetic_done = true;
        } else {
            const auto end = std::min(scan.synergetic_cursor + max_slots, std::size_t{5000});
            for (; scan.synergetic_cursor < end && !work_budget.exhausted(); ++scan.synergetic_cursor) {
                const auto slot = scan.synergetic_base + scan.synergetic_cursor * 0x20;
                std::array<std::uintptr_t, 2> pointers{};
                std::array<std::uint8_t, 2> type{};
                if (!platform::windows::read_memory(slot + 8, std::as_writable_bytes(std::span(pointers))) ||
                    !platform::windows::read_memory(slot + 0x18, std::as_writable_bytes(std::span(type)))) {
                    scan.synergetic_done = true;
                    break;
                }
                if ((type[0] == 1 || type[0] == 2) && type[1] == 0) {
                    add_pair(officer_id(pointers[0], scan.roster_base), officer_id(pointers[1], scan.roster_base));
                }
            }
            scan.synergetic_done = scan.synergetic_done || scan.synergetic_cursor == 5000;
        }
    }
    if (scan.synergetic_done && !scan.complete &&
        sort_affinity_values(scan.excluded_pairs, scan.sort, work_budget, pair_less)) {
        while (scan.unique_cursor < scan.excluded_pairs.size() && !work_budget.exhausted()) {
            const std::uint32_t value = scan.excluded_pairs[scan.unique_cursor++];

            if (scan.unique_count == 0 || scan.excluded_pairs[scan.unique_count - 1] != value) {
                scan.excluded_pairs[scan.unique_count++] = value;
            }
        }
        if (scan.unique_cursor == scan.excluded_pairs.size()) {
            scan.excluded_pairs.resize(scan.unique_count);
            scan.complete = true;
        }
    }
}

void tick_auto_affinity(AutoAffinity& state, std::uintptr_t module_base, std::uintptr_t game_base,
                        std::uintptr_t player, std::uint64_t now_ms, bool enabled,
                        const domain::WorkBudget& work_budget) {
    if (!enabled) {
        if (state.phase != AutoAffinityPhase::Idle || state.data_center != 0) {
            reset_auto_affinity(state);
        }
        return;
    }
    if (work_budget.exhausted()) {
        return;
    }
    std::uintptr_t data_center = 0;
    std::uint16_t player_id = 0;
    if (!module_base || game_base <= 0x10000 || player <= 0x10000 ||
        !platform::windows::read_memory(module_base + 0x02e98bc8, std::as_writable_bytes(std::span(&data_center, 1))) ||
        data_center <= 0x10000 ||
        !platform::windows::read_memory(player + 8, std::as_writable_bytes(std::span(&player_id, 1))) ||
        player_id < 1 || player_id > 5102) {
        return;
    }
    if (state.data_center != data_center || state.player != player || state.game_base != game_base) {
        reset_auto_affinity(state);
        state.data_center = data_center;
        state.player = player;
        state.game_base = game_base;
    }
    if (state.phase == AutoAffinityPhase::Idle) {
        if (now_ms - state.last_poll_ms < 250) {
            return;
        }
        state.last_poll_ms = now_ms;
    }
    std::uint16_t year = 0;
    std::uint8_t month = 0;
    if (!platform::windows::read_memory(data_center + 0x72d0, std::as_writable_bytes(std::span(&year, 1))) ||
        !platform::windows::read_memory(data_center + 0x72d2, std::as_writable_bytes(std::span(&month, 1))) ||
        year == 0 || month < 1 || month > 12) {
        return;
    }
    if (state.phase == AutoAffinityPhase::Idle) {
        if (!domain::advance_affinity_calendar(state.calendar, year, month)) {
            return;
        }
        const auto roster = resolve_officer_roster_base(module_base);
        if (!roster) {
            return;
        }
        state.roster_base = roster;
        reset_candidates(state.candidates);
        state.candidates.roster_base = roster;
        state.candidates.protagonist_id = player_id;
        reset_relations(state.relations);
        state.search = {};
        state.resolve = {};
        state.candidate_sort = {};
        state.pair_cursor = {};
        state.applied = 0;
        state.stale = 0;
        state.failed = 0;
        state.job_year = year;
        state.job_month = month;
        state.phase = AutoAffinityPhase::Candidates;
    } else if (state.job_year != year || state.job_month != month) {
        state.report = AutoAffinityReport{AutoAffinityOutcome::DateChanged, state.applied, state.stale, state.failed};
        state.phase = AutoAffinityPhase::Idle;
        return;
    }
    if (state.candidates.protagonist_id != player_id ||
        resolve_officer_roster_base(module_base) != state.roster_base) {
        reset_auto_affinity(state);
        return;
    }
    switch (state.phase) {
    case AutoAffinityPhase::Idle:
        break;
    case AutoAffinityPhase::Candidates:
        collect_affinity_candidates(state.candidates, 64, work_budget);
        if (state.candidates.cursor == 5102) {
            if (state.candidates.candidates.size() < 2) {
                state.report = AutoAffinityReport{AutoAffinityOutcome::InsufficientCandidates, 0, 0, 0};
                state.phase = AutoAffinityPhase::Idle;
                break;
            }
            state.phase = AutoAffinityPhase::SortCandidates;
        }
        break;
    case AutoAffinityPhase::SortCandidates:
        if (sort_affinity_values(state.candidates.candidates, state.candidate_sort, work_budget, candidate_less)) {
            state.phase = AutoAffinityPhase::ResolveRelations;
        }
        break;
    case AutoAffinityPhase::ResolveRelations:
        resolve_affinity_relations(state.resolve, game_base, state.roster_base, work_budget);
        if (state.resolve.complete) {
            const AffinityRelationBases bases = state.resolve.bases;

            state.relations.roster_base = state.roster_base;
            state.relations.direct_base = bases.direct;
            state.relations.synergetic_base = bases.synergetic;
            if (!bases.synergetic) {
                state.search.game_base = game_base;
                state.search.roster_base = state.roster_base;
                state.phase = AutoAffinityPhase::SearchRelations;
            } else {
                state.phase = AutoAffinityPhase::ScanRelations;
            }
        }
        break;
    case AutoAffinityPhase::SearchRelations:
        search_affinity_synergetic_table(state.search, 128, work_budget);
        if (state.search.complete) {
            state.relations.synergetic_base = state.search.result;
            if (state.relations.direct_base || state.relations.synergetic_base) {
                state.phase = AutoAffinityPhase::ScanRelations;
            } else {
                state.report = AutoAffinityReport{AutoAffinityOutcome::NoRelationTable, 0, 0, 0};
                state.phase = AutoAffinityPhase::Idle;
            }
        }
        break;
    case AutoAffinityPhase::ScanRelations:
        collect_affinity_relations(state.relations, 128, work_budget);
        if (state.relations.complete) {
            state.phase = AutoAffinityPhase::Apply;
        }
        break;
    case AutoAffinityPhase::Apply:
        for (std::size_t i = 0; i < 16 && !work_budget.exhausted(); ++i) {
            const std::optional<AffinityPair> pair = next_affinity_pair(state.candidates, state.relations,
                state.pair_cursor, 32, work_budget);
            if (!pair) {
                if (state.pair_cursor.left >= state.candidates.candidates.size()) {
                    state.report = AutoAffinityReport{AutoAffinityOutcome::Completed,
                                                      state.applied, state.stale, state.failed};
                    state.phase = AutoAffinityPhase::Idle;
                }
                break;
            }
            const auto result = apply_affinity_pair(data_center, *pair);
            if (result == AffinityApplyResult::Applied) {
                ++state.applied;
            } else if (result == AffinityApplyResult::Stale) {
                ++state.stale;
            } else if (result == AffinityApplyResult::Failed) {
                ++state.failed;
            }
        }
        break;
    }
}

} // namespace s8::game
