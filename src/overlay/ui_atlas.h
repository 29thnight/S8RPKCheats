#pragma once

#include <array>
#include <cstdint>

namespace s8::overlay {

// Borrowed renderer handles. The UI only supplies UVs and never owns a texture.
// Atlas order: menu_controls, status_badges, panel_frame.
struct UiAtlasTexture {
    void* texture = nullptr;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

using UiAtlasTextures = std::array<UiAtlasTexture, 3>;

} // namespace s8::overlay
