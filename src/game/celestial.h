#pragma once

#include "../domain/celestial.h"
#include <cstdint>

namespace s8::game {

bool set_celestial(std::uintptr_t module_base, bool enabled, const domain::CelestialSettings& settings);

} // namespace s8::game
