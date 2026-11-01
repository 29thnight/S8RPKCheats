#pragma once

#include "ui_atlas.h"
#include "../../third_party/imgui_legacy/imgui_internal.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace s8::overlay::menu_sprites {

enum class StatusTone { Good, Waiting, Error, Muted };
enum Icon : int { Overview, Officer, Trait, Scenario, City, Settings, Help, Refresh };

namespace detail {

// Pixel bounds are [left, top, right, bottom). The original PNGs stay intact.
struct SpriteRect {
    float left, top, right, bottom;
};

// menu_controls.png (original 027): normal, hover, selected, pressed, disabled.
inline constexpr std::array BUTTONS{
    SpriteRect{54, 236, 186, 284}, SpriteRect{188, 236, 320, 284},
    SpriteRect{322, 236, 454, 284}, SpriteRect{456, 286, 588, 334},
    SpriteRect{54, 286, 186, 334},
};

struct IconSprites {
    SpriteRect normal, active, disabled;
};

inline constexpr std::array ICONS{
    IconSprites{{348, 62, 404, 118}, {406, 62, 462, 118}, {522, 116, 578, 172}},
    IconSprites{{406, 120, 462, 176}, {464, 174, 520, 230}, {580, 174, 636, 230}},
    IconSprites{{696, 116, 752, 172}, {754, 116, 810, 172}, {870, 116, 926, 172}},
    IconSprites{{58, 120, 114, 176}, {116, 120, 172, 176}, {232, 120, 288, 176}},
    IconSprites{{754, 174, 810, 230}, {812, 174, 868, 230}, {928, 174, 984, 230}},
    IconSprites{{116, 178, 172, 234}, {174, 178, 230, 234}, {290, 178, 346, 234}},
    IconSprites{{0, 62, 56, 118}, {58, 62, 114, 118}, {174, 62, 230, 118}},
    IconSprites{{906, 0, 962, 56}, {964, 0, 1020, 56}, {822, 58, 878, 114}},
};

// status_badges.png (original 032), in StatusTone order.
inline constexpr std::array BADGES{
    SpriteRect{776, 241, 922, 279}, SpriteRect{774, 198, 924, 238},
    SpriteRect{480, 247, 628, 285}, SpriteRect{634, 247, 722, 285},
};
inline constexpr SpriteRect PANEL{666, 6, 1254, 444}; // panel_frame.png (original 033).

inline float positive_scale(float scale) {
    return std::isfinite(scale) && scale > 0.0f ? scale : 1.0f;
}

inline bool available(const UiAtlasTexture& atlas, SpriteRect rect) {
    return atlas.texture && atlas.width != 0 && atlas.height != 0 &&
        rect.left >= 0 && rect.top >= 0 && rect.right > rect.left && rect.bottom > rect.top &&
        rect.right <= static_cast<float>(atlas.width) && rect.bottom <= static_cast<float>(atlas.height);
}

inline void image(ImDrawList& draw, const UiAtlasTexture& atlas, SpriteRect rect,
    ImVec2 minimum, ImVec2 maximum, ImU32 tint) {
    if (!available(atlas, rect) || maximum.x <= minimum.x || maximum.y <= minimum.y) {
        return;
    }
    const float width = static_cast<float>(atlas.width);
    const float height = static_cast<float>(atlas.height);
    draw.AddImage(atlas.texture, minimum, maximum,
        ImVec2((rect.left + 0.5f) / width, (rect.top + 0.5f) / height),
        ImVec2((rect.right - 0.5f) / width, (rect.bottom - 0.5f) / height), tint);
}

inline void nine_slice(ImDrawList& draw, const UiAtlasTexture& atlas, SpriteRect rect,
    ImVec2 minimum, ImVec2 maximum, float cap_x, float cap_y, float scale, ImU32 tint) {
    if (!available(atlas, rect) || maximum.x <= minimum.x || maximum.y <= minimum.y) {
        return;
    }
    scale = positive_scale(scale);
    cap_x = std::clamp(cap_x, 0.5f, (rect.right - rect.left) * 0.5f);
    cap_y = std::clamp(cap_y, 0.5f, (rect.bottom - rect.top) * 0.5f);
    const float target_x = std::min(cap_x * scale, (maximum.x - minimum.x) * 0.5f);
    const float target_y = std::min(cap_y * scale, (maximum.y - minimum.y) * 0.5f);
    const std::array xs{minimum.x, minimum.x + target_x, maximum.x - target_x, maximum.x};
    const std::array ys{minimum.y, minimum.y + target_y, maximum.y - target_y, maximum.y};
    // Only the outer atlas edges are inset. Internal slice edges remain shared
    // so filtering cannot introduce a gap between two neighboring slices.
    const std::array us{rect.left + 0.5f, rect.left + cap_x, rect.right - cap_x, rect.right - 0.5f};
    const std::array vs{rect.top + 0.5f, rect.top + cap_y, rect.bottom - cap_y, rect.bottom - 0.5f};
    const float width = static_cast<float>(atlas.width);
    const float height = static_cast<float>(atlas.height);
    for (std::size_t y = 0; y < 3; ++y) {
        for (std::size_t x = 0; x < 3; ++x) {
            if (xs[x + 1] <= xs[x] || ys[y + 1] <= ys[y]) {
                continue;
            }
            draw.AddImage(atlas.texture, ImVec2(xs[x], ys[y]), ImVec2(xs[x + 1], ys[y + 1]),
                ImVec2(us[x] / width, vs[y] / height), ImVec2(us[x + 1] / width, vs[y + 1] / height), tint);
        }
    }
}

struct ThemeColor {
    ImGuiCol target;
    ImVec4 value;
};

inline constexpr std::array THEME_COLORS{
    ThemeColor{ImGuiCol_Text, {0.95f, 0.93f, 0.86f, 1.0f}},
    ThemeColor{ImGuiCol_TextDisabled, {0.63f, 0.61f, 0.55f, 1.0f}},
    ThemeColor{ImGuiCol_WindowBg, {0.055f, 0.052f, 0.055f, 0.99f}},
    ThemeColor{ImGuiCol_ChildBg, {0.075f, 0.070f, 0.075f, 0.98f}},
    ThemeColor{ImGuiCol_PopupBg, {0.080f, 0.075f, 0.080f, 0.99f}},
    ThemeColor{ImGuiCol_Border, {0.47f, 0.38f, 0.21f, 0.95f}},
    ThemeColor{ImGuiCol_BorderShadow, {0, 0, 0, 0.30f}},
    ThemeColor{ImGuiCol_FrameBg, {0.14f, 0.13f, 0.12f, 1.0f}},
    ThemeColor{ImGuiCol_FrameBgHovered, {0.23f, 0.20f, 0.14f, 1.0f}},
    ThemeColor{ImGuiCol_FrameBgActive, {0.31f, 0.25f, 0.14f, 1.0f}},
    ThemeColor{ImGuiCol_TitleBg, {0.09f, 0.08f, 0.06f, 1.0f}},
    ThemeColor{ImGuiCol_TitleBgActive, {0.15f, 0.12f, 0.07f, 1.0f}},
    ThemeColor{ImGuiCol_TitleBgCollapsed, {0.10f, 0.09f, 0.07f, 1.0f}},
    ThemeColor{ImGuiCol_MenuBarBg, {0.12f, 0.11f, 0.09f, 1.0f}},
    ThemeColor{ImGuiCol_ScrollbarBg, {0.065f, 0.060f, 0.065f, 1.0f}},
    ThemeColor{ImGuiCol_ScrollbarGrab, {0.34f, 0.29f, 0.19f, 1.0f}},
    ThemeColor{ImGuiCol_ScrollbarGrabHovered, {0.49f, 0.40f, 0.23f, 1.0f}},
    ThemeColor{ImGuiCol_ScrollbarGrabActive, {0.64f, 0.52f, 0.28f, 1.0f}},
    ThemeColor{ImGuiCol_CheckMark, {0.92f, 0.76f, 0.36f, 1.0f}},
    ThemeColor{ImGuiCol_SliderGrab, {0.66f, 0.53f, 0.27f, 1.0f}},
    ThemeColor{ImGuiCol_SliderGrabActive, {0.94f, 0.77f, 0.36f, 1.0f}},
    ThemeColor{ImGuiCol_Button, {0.24f, 0.20f, 0.12f, 1.0f}},
    ThemeColor{ImGuiCol_ButtonHovered, {0.74f, 0.60f, 0.32f, 1.0f}},
    ThemeColor{ImGuiCol_ButtonActive, {0.64f, 0.49f, 0.25f, 1.0f}},
    ThemeColor{ImGuiCol_Header, {0.32f, 0.27f, 0.15f, 0.90f}},
    ThemeColor{ImGuiCol_HeaderHovered, {0.54f, 0.44f, 0.23f, 1.0f}},
    ThemeColor{ImGuiCol_HeaderActive, {0.66f, 0.53f, 0.27f, 1.0f}},
    ThemeColor{ImGuiCol_Separator, {0.43f, 0.35f, 0.20f, 0.85f}},
    ThemeColor{ImGuiCol_SeparatorHovered, {0.70f, 0.57f, 0.30f, 1.0f}},
    ThemeColor{ImGuiCol_SeparatorActive, {0.90f, 0.73f, 0.35f, 1.0f}},
    ThemeColor{ImGuiCol_ResizeGrip, {0.49f, 0.40f, 0.22f, 0.45f}},
    ThemeColor{ImGuiCol_ResizeGripHovered, {0.72f, 0.58f, 0.29f, 0.80f}},
    ThemeColor{ImGuiCol_ResizeGripActive, {0.91f, 0.74f, 0.34f, 1.0f}},
    ThemeColor{ImGuiCol_Tab, {0.19f, 0.17f, 0.12f, 1.0f}},
    ThemeColor{ImGuiCol_TabHovered, {0.52f, 0.43f, 0.23f, 1.0f}},
    ThemeColor{ImGuiCol_TabSelected, {0.36f, 0.29f, 0.15f, 1.0f}},
    ThemeColor{ImGuiCol_TabSelectedOverline, {0.93f, 0.76f, 0.37f, 1.0f}},
    ThemeColor{ImGuiCol_TabDimmed, {0.12f, 0.11f, 0.09f, 1.0f}},
    ThemeColor{ImGuiCol_TabDimmedSelected, {0.25f, 0.21f, 0.14f, 1.0f}},
    ThemeColor{ImGuiCol_TabDimmedSelectedOverline, {0.60f, 0.49f, 0.27f, 1.0f}},
    ThemeColor{ImGuiCol_TableHeaderBg, {0.22f, 0.19f, 0.13f, 1.0f}},
    ThemeColor{ImGuiCol_TableBorderStrong, {0.40f, 0.33f, 0.21f, 1.0f}},
    ThemeColor{ImGuiCol_TableBorderLight, {0.25f, 0.22f, 0.17f, 1.0f}},
    ThemeColor{ImGuiCol_TableRowBg, {0, 0, 0, 0}},
    ThemeColor{ImGuiCol_TableRowBgAlt, {0.70f, 0.63f, 0.43f, 0.045f}},
    ThemeColor{ImGuiCol_TextSelectedBg, {0.72f, 0.56f, 0.25f, 0.45f}},
    ThemeColor{ImGuiCol_NavHighlight, {1.0f, 0.95f, 0.72f, 1.0f}},
};

} // namespace detail

