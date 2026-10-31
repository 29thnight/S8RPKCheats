#include "skill_condition.h"

#include "../platform/windows/process_memory.h"

#include <array>
#include <span>

namespace s8::game {

bool set_skill_condition(std::uintptr_t module_base, SkillConditionKind kind, bool enabled) {
    if (!module_base) {
        return false;
    }
    auto root = module_base + 0x034c8630;
    constexpr std::array<std::uintptr_t, 5> CHAIN{0, 0, 8, 0x10, 0};
    for (const auto offset : CHAIN) {
        std::uintptr_t next = 0;
        if (!platform::windows::read_memory(root + offset, std::as_writable_bytes(std::span(&next, 1))) || !next) {
            return false;
        }
        root = next;
    }
    constexpr std::array<std::uintptr_t, 3> OFFSETS{0x4bc732, 0x4bc7da, 0x4bc95e};
    constexpr std::array<std::int8_t, 3> DEFAULTS{2, 2, 1};
    const auto index = static_cast<std::size_t>(kind);
    const std::int8_t value = enabled ? 0 : DEFAULTS[index];
    return platform::windows::write_data_memory(root + OFFSETS[index], std::as_bytes(std::span(&value, 1)));
}

} // namespace s8::game
