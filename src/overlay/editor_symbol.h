#pragma once

#include <array>

namespace s8::overlay::editor_symbol {

// Editor-only brush silhouette in a 56 x 56 native HUD icon canvas.
struct Point { float x; float y; };
inline constexpr std::array<Point, 4> HANDLE{{{24, 29}, {37, 14}, {42, 19}, {29, 33}}};
inline constexpr std::array<Point, 4> FERRULE{{{20, 30}, {24, 26}, {32, 33}, {28, 37}}};
inline constexpr std::array<Point, 5> TIP{{{20, 32}, {26, 38}, {23, 43}, {13, 44}, {17, 39}}};

} // namespace s8::overlay::editor_symbol
