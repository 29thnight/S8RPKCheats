#pragma once

#include <array>

namespace s8::domain {

struct DongtoSettings {
    std::array<int, 3> probability{100, 100, 100};
    std::array<int, 3> status_probability{20, 50, 100};
};

} // namespace s8::domain
