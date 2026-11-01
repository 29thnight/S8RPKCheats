#pragma once

#include "imgui.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace s8::preview {

struct UiItem {
    ImGuiID id = 0;
    std::string label;
    std::string window;
    ImVec2 min{}, max{};
    bool enabled = false;
    bool visible = false;
};

// Read-only instrumentation through ImGui's existing test hooks. Actual input
// tests still travel through the SDL platform backend and ImGui event queue.
void begin_probe_frame();
const std::vector<UiItem>& ui_items();
std::optional<UiItem> find_item(std::string_view label, std::string_view window = {});
void dump_items(const std::string& path);

} // namespace s8::preview
