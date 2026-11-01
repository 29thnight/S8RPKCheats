#pragma once

#include "../services/game_font.h"
#include "../../third_party/imgui_legacy/imgui.h"
#include <cstring>
#include <vector>

namespace s8::overlay {
inline void load_game_font(float size_pixels) {
    const own::shared_owner<const services::GameFont> source = services::game_font_view();

    if (source->glyphs.empty()) {
        return;
    }

    ImFontAtlas& atlas = *ImGui::GetIO().Fonts;
    ImFontConfig base_config;

    base_config.SizePixels = size_pixels;
    ImFont* font = atlas.Fonts.empty() ? atlas.AddFontDefault(&base_config) : atlas.Fonts[0];
    const float scale = size_pixels / 64.0f;
    std::vector<int> rectangles;

    rectangles.reserve(source->glyphs.size());
    for (const services::GameFontGlyph& glyph : source->glyphs) {
        rectangles.push_back(atlas.AddCustomRectFontGlyph(font, static_cast<ImWchar>(glyph.codepoint),
            glyph.bitmap_width, glyph.bitmap_height, glyph.advance * scale,
            ImVec2(glyph.bearing_x * scale, glyph.top * scale)));
    }

    unsigned char* pixels = nullptr;
    int atlas_width = 0;
    int atlas_height = 0;

    atlas.GetTexDataAsAlpha8(&pixels, &atlas_width, &atlas_height);
    for (std::size_t index = 0; index < source->glyphs.size(); ++index) {
        const services::GameFontGlyph& glyph = source->glyphs[index];
        const ImFontAtlasCustomRect& rectangle = *atlas.GetCustomRectByIndex(rectangles[index]);

        for (unsigned row = 0; row < glyph.bitmap_height; ++row) {
            std::memcpy(pixels + (rectangle.Y + row) * atlas_width + rectangle.X,
                source->pixels.data() + glyph.pixels_offset + row * glyph.bitmap_width, glyph.bitmap_width);
        }

        ImFontGlyph& registered = *const_cast<ImFontGlyph*>(font->FindGlyphNoFallback(static_cast<ImWchar>(glyph.codepoint)));

        registered.X1 = registered.X0 + glyph.width * scale;
        registered.Y1 = registered.Y0 + glyph.height * scale;
    }
}

} // namespace s8::overlay
