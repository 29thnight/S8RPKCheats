#pragma once

#include "../domain/terrain_tactics.h"
#include <cstdint>

namespace s8::game {

bool set_terrain_tactics(std::uintptr_t module_base, bool enabled, const domain::TerrainTacticsSettings& settings);

} // namespace s8::game
