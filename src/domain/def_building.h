#pragma once

#include <array>
#include <string_view>

namespace s8::domain {

inline constexpr std::array<std::string_view, 19> DEF_BUILDING_KEYS{
    "v_City_Dur", "v_City_Range", "v_City_Atk", "v_City_Sight",
    "v_Gate_Dur", "v_Gate_Range", "v_Gate_Atk", "v_Gate_Sight",
    "v_Tower_Dur", "v_Tower_Range", "v_Tower_Atk", "v_Tower_Sight",
    "v_WallCatapult_Dur", "v_WallCatapult_Range", "v_WallCatapult_Atk", "v_WallCatapult_Sight",
    "v_Signal_Dur", "v_Signal_Spirit", "v_Signal_Sight"};

struct DefBuildingSettings {
    std::array<int, 19> values{6000, 4, 16, 6, 4000, 4, 16, 4, 1400, 4, 12, 4,
                               2000, 5, 20, 5, 1400, 10, 4};
};

} // namespace s8::domain
