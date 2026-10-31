#include "ship_weaponization.h"
#include "tactics_data.h"
#include "../platform/windows/process_memory.h"

#include <array>
#include <span>

namespace s8::game {

void update_ship_weaponization(ShipWeaponization& state, std::uintptr_t module_base,
                               const domain::SkillCounts& skill_counts, std::uint64_t now_ms) {
    if (now_ms - state.last_update_ms < 300) {
        return;
    }
    state.last_update_ms = now_ms;
    const auto tactics = resolve_tactics_data(module_base);
    if (!tactics) {
        return;
    }
    auto active = module_base + 0x03510578;
    for (const auto offset : std::array<std::uintptr_t, 8>{0x100, 0x78, 0, 0x50, 0x108, 0x50, 8, 0}) {
        std::uintptr_t next = 0;
        if (!platform::windows::read_memory(active, std::as_writable_bytes(std::span(&next, 1))) || !next) {
            return;
        }
        active = next + offset;
    }
    std::uintptr_t terrain_owner = 0;
    std::uintptr_t terrain_data = 0;
    std::uint8_t terrain = 0;
    if (!platform::windows::read_memory(active + 0x40, std::as_writable_bytes(std::span(&terrain_owner, 1))) ||
        !terrain_owner || !platform::windows::read_memory(terrain_owner + 0x10,
            std::as_writable_bytes(std::span(&terrain_data, 1))) || !terrain_data ||
        !platform::windows::read_memory(terrain_data + 8, std::as_writable_bytes(std::span(&terrain, 1)))) {
        return;
    }
    if (terrain == 11 || terrain == 12) {
        state.last_active_unit = active;
        state.last_triggered = false;
    } else if (active != state.last_active_unit) {
        state.last_active_unit = active;
        state.last_triggered = false;
        std::uintptr_t members = 0;
        if (platform::windows::read_memory(active + 0x18, std::as_writable_bytes(std::span(&members, 1))) && members) {
            for (const auto offset : std::array<std::uintptr_t, 3>{8, 0x10, 0x18}) {
                std::uintptr_t officer = 0;
                std::uint16_t id = 0;
                if (platform::windows::read_memory(members + offset, std::as_writable_bytes(std::span(&officer, 1))) &&
                    officer && platform::windows::read_memory(officer + 8, std::as_writable_bytes(std::span(&id, 1))) && id) {
                    const auto found = skill_counts.find(id);
                    if (found != skill_counts.end()) {
                        const auto skill = found->second.find(0x1009);
                        if (skill != found->second.end() && skill->second > 0) {
                            state.last_triggered = true;
                            break;
                        }
                    }
                }
            }
        }
    }
    const std::uint8_t value = state.last_triggered ? 8 : 4;
    for (const auto offset : std::array<std::uintptr_t, 5>{0x9a4, 0xa24, 0xaa4, 0xb24, 0xba4}) {
        std::uint8_t current = 0;
        if (platform::windows::read_memory(tactics + offset, std::as_writable_bytes(std::span(&current, 1))) &&
            current != value) {
            platform::windows::write_data_memory(tactics + offset, std::as_bytes(std::span(&value, 1)));
        }
    }
}

} // namespace s8::game
