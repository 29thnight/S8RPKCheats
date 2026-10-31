#pragma once

#include "../domain/self_heal.h"
#include <cstdint>

namespace s8::game {

bool set_self_heal(std::uintptr_t module_base, bool enabled, const domain::SelfHealSettings& settings);

} // namespace s8::game
