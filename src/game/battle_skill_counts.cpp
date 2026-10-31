#include "battle_skill_counts.h"
#include "../platform/windows/process_memory.h"

#include <algorithm>
#include <array>
#include <span>

namespace s8::game {
namespace {

constexpr std::size_t MAX_UNITS = 60;
constexpr std::size_t SKILLS_PER_UNIT = 45;
constexpr std::size_t ITEMS_PER_UPDATE = 64;
constexpr std::array<std::uintptr_t, 3> OFFICER_OFFSETS{8, 0x10, 0x18};

bool admit_item(const domain::WorkBudget& budget, std::size_t items) {
    const bool bounded = budget.deadline != (std::chrono::steady_clock::time_point::max)();

    return !budget.exhausted() && (!bounded || items < ITEMS_PER_UPDATE);
}

void advance_unit(BattleSkillCounts& state) {
    ++state.unit_cursor;
    state.skill_cursor = 0;
    state.unit_identity = 0;
    state.skill_identity = 0;
    state.officer_id = 0;
}

} // namespace

void update_battle_skill_counts(BattleSkillCounts& state, const BattleObservation& observation,
                                const domain::SkillCounts& counts, const domain::WorkBudget& work_budget) {
    const std::size_t unit_count = std::min<std::size_t>(observation.unit_count, MAX_UNITS);
    const bool identity_changed = state.unit_list != observation.unit_list ||
        state.day_base != observation.day_base;

    if (identity_changed) {
        state = {};
        state.unit_list = observation.unit_list;
        state.day_base = observation.day_base;
        state.unit_count = unit_count;
    }
    if (!state.pending) {
        const bool apply = !state.initialized || (observation.day == 1 && state.last_day != 1);

        if (!apply) {
            state.last_day = observation.day;
            return;
        }
        state.pending = true;
        state.pending_day = observation.day;
        state.unit_count = unit_count;
        state.unit_cursor = 0;
        state.skill_cursor = 0;
    }
    // Reinforcements or removed units do not replenish already consumed counts.
    // Only narrow a pass that is already in progress; the next day-1 pass takes
    // a new roster size without restarting completed units from this pass.
    state.unit_count = std::min(state.unit_count, unit_count);

    std::size_t items = 0;

    while (state.unit_cursor < state.unit_count) {
        if (!admit_item(work_budget, items)) {
            return;
        }
        ++items;

        std::uintptr_t unit = 0;
        std::uintptr_t members = 0;

        if (!platform::windows::read_memory(observation.unit_list + 8 + state.unit_cursor * 0x10,
                std::as_writable_bytes(std::span(&unit, 1))) ||
            !platform::windows::is_committed_memory(unit, 0x600) ||
            !platform::windows::read_memory(unit + 0x18, std::as_writable_bytes(std::span(&members, 1))) ||
            !platform::windows::is_committed_memory(members, 0x100)) {
            advance_unit(state);
            continue;
        }

        std::uint16_t officer_id = 0;

        for (const std::uintptr_t offset : OFFICER_OFFSETS) {
            std::uintptr_t officer = 0;

            if (platform::windows::read_memory(members + offset, std::as_writable_bytes(std::span(&officer, 1))) &&
                platform::windows::is_committed_memory(officer, 0x10) &&
                platform::windows::read_memory(officer + 8, std::as_writable_bytes(std::span(&officer_id, 1)))) {
                break;
            }
        }

        const domain::SkillCounts::const_iterator officer = counts.find(officer_id);
        std::uintptr_t skill_start = 0;

        if (!officer_id || officer == counts.end() ||
            !platform::windows::read_memory(unit + 0x5d8, std::as_writable_bytes(std::span(&skill_start, 1))) ||
            !platform::windows::is_committed_memory(skill_start, 0x28)) {
            advance_unit(state);
            continue;
        }
        if (state.unit_identity != unit || state.skill_identity != skill_start || state.officer_id != officer_id) {
            state.unit_identity = unit;
            state.skill_identity = skill_start;
            state.officer_id = officer_id;
            state.skill_cursor = 0;
        }

        while (state.skill_cursor < SKILLS_PER_UNIT) {
            if (!admit_item(work_budget, items)) {
                return;
            }
            ++items;

            const std::uintptr_t record = skill_start + state.skill_cursor * 0x28;
            std::uintptr_t core = 0;
            std::int32_t record_index = 0;

            if (!platform::windows::is_committed_memory(record, 0x20) ||
                !platform::windows::read_memory(record, std::as_writable_bytes(std::span(&core, 1))) ||
                !platform::windows::read_memory(record + 0x0c, std::as_writable_bytes(std::span(&record_index, 1))) ||
                record_index != static_cast<std::int32_t>(state.skill_cursor) ||
                ((core & 0xff) != 0x58 && (core & 0xff) != 0xd8)) {
                break;
            }

            std::uint8_t skill_id = 0;

            if (platform::windows::is_committed_memory(core, 0x10) &&
                platform::windows::read_memory(core + 8, std::as_writable_bytes(std::span(&skill_id, 1))) && skill_id) {
                using SkillSettings = domain::SkillCounts::mapped_type;
                const SkillSettings::const_iterator setting = officer->second.find(
                    static_cast<std::uintptr_t>(skill_id) + 0x138);

                if (setting != officer->second.end() && setting->second > 0) {
                    const std::uint8_t value = static_cast<std::uint8_t>(setting->second);

                    // Counts are consumable. A failed API result may still have
                    // written the value, so never replenish this record again
                    // on a later tick after the game could have consumed it.
                    platform::windows::write_data_memory(record + 0x10, std::as_bytes(std::span(&value, 1)));
                }
            }
            ++state.skill_cursor;
        }
        advance_unit(state);
    }
    state.pending = false;
    state.initialized = true;
    state.last_day = state.pending_day;
}

} // namespace s8::game