// Scope this around all overlay windows. No global font, cursor or style state
// is retained after destruction, including when a caller returns early.
class ThemeScope {
public:
    explicit ThemeScope(float scale) {
        scale = detail::positive_scale(scale);
        for (const auto& color : detail::THEME_COLORS) {
            ImGui::PushStyleColor(color.target, color.value);
        }
        push(ImGuiStyleVar_WindowPadding, ImVec2(14 * scale, 12 * scale));
        push(ImGuiStyleVar_FramePadding, ImVec2(9 * scale, 4 * scale));
        push(ImGuiStyleVar_ItemSpacing, ImVec2(10 * scale, 8 * scale));
        push(ImGuiStyleVar_ItemInnerSpacing, ImVec2(7 * scale, 4 * scale));
        push(ImGuiStyleVar_CellPadding, ImVec2(7 * scale, 5 * scale));
        push(ImGuiStyleVar_WindowRounding, 8 * scale);
        push(ImGuiStyleVar_ChildRounding, 6 * scale);
        push(ImGuiStyleVar_PopupRounding, 6 * scale);
        push(ImGuiStyleVar_FrameRounding, 4 * scale);
        push(ImGuiStyleVar_GrabRounding, 4 * scale);
        push(ImGuiStyleVar_TabRounding, 4 * scale);
        push(ImGuiStyleVar_ScrollbarRounding, 6 * scale);
        push(ImGuiStyleVar_ScrollbarSize, 14 * scale);
        push(ImGuiStyleVar_GrabMinSize, 12 * scale);
        push(ImGuiStyleVar_WindowBorderSize, 1.0f);
        push(ImGuiStyleVar_ChildBorderSize, 1.0f);
        push(ImGuiStyleVar_PopupBorderSize, 1.0f);
        push(ImGuiStyleVar_FrameBorderSize, 1.0f);
    }

