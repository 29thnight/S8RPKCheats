#include "special_ability_assignment.h"

#include "officer_roster.h"
#include "../platform/windows/process_memory.h"

#include <algorithm>
#include <cstring>
#include <utility>

namespace s8::game {
namespace {

constexpr std::size_t SCAN_BUDGET = 32;
constexpr std::size_t COMMIT_BUDGET = 8;
constexpr std::size_t OFFICER_STRIDE = 0x3d0;

bool running(const SpecialAbilityAssignment& state) {

    return state.summary.phase == domain::SpecialAssignmentPhase::Scanning ||
        state.summary.phase == domain::SpecialAssignmentPhase::Committing;
}

bool existing_special_ability(const domain::SkillCounts& counts, std::uint16_t id) {

    const domain::SkillCounts::const_iterator officer = counts.find(id);

    if (officer == counts.end()) {
        return false;
    }

    for (std::size_t index = 0; index < domain::SPECIAL_ABILITY_COUNT; ++index) {

        const std::unordered_map<std::uintptr_t, int>::const_iterator value = officer->second.find(0x1000 + index);

        if (value != officer->second.end() && value->second > 0) {
            return true;
        }
    }

    return false;
}

domain::SpecialAbilityProfile read_profile(const std::array<std::byte, OFFICER_STRIDE>& record) {

    domain::SpecialAbilityProfile profile{};

    std::memcpy(&profile.officer_id, record.data() + 8, sizeof(profile.officer_id));
    std::memcpy(profile.stats.data(), record.data() + 0xaa, profile.stats.size());
    std::memcpy(profile.branch_skills.data(), record.data() + 0x1dc, profile.branch_skills.size());
    std::memcpy(profile.military_skills.data(), record.data() + 0x1e2, profile.military_skills.size());

    for (std::size_t category = 0; category < profile.tactics.size(); ++category) {
        std::memcpy(profile.tactics[category].data(), record.data() + 0x139 + category * 5, 5);
    }

    return profile;
}

void scan_next_officers(SpecialAbilityAssignment& state) {

    const std::size_t end = std::min(domain::SPECIAL_ASSIGNMENT_OFFICER_LIMIT,
                                    static_cast<std::size_t>(state.summary.scanned) + SCAN_BUDGET);

    while (state.summary.scanned < end) {

        const std::uintptr_t address = state.roster + state.summary.scanned * OFFICER_STRIDE;

        ++state.summary.scanned;

        if (!platform::windows::read_memory(address, state.record)) {
            continue;
        }

        const domain::SpecialAbilityProfile profile = read_profile(state.record);
        const std::uint16_t id = profile.officer_id;

        if (id == 0 || id > domain::SPECIAL_ASSIGNMENT_OFFICER_LIMIT || state.seen[id]) {
            continue;
        }

        state.seen.set(id);

        if (state.summary.annual && existing_special_ability(*state.existing, id)) {
            ++state.summary.excluded_existing;
            continue;
        }

        ++state.summary.valid;

        const std::uint16_t matches = domain::evaluate_special_abilities(profile);

        if (matches == 0) {
            continue;
        }

        ++state.summary.matched_officers;
        state.pending[state.pending_count++] = {id, matches};

        for (std::size_t index = 0; index < domain::SPECIAL_ABILITY_COUNT; ++index) {

            if ((matches & (1u << index)) != 0) {
                ++state.summary.matched_counts[index];
            }
        }
    }

    if (state.summary.scanned == domain::SPECIAL_ASSIGNMENT_OFFICER_LIMIT) {
        state.summary.phase = domain::SpecialAssignmentPhase::Committing;
    }
}

void commit_next_assignments(SpecialAbilityAssignment& state, domain::SkillCounts& counts) {

    const std::size_t end = std::min(state.pending_count, state.commit_cursor + COMMIT_BUDGET);

    while (state.commit_cursor < end) {

        const domain::SpecialAssignmentResult pending = state.pending[state.commit_cursor++];
        std::unordered_map<std::uintptr_t, int>& skills = counts[pending.officer_id];
        std::uint16_t assigned = 0;

        ++state.summary.committed_officers;

        for (std::size_t index = 0; index < domain::SPECIAL_ABILITY_COUNT; ++index) {

            if ((pending.abilities & (1u << index)) == 0) {
                continue;
            }

            const std::uintptr_t key = 0x1000 + index;
            const std::unordered_map<std::uintptr_t, int>::iterator existing = skills.find(key);

            if (existing != skills.end() && existing->second > 0) {
                continue;
            }

            skills[key] = 1;
            assigned |= static_cast<std::uint16_t>(1u << index);
            ++state.summary.assigned;
            ++state.summary.assigned_counts[index];
        }

        if (assigned != 0) {
            state.pending[state.result_count++] = {pending.officer_id, assigned};
            ++state.summary.assigned_officers;
        }
    }

    if (state.commit_cursor == state.pending_count) {
        state.summary.phase = domain::SpecialAssignmentPhase::Completed;
        state.existing.reset();
    }
}

} // namespace

bool start_special_ability_assignment(SpecialAbilityAssignment& state, std::uintptr_t module_base,
                                       std::uintptr_t game_base, std::uintptr_t player,
                                       domain::GameEpoch epoch,
                                       own::shared_owner<const domain::SkillCounts> existing,
                                       bool annual) {

    if (running(state) || !existing || !module_base || game_base <= 0x10000 || player <= 0x10000) {
        return false;
    }

    const std::uintptr_t roster = resolve_officer_roster_base(module_base);

    if (roster <= 0x10000) {
        return false;
    }

    state.summary = {};
    state.summary.phase = domain::SpecialAssignmentPhase::Scanning;
    state.summary.annual = annual;
    state.module_base = module_base;
    state.game_base = game_base;
    state.player = player;
    state.roster = roster;
    state.epoch = epoch;
    state.pending_count = 0;
    state.commit_cursor = 0;
    state.result_count = 0;
    state.seen.reset();
    state.existing = std::move(existing);
    return true;
}

bool cancel_special_ability_assignment(SpecialAbilityAssignment& state) {

    if (state.summary.phase != domain::SpecialAssignmentPhase::Scanning) {
        return false;
    }

    state.summary.phase = domain::SpecialAssignmentPhase::Cancelled;
    state.pending_count = 0;
    state.result_count = 0;
    state.existing.reset();
    return true;
}

bool update_special_ability_assignment(SpecialAbilityAssignment& state, std::uintptr_t module_base,
                                        std::uintptr_t game_base, std::uintptr_t player,
                                        domain::GameEpoch epoch, domain::SkillCounts& counts) {

    if (!running(state)) {
        return false;
    }

    if (state.module_base != module_base || state.game_base != game_base || state.player != player ||
        state.epoch != epoch || resolve_officer_roster_base(module_base) != state.roster) {
        state.summary.phase = domain::SpecialAssignmentPhase::SessionChanged;
        state.existing.reset();
        return true;
    }

    if (state.summary.annual) {

        std::uint8_t phase = 0;

        if (!platform::windows::read_memory(game_base + 0xd0, std::as_writable_bytes(std::span(&phase, 1))) ||
            (phase != 5 && phase != 7)) {
            return false;
        }
    }

    if (state.summary.phase == domain::SpecialAssignmentPhase::Scanning) {
        scan_next_officers(state);
        return false;
    }

    commit_next_assignments(state, counts);
    return state.summary.phase == domain::SpecialAssignmentPhase::Completed;
}

std::span<const domain::SpecialAssignmentResult> special_ability_assignment_results(
    const SpecialAbilityAssignment& state) {

    return std::span(state.pending).first(state.result_count);
}

} // namespace s8::game
