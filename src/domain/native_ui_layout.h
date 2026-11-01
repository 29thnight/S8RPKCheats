#pragma once

#include <array>

namespace s8::domain::native_ui {

// Logical pixel bounds in the 1024x2048 controls atlas assembled at startup.
struct Rect {
    unsigned left, top, right, bottom;
};

struct JewelSprite { Rect source, atlas; };
inline constexpr std::array JEWEL_FRAMES{
    JewelSprite{{797, 396, 893, 492}, {0, 1280, 96, 1376}},
    JewelSprite{{601, 396, 697, 492}, {96, 1280, 192, 1376}},
    JewelSprite{{699, 396, 795, 492}, {192, 1280, 288, 1376}},
    JewelSprite{{1628, 390, 1724, 486}, {288, 1280, 384, 1376}},
    JewelSprite{{1938, 392, 2034, 488}, {384, 1280, 480, 1376}},
    JewelSprite{{1938, 294, 2034, 390}, {480, 1280, 576, 1376}},
    JewelSprite{{1938, 196, 2034, 292}, {576, 1280, 672, 1376}},
    JewelSprite{{1628, 292, 1724, 388}, {672, 1280, 768, 1376}},
    JewelSprite{{1938, 0, 2034, 96}, {768, 1280, 864, 1376}},
    JewelSprite{{1938, 98, 2034, 194}, {864, 1280, 960, 1376}},
};
inline constexpr std::array JEWEL_GEMS{
    JewelSprite{{1581, 430, 1621, 470}, {0, 1380, 40, 1420}},
    JewelSprite{{1539, 430, 1579, 470}, {44, 1380, 84, 1420}},
    JewelSprite{{1497, 430, 1537, 470}, {88, 1380, 128, 1420}},
    JewelSprite{{1097, 496, 1137, 536}, {132, 1380, 172, 1420}},
    JewelSprite{{1055, 496, 1095, 536}, {176, 1380, 216, 1420}},
    JewelSprite{{344, 446, 384, 486}, {220, 1380, 260, 1420}},
    JewelSprite{{1581, 548, 1621, 588}, {264, 1380, 304, 1420}},
    JewelSprite{{1539, 548, 1579, 588}, {308, 1380, 348, 1420}},
    JewelSprite{{1497, 548, 1537, 588}, {352, 1380, 392, 1420}},
    JewelSprite{{1405, 550, 1445, 590}, {396, 1380, 436, 1420}},
    JewelSprite{{1363, 550, 1403, 590}, {440, 1380, 480, 1420}},
    JewelSprite{{1623, 548, 1663, 588}, {484, 1380, 524, 1420}},
    JewelSprite{{42, 551, 82, 591}, {528, 1380, 568, 1420}},
    JewelSprite{{0, 551, 40, 591}, {572, 1380, 612, 1420}},
    JewelSprite{{1447, 550, 1487, 590}, {616, 1380, 656, 1420}},
    JewelSprite{{676, 554, 716, 594}, {660, 1380, 700, 1420}},
    JewelSprite{{126, 551, 166, 591}, {704, 1380, 744, 1420}},
    JewelSprite{{84, 551, 124, 591}, {748, 1380, 788, 1420}},
    JewelSprite{{802, 554, 842, 594}, {792, 1380, 832, 1420}},
    JewelSprite{{760, 554, 800, 594}, {836, 1380, 876, 1420}},
    JewelSprite{{718, 554, 758, 594}, {880, 1380, 920, 1420}},
    JewelSprite{{928, 554, 968, 594}, {924, 1380, 964, 1420}},
    JewelSprite{{886, 554, 926, 594}, {0, 1424, 40, 1464}},
    JewelSprite{{844, 554, 884, 594}, {44, 1424, 84, 1464}},
    JewelSprite{{1665, 554, 1705, 594}, {88, 1424, 128, 1464}},
    JewelSprite{{1012, 554, 1052, 594}, {132, 1424, 172, 1464}},
    JewelSprite{{970, 554, 1010, 594}, {176, 1424, 216, 1464}},
    JewelSprite{{1791, 554, 1831, 594}, {220, 1424, 260, 1464}},
    JewelSprite{{1749, 554, 1789, 594}, {264, 1424, 304, 1464}},
    JewelSprite{{1707, 554, 1747, 594}, {308, 1424, 348, 1464}},
    JewelSprite{{1917, 554, 1957, 594}, {352, 1424, 392, 1464}},
    JewelSprite{{1875, 554, 1915, 594}, {396, 1424, 436, 1464}},
    JewelSprite{{1833, 554, 1873, 594}, {440, 1424, 480, 1464}},
};
inline constexpr std::array JEWEL_CATEGORY_BUTTONS{
    JewelSprite{{172, 404, 342, 462}, {0, 1480, 170, 1538}},
    JewelSprite{{1726, 434, 1896, 492}, {180, 1480, 350, 1538}},
    JewelSprite{{172, 464, 342, 522}, {360, 1480, 530, 1538}},
};
inline constexpr JewelSprite JEWEL_START{{344, 404, 384, 444}, {960, 1280, 1000, 1320}};
inline constexpr JewelSprite JEWEL_BUTTON_HOVER{{1028, 0, 1412, 144}, {0, 1550, 384, 1694}};
// Remaining jewel art lives in paper atlas slot 2, to the right of the original panel.
inline constexpr Rect JEWEL_BACKDROP{2048, 1536, 2824, 1992};
inline constexpr Rect MAP{0, 960, 340, 1185};
inline constexpr Rect NAMEPLATE{350, 960, 566, 1008};
inline constexpr Rect TITLE{0, 512, 413, 584};
inline constexpr Rect EDIT{420, 512, 686, 570};
inline constexpr Rect CARD{0, 592, 80, 682};
inline constexpr Rect LARGE_BUTTON{88, 592, 360, 692};
inline constexpr Rect SELECTION{368, 592, 538, 646};
inline constexpr Rect SETTINGS{544, 592, 810, 650};
inline constexpr Rect CITY{544, 656, 810, 714};
inline constexpr Rect NOTICE{0, 760, 853, 800};
inline constexpr Rect OFFICER{0, 832, 266, 890};

} // namespace s8::domain::native_ui