    ~ThemeScope() {
        ImGui::PopStyleVar(variables_);
        ImGui::PopStyleColor(static_cast<int>(detail::THEME_COLORS.size()));
    }
    ThemeScope(const ThemeScope&) = delete;
    ThemeScope& operator=(const ThemeScope&) = delete;

private:
    int variables_ = 0;
    void push(ImGuiStyleVar variable, float value) { ImGui::PushStyleVar(variable, value); ++variables_; }
    void push(ImGuiStyleVar variable, ImVec2 value) { ImGui::PushStyleVar(variable, value); ++variables_; }
};

inline bool sprite_button(const UiAtlasTextures& atlases, const char* label, ImVec2 size,
    bool selected = false, int icon = -1) {
    if (ImGui::GetCurrentWindow()->SkipItems) {
        return false;
    }
    const char* text_end = ImGui::FindRenderedTextEnd(label);
    const ImVec2 text_size = ImGui::CalcTextSize(label, text_end);
    const bool has_icon = icon >= 0 && icon < static_cast<int>(detail::ICONS.size());
    const ImGuiStyle& style = ImGui::GetStyle();
    if (size.y == 0) {
        size.y = ImGui::GetFrameHeight();
    }
    if (size.x == 0) {
        size.x = text_size.x + style.FramePadding.x * 2 +
            (has_icon ? std::max(0.0f, size.y * 0.6f) + style.ItemInnerSpacing.x : 0.0f);
    }
    const auto& atlas = atlases[0];
    if (!detail::available(atlas, detail::BUTTONS[3])) {
        if (selected) {
            ImGui::PushStyleColor(ImGuiCol_Button, style.Colors[ImGuiCol_HeaderActive]);
        }
        const bool pressed = ImGui::Button(label, size);
        if (selected) {
            ImGui::PopStyleColor();
        }
        return pressed;
    }

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_NavHighlight, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
    // Keep Button's native keyboard/mouse behavior and original ID. Its empty
    // label avoids the bundled Button's custom hover text drawing underneath.
    ImGui::PushOverrideID(ImGui::GetID(label));
    const bool pressed = ImGui::Button("", size);
    ImGui::PopID();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);
    ImGuiContext& g = *GImGui;
    IMGUI_TEST_ENGINE_ITEM_INFO(ImGui::GetItemID(), label, ImGui::GetItemStatusFlags());
    if (!ImGui::IsItemVisible()) {
        return pressed;
    }

    const bool disabled = (ImGui::GetItemFlags() & ImGuiItemFlags_Disabled) != 0;
    const bool hovered = ImGui::IsItemHovered();
    const bool held = ImGui::IsItemActive();
    const std::size_t state = disabled ? 4 : held ? 3 : selected ? 2 : hovered ? 1 : 0;
    const ImVec2 minimum = ImGui::GetItemRectMin();
    const ImVec2 maximum = ImGui::GetItemRectMax();
    const float width = maximum.x - minimum.x;
    const float height = maximum.y - minimum.y;
    const float scale = height / 48.0f;
    const float padding = std::min(12 * scale, width * 0.25f);
    float text_left = minimum.x + padding;
    const float text_right = maximum.x - padding;
    ImDrawList& draw = *ImGui::GetWindowDrawList();
    const ImU32 tint = ImGui::GetColorU32(ImVec4(1, 1, 1, 1));
    detail::nine_slice(draw, atlas, detail::BUTTONS[state], minimum, maximum, 12, 10, scale, tint);
    if (has_icon && width > 44 * scale) {
        const auto& icons = detail::ICONS[static_cast<std::size_t>(icon)];
        const auto rect = disabled ? icons.disabled : (selected || hovered) ? icons.active : icons.normal;
        const float side = std::min(28 * scale, std::max(0.0f, width - padding * 2));
        const float top = minimum.y + (height - side) * 0.5f;
        detail::image(draw, atlas, rect, ImVec2(text_left, top), ImVec2(text_left + side, top + side), tint);
        text_left += side + 8 * scale;
    }
    if (text_right > text_left) {
        const float text_x = has_icon ? text_left : std::max(text_left, minimum.x + (width - text_size.x) * 0.5f);
        const ImVec4 clip(text_left, minimum.y, text_right, maximum.y);
        const ImVec4 ink = disabled ? ImVec4(0.90f, 0.89f, 0.85f, 1.0f)
            : held ? ImVec4(1.0f, 0.97f, 0.88f, 1.0f) : ImVec4(0.08f, 0.06f, 0.025f, 1.0f);
        draw.AddText(ImGui::GetFont(), ImGui::GetFontSize(),
            ImVec2(text_x, minimum.y + (height - text_size.y) * 0.5f),
            ImGui::GetColorU32(ink), label, text_end, 0, &clip);
    }
    if (!disabled) {
        ImGui::RenderNavHighlight(ImRect(minimum, maximum), ImGui::GetItemID(), ImGuiNavHighlightFlags_Compact);
    }
    if (text_size.x > text_right - text_left && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("%.*s", static_cast<int>(text_end - label), label);
    }
    return pressed;
}

