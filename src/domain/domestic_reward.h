#pragma once

#include <cstdint>

namespace s8::domain {

enum class DomesticRewardMode : std::int32_t {
    Off = 0,
    Achievement200Top3 = 1,
    Achievement200AnyRank = 2,
};

} // namespace s8::domain
