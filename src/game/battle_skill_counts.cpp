#include "battle_skill_counts.h"
#include "../platform/windows/process_memory.h"

#include <array>
#include <span>

namespace s8::game {

void update_battle_skill_counts(BattleSkillCounts& state, const BattleObservation& observation,
                                const domain::SkillCounts& counts) {
    const bool apply = !state.initialized || (observation.day == 1 && state.last_day != 1);
    state.initialized = true;
    state.last_day = observation.day;
    if (!apply) {
        return;
    }
    for (std::size_t i = 0; i < observation.unit_count; ++i) {
        std::uintptr_t unit = 0;
        std::uintptr_t members = 0;
        if (!platform::windows::read_memory(observation.unit_list + 8 + i * 0x10,
                std::as_writable_bytes(std::span(&unit, 1))) ||
            !platform::windows::is_committed_memory(unit, 0x600) ||
            !platform::windows::read_memory(unit + 0x18, std::as_writable_bytes(std::span(&members, 1))) ||
            !platform::windows::is_committed_memory(members, 0x100)) {
            continue;
        }
        std::uint16_t officer_id = 0;
        for (const auto offset : std::array<std::uintptr_t, 3>{8, 0x10, 0x18}) {
            std::uintptr_t officer = 0;
            if (platform::windows::read_memory(members + offset, std::as_writable_bytes(std::span(&officer, 1))) &&
                platform::windows::is_committed_memory(officer, 0x10) &&
                platform::windows::read_memory(officer + 8, std::as_writable_bytes(std::span(&officer_id, 1)))) {
                break;
            }
        }
        if (!officer_id) {
            continue;
        }
        const auto officer = counts.find(officer_id);
        if (officer == counts.end()) {
            continue;
        }
        std::uintptr_t skill_start = 0;
        if (!platform::windows::read_memory(unit + 0x5d8, std::as_writable_bytes(std::span(&skill_start, 1))) ||
            !platform::windows::is_committed_memory(skill_start, 0x28)) {
            continue;
        }
        for (int index = 0; index < 45; ++index) {
            const auto record = skill_start + static_cast<std::uintptr_t>(index) * 0x28;
            std::uintptr_t core = 0;
            std::int32_t record_index = 0;
            if (!platform::windows::is_committed_memory(record, 0x20) ||
                !platform::windows::read_memory(record, std::as_writable_bytes(std::span(&core, 1))) ||
                !platform::windows::read_memory(record + 0x0c, std::as_writable_bytes(std::span(&record_index, 1))) ||
                record_index != index || ((core & 0xff) != 0x58 && (core & 0xff) != 0xd8)) {
                break;
            }
            std::uint8_t skill_id = 0;
            if (!platform::windows::is_committed_memory(core, 0x10) ||
                !platform::windows::read_memory(core + 8, std::as_writable_bytes(std::span(&skill_id, 1))) || !skill_id) {
                continue;
            }
            const auto setting = officer->second.find(static_cast<std::uintptr_t>(skill_id) + 0x138);
            if (setting != officer->second.end() && setting->second > 0) {
                const auto value = static_cast<std::uint8_t>(setting->second);
                platform::windows::write_data_memory(record + 0x10, std::as_bytes(std::span(&value, 1)));
            }
        }
    }
}

} // namespace s8::game
