#pragma once

#include <cstdint>
#include <unordered_map>

namespace s8::domain {

using SkillCounts = std::unordered_map<int, std::unordered_map<std::uintptr_t, int>>;

} // namespace s8::domain
