#pragma once

#include <array>

namespace s8::domain {

struct SelfHealSettings {
    std::array<bool, 3> self{true, true, true};
    std::array<int, 3> amount{2000, 3500, 7000};
    std::array<int, 2> range{9, 11};
};

} // namespace s8::domain
