#pragma once

#include "../domain/terrain_tactics.h"
#include <cstdint>

namespace s8::game {

// Shared condition fields are resolved here. Terrain override takes precedence over weather.
bool write_terrain_tactic_conditions(std::uintptr_t tactics, bool terrain_override,
                                     std::uint8_t water_condition, std::uint8_t stone_condition);
bool set_terrain_tactics(std::uintptr_t module_base, bool enabled, const domain::TerrainTacticsSettings& settings);

} // namespace s8::game
