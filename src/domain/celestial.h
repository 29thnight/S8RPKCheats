#pragma once

#include <array>

namespace s8::domain {

struct CelestialSettings {
    std::array<int, 3> probability{100, 100, 100};
    std::array<int, 3> amount{2000, 3500, 7000};
    std::array<int, 3> range{11, 11, 11};
};

} // namespace s8::domain
