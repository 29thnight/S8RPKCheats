#pragma once

#include <cstdint>

namespace s8::game {

struct TechCityVisibility {
    bool applied = false;
};

bool set_tech_city_visibility(TechCityVisibility& state, std::uintptr_t module_base, bool enabled);

} // namespace s8::game
