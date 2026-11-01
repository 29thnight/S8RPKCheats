#pragma once

#include "../domain/ownership.h"

#include <cstddef>
#include <filesystem>
#include <cstdint>
#include <string>
#include <vector>

namespace s8::services {

class UiBridge;
struct RangeImage;

struct GamePortrait {
    unsigned face;
    own::shared_owner<const RangeImage> image;
};
bool request_game_portrait(unsigned face);
void poll_game_portrait();
own::shared_owner<const GamePortrait> game_portrait_view();
struct GameFontGlyph {
    std::uint32_t codepoint;
    std::uint16_t bitmap_width;
    std::uint16_t bitmap_height;
    float width;
    float height;
    float advance;
    float bearing_x;
    float top;
    std::size_t pixels_offset;
};

struct GameFont {
    std::vector<GameFontGlyph> glyphs;
    std::vector<unsigned char> pixels;
    std::string error;
};

// The service thread reads the installed game's font once. A published empty
// font carries the load error and lets the renderer use its existing font.
void prepare_game_font(const std::filesystem::path& directory = {});
own::shared_owner<const GameFont> game_font_view();
bool publish_game_ui_images(UiBridge& bridge);

} // namespace s8::services
