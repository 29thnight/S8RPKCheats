#pragma once

#include "../domain/dongto.h"
#include <cstdint>

namespace s8::game {

bool set_dongto(std::uintptr_t module_base, bool enabled, const domain::DongtoSettings& settings);

} // namespace s8::game
