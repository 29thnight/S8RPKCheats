#pragma once

#include <array>

namespace s8::domain {

struct TerrainTacticsSettings {
    std::array<int, 3> water_amount{60, 70, 85};
    std::array<int, 3> water_range{5, 5, 5};
    std::array<int, 3> stone_amount{60, 70, 85};
    std::array<int, 3> stone_range{4, 4, 4};
};

} // namespace s8::domain
