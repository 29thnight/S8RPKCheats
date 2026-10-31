#pragma once

#include <cstdint>

namespace s8::game {

enum class SkillConditionKind { Manbyeong, Yumok, Sangbyeong };

bool set_skill_condition(std::uintptr_t module_base, SkillConditionKind kind, bool enabled);

} // namespace s8::game
