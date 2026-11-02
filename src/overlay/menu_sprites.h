#pragma once

#include "ui_atlas.h"
#include "editor_symbol.h"
#include "../../third_party/imgui_legacy/imgui_internal.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <span>

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

// compact_controls.png at its original repository resolution (2048 x 1024).
// Native brown/gold states: normal, hover, selected, pressed, disabled.
inline constexpr std::array COMPACT_BUTTONS{
    SpriteRect{1500, 120, 1607, 155}, SpriteRect{1607, 140, 1714, 175},
    SpriteRect{1714, 151, 1821, 186}, SpriteRect{1928, 151, 2035, 186},
    SpriteRect{1821, 151, 1928, 186},
};
inline constexpr std::array CATEGORY_TABS{
    SpriteRect{724, 284, 820, 334}, SpriteRect{822, 284, 918, 334},
    SpriteRect{920, 284, 1016, 336},
};
// return_controls.png (Sam8RE e295681c_74a9de1537_013), without baked text.
inline constexpr std::array BACK_BUTTONS{
    SpriteRect{12, 12, 148, 148}, SpriteRect{174, 12, 310, 148},
    SpriteRect{336, 12, 472, 148}, SpriteRect{498, 12, 634, 148},
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
// paper_panels.png (Sam8RE e9ed623d_6cb25e2cc1_011), original 2048 x 2048.
inline constexpr SpriteRect PANEL{0, 0, 1280, 720};
inline constexpr SpriteRect LANDSCAPE{1284, 488, 1928, 646};

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
    // Shared integer boundaries prevent one-pixel gaps between SDL/DX triangles
    // when the destination control uses a fractional display scale.
    const std::array xs{std::round(minimum.x), std::round(minimum.x + target_x),
        std::round(maximum.x - target_x), std::round(maximum.x)};
    const std::array ys{std::round(minimum.y), std::round(minimum.y + target_y),
        std::round(maximum.y - target_y), std::round(maximum.y)};
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

// The native menu uses dark ink on opaque paper. Custom gold controls below
// select their own cream/black ink instead of inheriting the body text color.
inline constexpr std::array THEME_COLORS{
    ThemeColor{ImGuiCol_Text, {0.18f, 0.14f, 0.10f, 1.0f}},
    ThemeColor{ImGuiCol_TextDisabled, {0.43f, 0.38f, 0.31f, 1.0f}},
    ThemeColor{ImGuiCol_WindowBg, {0.89f, 0.87f, 0.80f, 1.0f}},
    ThemeColor{ImGuiCol_ChildBg, {0.94f, 0.92f, 0.85f, 0.90f}},
    ThemeColor{ImGuiCol_PopupBg, {0.94f, 0.92f, 0.85f, 1.0f}},
    ThemeColor{ImGuiCol_Border, {0.56f, 0.49f, 0.32f, 0.95f}},
    ThemeColor{ImGuiCol_BorderShadow, {0.18f, 0.14f, 0.08f, 0.24f}},
    ThemeColor{ImGuiCol_FrameBg, {0.96f, 0.93f, 0.84f, 1.0f}},
    ThemeColor{ImGuiCol_FrameBgHovered, {0.99f, 0.91f, 0.65f, 1.0f}},
    ThemeColor{ImGuiCol_FrameBgActive, {1.0f, 0.87f, 0.48f, 1.0f}},
    ThemeColor{ImGuiCol_TitleBg, {0.76f, 0.68f, 0.49f, 1.0f}},
    ThemeColor{ImGuiCol_TitleBgActive, {0.84f, 0.74f, 0.47f, 1.0f}},
    ThemeColor{ImGuiCol_TitleBgCollapsed, {0.75f, 0.69f, 0.54f, 1.0f}},
    ThemeColor{ImGuiCol_MenuBarBg, {0.87f, 0.83f, 0.70f, 1.0f}},
    ThemeColor{ImGuiCol_ScrollbarBg, {0.58f, 0.54f, 0.46f, 0.24f}},
    ThemeColor{ImGuiCol_ScrollbarGrab, {0.70f, 0.65f, 0.52f, 1.0f}},
    ThemeColor{ImGuiCol_ScrollbarGrabHovered, {0.78f, 0.67f, 0.37f, 1.0f}},
    ThemeColor{ImGuiCol_ScrollbarGrabActive, {0.86f, 0.68f, 0.24f, 1.0f}},
    ThemeColor{ImGuiCol_CheckMark, {0.52f, 0.28f, 0.06f, 1.0f}},
    ThemeColor{ImGuiCol_SliderGrab, {0.67f, 0.49f, 0.15f, 1.0f}},
    ThemeColor{ImGuiCol_SliderGrabActive, {0.86f, 0.60f, 0.12f, 1.0f}},
    ThemeColor{ImGuiCol_Button, {0.81f, 0.70f, 0.43f, 1.0f}},
    ThemeColor{ImGuiCol_ButtonHovered, {1.0f, 0.84f, 0.27f, 1.0f}},
    ThemeColor{ImGuiCol_ButtonActive, {0.94f, 0.70f, 0.18f, 1.0f}},
    ThemeColor{ImGuiCol_Header, {0.78f, 0.67f, 0.40f, 0.72f}},
    ThemeColor{ImGuiCol_HeaderHovered, {1.0f, 0.85f, 0.33f, 0.80f}},
    ThemeColor{ImGuiCol_HeaderActive, {1.0f, 0.82f, 0.22f, 0.92f}},
    ThemeColor{ImGuiCol_Separator, {0.52f, 0.45f, 0.30f, 0.65f}},
    ThemeColor{ImGuiCol_SeparatorHovered, {0.65f, 0.45f, 0.12f, 1.0f}},
    ThemeColor{ImGuiCol_SeparatorActive, {0.84f, 0.58f, 0.12f, 1.0f}},
    ThemeColor{ImGuiCol_ResizeGrip, {0.49f, 0.40f, 0.22f, 0.35f}},
    ThemeColor{ImGuiCol_ResizeGripHovered, {0.72f, 0.58f, 0.29f, 0.80f}},
    ThemeColor{ImGuiCol_ResizeGripActive, {0.91f, 0.74f, 0.34f, 1.0f}},
    ThemeColor{ImGuiCol_Tab, {0.75f, 0.65f, 0.40f, 1.0f}},
    ThemeColor{ImGuiCol_TabHovered, {1.0f, 0.84f, 0.30f, 1.0f}},
    ThemeColor{ImGuiCol_TabSelected, {1.0f, 0.82f, 0.20f, 1.0f}},
    ThemeColor{ImGuiCol_TabSelectedOverline, {0.58f, 0.39f, 0.06f, 1.0f}},
    ThemeColor{ImGuiCol_TabDimmed, {0.82f, 0.77f, 0.65f, 1.0f}},
    ThemeColor{ImGuiCol_TabDimmedSelected, {0.88f, 0.76f, 0.46f, 1.0f}},
    ThemeColor{ImGuiCol_TabDimmedSelectedOverline, {0.60f, 0.49f, 0.27f, 1.0f}},
    ThemeColor{ImGuiCol_TableHeaderBg, {0.79f, 0.69f, 0.44f, 0.96f}},
    ThemeColor{ImGuiCol_TableBorderStrong, {0.53f, 0.46f, 0.29f, 0.85f}},
    ThemeColor{ImGuiCol_TableBorderLight, {0.52f, 0.47f, 0.37f, 0.46f}},
    ThemeColor{ImGuiCol_TableRowBg, {0, 0, 0, 0}},
    ThemeColor{ImGuiCol_TableRowBgAlt, {0.45f, 0.37f, 0.20f, 0.035f}},
    ThemeColor{ImGuiCol_TextSelectedBg, {0.94f, 0.73f, 0.23f, 0.52f}},
    ThemeColor{ImGuiCol_NavHighlight, {0.47f, 0.26f, 0.05f, 1.0f}},
};

// Use a real button for capture, keyboard navigation, disabled state and IDs.
// Only its paint is supplied by the sprite/list helpers below.
inline bool button_item(const char* label, ImVec2 size) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_NavHighlight, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
    ImGui::PushOverrideID(ImGui::GetID(label));
    const bool pressed = ImGui::Button("", size);
    ImGui::PopID();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);
    [[maybe_unused]] ImGuiContext& g = *GImGui;
    IMGUI_TEST_ENGINE_ITEM_INFO(ImGui::GetItemID(), label, ImGui::GetItemStatusFlags());
    return pressed;
}

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
        push(ImGuiStyleVar_WindowPadding, ImVec2(10 * scale, 8 * scale));
        push(ImGuiStyleVar_FramePadding, ImVec2(7 * scale, 3 * scale));
        push(ImGuiStyleVar_ItemSpacing, ImVec2(7 * scale, 4 * scale));
        push(ImGuiStyleVar_ItemInnerSpacing, ImVec2(5 * scale, 3 * scale));
        push(ImGuiStyleVar_CellPadding, ImVec2(5 * scale, 3 * scale));
        push(ImGuiStyleVar_WindowRounding, 0.0f);
        push(ImGuiStyleVar_ChildRounding, 0.0f);
        push(ImGuiStyleVar_PopupRounding, 2 * scale);
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
    const bool compact = detail::available(atlases[3], detail::COMPACT_BUTTONS[1]) &&
        detail::available(atlases[3], detail::COMPACT_BUTTONS[4]);
    const auto& atlas = atlases[compact ? 3 : 0];
    const auto& buttons = compact ? detail::COMPACT_BUTTONS : detail::BUTTONS;
    if (!detail::available(atlas, buttons[3])) {
        if (selected) {
            ImGui::PushStyleColor(ImGuiCol_Button, style.Colors[ImGuiCol_HeaderActive]);
        }
        const bool pressed = ImGui::Button(label, size);
        if (selected) {
            ImGui::PopStyleColor();
        }
        return pressed;
    }

    const bool pressed = detail::button_item(label, size);
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
    const float scale = height / (compact ? 35.0f : 48.0f);
    const float padding = std::min((compact ? 9 : 12) * scale, width * 0.25f);
    float text_left = minimum.x + padding;
    const float text_right = maximum.x - padding;
    ImDrawList& draw = *ImGui::GetWindowDrawList();
    const ImU32 tint = ImGui::GetColorU32(ImVec4(1, 1, 1, 1));
    detail::nine_slice(draw, atlas, buttons[state], minimum, maximum,
        compact ? 10 : 12, compact ? 7 : 10, scale, tint);
    if (has_icon && width > 44 * scale) {
        const auto& icons = detail::ICONS[static_cast<std::size_t>(icon)];
        const auto rect = disabled ? icons.disabled : (selected || hovered) ? icons.active : icons.normal;
        const float side = std::min((compact ? 21 : 28) * scale, std::max(0.0f, width - padding * 2));
        const float top = minimum.y + (height - side) * 0.5f;
        detail::image(draw, atlases[0], rect, ImVec2(text_left, top), ImVec2(text_left + side, top + side), tint);
        text_left += side + (compact ? 5 : 8) * scale;
    }
    if (text_right > text_left) {
        const float text_x = has_icon ? text_left : std::max(text_left, minimum.x + (width - text_size.x) * 0.5f);
        const ImVec4 clip(text_left, minimum.y, text_right, maximum.y);
        const ImVec4 ink = disabled ? ImVec4(0.78f, 0.77f, 0.71f, 1.0f)
            : held && !compact ? ImVec4(1.0f, 0.97f, 0.85f, 1.0f)
            : selected || hovered || held ? ImVec4(0.22f, 0.13f, 0.025f, 1.0f)
            : ImVec4(1.0f, 0.97f, 0.85f, 1.0f);
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

// The tab strip shares one baseline with the paper content panel.
inline void navigation_background(const UiAtlasTextures&, ImVec2 minimum, ImVec2 maximum, float) {
    ImGui::GetWindowDrawList()->AddLine(ImVec2(minimum.x, maximum.y), maximum,
        ImGui::GetColorU32(ImGuiCol_Border));
}

inline bool navigation_row(const UiAtlasTextures& atlases, const char* label, ImVec2 size, bool selected) {
    if (ImGui::GetCurrentWindow()->SkipItems) return false;
    const bool pressed = detail::button_item(label, size);
    if (!ImGui::IsItemVisible()) return pressed;
    const ImVec2 minimum = ImGui::GetItemRectMin(), maximum = ImGui::GetItemRectMax();
    const bool disabled = (ImGui::GetItemFlags() & ImGuiItemFlags_Disabled) != 0;
    const bool hovered = ImGui::IsItemHovered();
    const float scale = size.y / 34.0f;
    const ImVec2 tab_min(minimum.x, minimum.y + (selected ? 0.0f : 4.0f * scale));
    const std::size_t state = selected ? 2 : hovered ? 1 : 0;
    ImDrawList& draw = *ImGui::GetWindowDrawList();
    if (detail::available(atlases[0], detail::CATEGORY_TABS[state])) {
        detail::nine_slice(draw, atlases[0], detail::CATEGORY_TABS[state], tab_min, maximum,
            14, 10, scale * 0.72f, ImGui::GetColorU32(ImVec4(1, 1, 1, 1)));
    } else {
        draw.AddRectFilled(tab_min, maximum, ImGui::GetColorU32(selected ? ImGuiCol_ButtonActive : ImGuiCol_Button));
        draw.AddRect(tab_min, maximum, ImGui::GetColorU32(ImGuiCol_Border));
    }
    const char* end = ImGui::FindRenderedTextEnd(label);
    const ImVec2 text = ImGui::CalcTextSize(label, end);
    const float inset = std::min(8.0f * scale, size.x * 0.2f);
    const ImVec4 clip(minimum.x + inset, tab_min.y, maximum.x - inset, maximum.y);
    const ImVec4 ink = disabled ? ImVec4(0.69f, 0.66f, 0.57f, 1.0f)
        : selected || hovered ? ImVec4(0.27f, 0.16f, 0.03f, 1.0f) : ImVec4(1.0f, 0.97f, 0.85f, 1.0f);
    draw.AddText(ImGui::GetFont(), ImGui::GetFontSize(),
        ImVec2(std::max(clip.x, minimum.x + (size.x - text.x) * 0.5f),
            tab_min.y + (maximum.y - tab_min.y - text.y) * 0.5f), ImGui::GetColorU32(ink), label, end, 0, &clip);
    if (!disabled) ImGui::RenderNavHighlight(ImRect(minimum, maximum), ImGui::GetItemID(), ImGuiNavHighlightFlags_Compact);
    return pressed;
}

inline bool launcher_button(const UiAtlasTextures& atlases, const char* label, float edge, float scale) {
    if (ImGui::GetCurrentWindow()->SkipItems) return false;
    const bool pressed = detail::button_item(label, ImVec2(edge, edge));
    if (!ImGui::IsItemVisible()) return pressed;
    const bool disabled = (ImGui::GetItemFlags() & ImGuiItemFlags_Disabled) != 0;
    const bool hovered = ImGui::IsItemHovered();
    const ImVec2 minimum = ImGui::GetItemRectMin(), maximum = ImGui::GetItemRectMax();
    const auto& icons = detail::ICONS[Overview]; // 000_btn_sys_info_0 and its state variants.
    const auto rect = disabled ? icons.disabled : hovered || ImGui::IsItemActive() ? icons.active : icons.normal;
    ImDrawList& draw = *ImGui::GetWindowDrawList();
    if (detail::available(atlases[0], rect)) {
        detail::image(draw, atlases[0], rect, minimum, maximum,
            ImGui::GetColorU32(ImVec4(1, 1, 1, 1)));
    } else {
        const ImVec2 center((minimum.x + maximum.x) * 0.5f, (minimum.y + maximum.y) * 0.5f);
        const ImU32 ink = ImGui::GetColorU32(ImVec4(0.86f, 0.78f, 0.45f, 1.0f));
        draw.AddCircleFilled(center, edge * 0.47f, IM_COL32(35, 34, 27, 245));
        draw.AddCircle(center, edge * 0.47f, ink, 32, std::max(1.0f, scale * 1.6f));
    }
    if (!disabled) ImGui::RenderNavHighlight(ImRect(minimum, maximum), ImGui::GetItemID(), ImGuiNavHighlightFlags_Compact);
    return pressed;
}

inline bool back_button(const UiAtlasTextures& atlases, const char* label, float edge, float scale) {
    if (ImGui::GetCurrentWindow()->SkipItems) return false;
    const bool pressed = detail::button_item(label, ImVec2(edge, edge));
    if (!ImGui::IsItemVisible()) return pressed;
    const bool disabled = (ImGui::GetItemFlags() & ImGuiItemFlags_Disabled) != 0;
    const bool hovered = ImGui::IsItemHovered();
    const std::size_t state = disabled ? 3 : ImGui::IsItemActive() ? 2 : hovered ? 1 : 0;
    const ImVec2 minimum = ImGui::GetItemRectMin(), maximum = ImGui::GetItemRectMax();
    ImDrawList& draw = *ImGui::GetWindowDrawList();
    if (detail::available(atlases[4], detail::BACK_BUTTONS[state])) {
        detail::image(draw, atlases[4], detail::BACK_BUTTONS[state], minimum, maximum,
            ImGui::GetColorU32(ImVec4(1, 1, 1, 1)));
    } else {
        const ImVec2 center((minimum.x + maximum.x) * 0.5f, (minimum.y + maximum.y) * 0.5f);
        // Equal bevels remain recognisable even without a texture.
        const float cut = edge * 0.28f;
        const ImVec2 points[]{{minimum.x + cut, minimum.y}, {maximum.x - cut, minimum.y},
            {maximum.x, minimum.y + cut}, {maximum.x, maximum.y - cut},
            {maximum.x - cut, maximum.y}, {minimum.x + cut, maximum.y},
            {minimum.x, maximum.y - cut}, {minimum.x, minimum.y + cut}};
        (void)center;
        draw.AddConvexPolyFilled(points, 8, ImGui::GetColorU32(disabled
            ? ImVec4(0.35f, 0.34f, 0.32f, 1) : ImVec4(0.48f, 0.09f, 0.12f, 1)));
        draw.AddPolyline(points, 8, ImGui::GetColorU32(ImVec4(0.75f, 0.64f, 0.34f, 1)), ImDrawFlags_Closed,
            std::max(1.0f, scale));
    }
    const char* end = ImGui::FindRenderedTextEnd(label);
    const ImVec2 text = ImGui::CalcTextSize(label, end);
    const ImVec4 clip(minimum.x, minimum.y, maximum.x, maximum.y);
    draw.AddText(ImGui::GetFont(), ImGui::GetFontSize(),
        ImVec2(minimum.x + (edge - text.x) * 0.5f, minimum.y + (edge - text.y) * 0.5f),
        ImGui::GetColorU32(ImVec4(1.0f, 0.94f, 0.72f, 1.0f)), label, end, 0, &clip);
    if (!disabled) ImGui::RenderNavHighlight(ImRect(minimum, maximum), ImGui::GetItemID(), ImGuiNavHighlightFlags_Compact);
    return pressed;
}

// Draw before the panel's contents. This does not advance the ImGui cursor.
inline void draw_panel(const UiAtlasTextures& atlases, ImVec2 minimum, ImVec2 maximum, float scale) {
    if (maximum.x <= minimum.x || maximum.y <= minimum.y) return;
    scale = detail::positive_scale(scale);
    ImDrawList& draw = *ImGui::GetWindowDrawList();
    draw.AddRectFilled(ImVec2(minimum.x + 3 * scale, minimum.y + 3 * scale),
        ImVec2(maximum.x + 3 * scale, maximum.y + 3 * scale), IM_COL32(0, 0, 0, 65));
    draw.AddRectFilled(minimum, maximum, ImGui::GetColorU32(ImGuiCol_WindowBg));
    if (detail::available(atlases[2], detail::PANEL)) {
        detail::image(draw, atlases[2], detail::PANEL, minimum, maximum,
            ImGui::GetColorU32(ImVec4(1, 1, 1, 1)));
        draw.AddRectFilled(minimum, maximum, ImGui::GetColorU32(ImVec4(0.94f, 0.93f, 0.86f, 0.38f)));
        const float width = std::min(maximum.x - minimum.x, 900 * scale);
        const float height = std::min(maximum.y - minimum.y, width * 158.0f / 644.0f);
        detail::image(draw, atlases[2], detail::LANDSCAPE,
            ImVec2(minimum.x + (maximum.x - minimum.x - width) * 0.5f, maximum.y - height),
            ImVec2(minimum.x + (maximum.x - minimum.x + width) * 0.5f, maximum.y),
            ImGui::GetColorU32(ImVec4(1, 1, 1, 0.16f)));
    }
    draw.AddRect(minimum, maximum, ImGui::GetColorU32(ImGuiCol_Border));
    if (maximum.x - minimum.x > 4 && maximum.y - minimum.y > 4) {
        draw.AddRect(ImVec2(minimum.x + 1, minimum.y + 1), ImVec2(maximum.x - 1, maximum.y - 1),
            ImGui::GetColorU32(ImVec4(1, 0.98f, 0.87f, 0.62f)));
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
    const float padding = 10 * scale;
    const float wanted_width = std::max(54 * scale, text_size.x + padding * 2);
    const float width = std::max(1.0f, std::min(wanted_width, ImGui::GetContentRegionAvail().x));
    const float height = std::max(24 * scale, text_size.y + 6 * scale);
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
