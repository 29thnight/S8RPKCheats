#pragma once

#include "../domain/catapult.h"
#include <cstdint>

namespace s8::game {

bool set_catapult(std::uintptr_t module_base, bool enabled, const domain::CatapultSettings& settings);

} // namespace s8::game
