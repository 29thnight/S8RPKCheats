#pragma once

#include "../domain/def_building.h"
#include <cstdint>

namespace s8::game {

bool set_def_building(std::uintptr_t module_base, bool enabled, const domain::DefBuildingSettings& settings);

} // namespace s8::game