// Draw before the panel's contents. This does not advance the ImGui cursor.
inline void draw_panel(const UiAtlasTextures& atlases, ImVec2 minimum, ImVec2 maximum, float scale) {
    if (maximum.x <= minimum.x || maximum.y <= minimum.y) {
        return;
    }
    ImDrawList& draw = *ImGui::GetWindowDrawList();
    if (detail::available(atlases[2], detail::PANEL)) {
        detail::nine_slice(draw, atlases[2], detail::PANEL, minimum, maximum, 24, 24, scale,
            ImGui::GetColorU32(ImVec4(1, 1, 1, 1)));
    } else {
        const float rounding = std::min(8 * detail::positive_scale(scale),
            std::min(maximum.x - minimum.x, maximum.y - minimum.y) * 0.5f);
        draw.AddRectFilled(minimum, maximum, ImGui::GetColorU32(ImGuiCol_ChildBg), rounding);
        draw.AddRect(minimum, maximum, ImGui::GetColorU32(ImGuiCol_Border), rounding);
    }
}

// A read-only layout item: callers determine the tone from observed status,
// never from a checked preference alone. SameLine() may follow this helper.
inline void status_badge(const UiAtlasTextures& atlases, const char* label, StatusTone tone, float scale) {
    if (ImGui::GetCurrentWindow()->SkipItems) {
        return;
    }
    scale = detail::positive_scale(scale);
    const char* text_end = ImGui::FindRenderedTextEnd(label);
    const ImVec2 text_size = ImGui::CalcTextSize(label, text_end);
    const float padding = 14 * scale;
    const float wanted_width = std::max(54 * scale, text_size.x + padding * 2);
    const float width = std::max(1.0f, std::min(wanted_width, ImGui::GetContentRegionAvail().x));
    const float height = std::max(28 * scale, text_size.y + 8 * scale);
    ImGui::Dummy(ImVec2(width, height));
    if (!ImGui::IsItemVisible()) {
        return;
    }
    const ImVec2 minimum = ImGui::GetItemRectMin();
    const ImVec2 maximum = ImGui::GetItemRectMax();
    const std::size_t index = std::min(static_cast<std::size_t>(tone), detail::BADGES.size() - 1);
    ImDrawList& draw = *ImGui::GetWindowDrawList();
    if (detail::available(atlases[1], detail::BADGES[index])) {
        detail::nine_slice(draw, atlases[1], detail::BADGES[index], minimum, maximum, 16, 8,
            scale, ImGui::GetColorU32(ImVec4(1, 1, 1, 1)));
    } else {
        constexpr std::array COLORS{
            ImVec4(0.22f, 0.32f, 0.21f, 1.0f), ImVec4(0.38f, 0.29f, 0.13f, 1.0f),
            ImVec4(0.40f, 0.18f, 0.19f, 1.0f), ImVec4(0.58f, 0.57f, 0.55f, 1.0f),
        };
        draw.AddRectFilled(minimum, maximum, ImGui::GetColorU32(COLORS[index]), 4 * scale);
    }
    const float inset = std::min(padding, width * 0.25f);
    const ImVec4 clip(minimum.x + inset, minimum.y, maximum.x - inset, maximum.y);
    const ImVec4 ink = tone == StatusTone::Muted ? ImVec4(0.10f, 0.09f, 0.08f, 1.0f)
        : ImVec4(1.0f, 0.97f, 0.90f, 1.0f);
    draw.AddText(ImGui::GetFont(), ImGui::GetFontSize(),
        ImVec2(std::max(clip.x, minimum.x + (width - text_size.x) * 0.5f),
            minimum.y + (height - text_size.y) * 0.5f), ImGui::GetColorU32(ink), label, text_end, 0, &clip);
    if (text_size.x > clip.z - clip.x && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%.*s", static_cast<int>(text_end - label), label);
    }
}

} // namespace s8::overlay::menu_sprites
