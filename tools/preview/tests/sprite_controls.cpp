#include "overlay/menu_sprites.h"
#include <cstdlib>
#include <cmath>
#include <cstdio>

// Headless behavioral checks against the production helper and bundled ImGui.
// Texture IDs are opaque test handles: actual PNG rendering is covered by the
// SDL preview. This executable never loads or accesses a game process.
using namespace s8::overlay;

void require(bool condition, const char* expression, int line) {
    if (!condition) {
        std::fprintf(stderr, "FAIL at line %d: %s\n", line, expression);
        std::abort();
    }
}

// Always evaluate checks, including frame/input calls, in Release builds.
#define CHECK(condition) require((condition), #condition, __LINE__)

struct FrameResult { bool pressed; ImGuiID id; ImVec2 minimum, maximum; };
enum class Control { Action, Navigation, Launcher };

FrameResult frame(const UiAtlasTextures& atlases, Control control, bool disabled = false) {
    ImGui::NewFrame();
    const int colors_before = GImGui->ColorStack.Size;
    const int variables_before = GImGui->StyleVarStack.Size;
    const ImVec2 padding_before = ImGui::GetStyle().FramePadding;
    const ImVec4 background_before = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
    FrameResult result{};
    {
        menu_sprites::ThemeScope theme(1.0f);
        ImGui::SetNextWindowPos(ImVec2(10, 10));
        ImGui::SetNextWindowSize(ImVec2(780, 580));
        ImGui::Begin("Sprite smoke", nullptr, ImGuiWindowFlags_NoSavedSettings);
        ImGui::BeginDisabled(disabled);
        switch (control) {
        case Control::Action:
            result.pressed = menu_sprites::sprite_button(atlases, "Action##stable", ImVec2(200, 30),
                true, menu_sprites::Officer);
            break;
        case Control::Navigation:
            result.pressed = menu_sprites::navigation_row(atlases, "Action##stable", ImVec2(170, 34), true);
            break;
        case Control::Launcher:
            result.pressed = menu_sprites::launcher_button(atlases, "Action##stable", 36.0f, 1.0f);
            break;
        }
        result.id = ImGui::GetItemID();
        CHECK(result.id == ImGui::GetID("Action##stable"));
        result.minimum = ImGui::GetItemRectMin();
        result.maximum = ImGui::GetItemRectMax();
        ImGui::EndDisabled();
        menu_sprites::sprite_button(atlases, "Extremely long label##clipped", ImVec2(8, 2), false,
            menu_sprites::Settings);
        const ImVec2 cursor = ImGui::GetCursorScreenPos();
        menu_sprites::draw_panel(atlases, ImVec2(500, 400), ImVec2(500.2f, 400.3f), 0.01f);
        menu_sprites::navigation_background(atlases, ImVec2(500, 400), ImVec2(500.2f, 400.3f), 0.01f);
        const ImVec2 after_panel = ImGui::GetCursorScreenPos();
        CHECK(cursor.x == after_panel.x && cursor.y == after_panel.y);
        menu_sprites::status_badge(atlases, "Ready", menu_sprites::StatusTone::Good, 0.5f);
        menu_sprites::status_badge(atlases, "Waiting", menu_sprites::StatusTone::Waiting, 1.0f);
        menu_sprites::status_badge(atlases, "Error", menu_sprites::StatusTone::Error, 1.5f);
        menu_sprites::status_badge(atlases, "Off", menu_sprites::StatusTone::Muted, 0.05f);
        ImGui::End();
    }
    CHECK(GImGui->ColorStack.Size == colors_before);
    CHECK(GImGui->StyleVarStack.Size == variables_before);
    const ImGuiStyle& style = ImGui::GetStyle();
    CHECK(style.FramePadding.x == padding_before.x && style.FramePadding.y == padding_before.y);
    const auto background = style.Colors[ImGuiCol_WindowBg];
    CHECK(background.x == background_before.x && background.y == background_before.y &&
        background.z == background_before.z && background.w == background_before.w);
    ImGui::Render();
    const ImDrawData& data = *ImGui::GetDrawData();
    for (int i = 0; i < data.CmdListsCount; ++i) {
        for (const ImDrawVert& vertex : data.CmdLists[i]->VtxBuffer) {
            CHECK(std::isfinite(vertex.pos.x) && std::isfinite(vertex.pos.y));
            CHECK(std::isfinite(vertex.uv.x) && std::isfinite(vertex.uv.y));
            CHECK(vertex.uv.x >= 0 && vertex.uv.x <= 1 && vertex.uv.y >= 0 && vertex.uv.y <= 1);
        }
    }
    return result;
}

void check_control(Control control) {
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(800, 600);
    io.DeltaTime = 1.0f / 60.0f;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    unsigned char* pixels = nullptr;
    int width = 0, height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    io.Fonts->SetTexID(reinterpret_cast<void*>(1));
    UiAtlasTextures loaded{{{reinterpret_cast<void*>(2), 1024, 512},
        {reinterpret_cast<void*>(3), 1024, 512}, {reinterpret_cast<void*>(4), 2048, 2048},
        {reinterpret_cast<void*>(5), 2048, 1024}, {reinterpret_cast<void*>(6), 1024, 256}}};
    UiAtlasTextures missing{};
    const auto initial = frame(loaded, control);
    const auto ready = frame(loaded, control);
    io.AddMousePosEvent((ready.minimum.x + ready.maximum.x) / 2,
        (ready.minimum.y + ready.maximum.y) / 2);
    frame(loaded, control);
    io.AddMouseButtonEvent(0, true);
    CHECK(!frame(loaded, control).pressed);
    io.AddMouseButtonEvent(0, false);
    CHECK(frame(loaded, control).pressed);
    frame(loaded, control, true);
    io.AddMouseButtonEvent(0, true);
    CHECK(!frame(loaded, control, true).pressed);
    io.AddMouseButtonEvent(0, false);
    CHECK(!frame(loaded, control, true).pressed);
    io.AddMousePosEvent(-100, -100);
    frame(loaded, control);
    // Native keyboard navigation must first become visible (the public focus
    // API intentionally preserves mouse mode and is not a substitute for Tab).
    for (int attempt = 0; attempt < 3; ++attempt) {
        io.AddKeyEvent(ImGuiKey_Tab, true);
        frame(loaded, control);
        io.AddKeyEvent(ImGuiKey_Tab, false);
        frame(loaded, control);
        if (GImGui->NavId == initial.id && !GImGui->NavDisableHighlight) break;
    }
    CHECK(GImGui->NavId == initial.id);
    CHECK(!GImGui->NavDisableHighlight);
    io.AddKeyEvent(ImGuiKey_Space, true);
    const bool keyboard_press = frame(loaded, control).pressed;
    io.AddKeyEvent(ImGuiKey_Space, false);
    const bool keyboard_release = frame(loaded, control).pressed;
    CHECK(keyboard_press || keyboard_release);
    CHECK(frame(missing, control).id == initial.id);
    CHECK(GImGui->NavId == initial.id);
    CHECK(frame(loaded, control).id == initial.id);
    ImGui::DestroyContext();
}

int main() {
    check_control(Control::Action);
    check_control(Control::Navigation);
    check_control(Control::Launcher);
    std::puts("PASS: action, navigation and launcher mouse/keyboard activation, disabled input, fallback ID/focus continuity, finite clipped UV geometry, and ThemeScope restoration.");
}
