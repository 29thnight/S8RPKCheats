#include "ui_probe.h"
#include "imgui_internal.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <unordered_map>

namespace {
std::vector<s8::preview::UiItem> items;
std::unordered_map<ImGuiID, std::size_t> indices;
}

void ImGuiTestEngineHook_ItemAdd(ImGuiContext* context, ImGuiID id, const ImRect& bounds,
    const ImGuiLastItemData*) {
    if (!id || !context->CurrentWindow) return;
    const auto [entry, inserted] = indices.emplace(id, items.size());
    if (inserted) items.emplace_back();
    auto& item = items[entry->second];
    item.id = id;
    item.min = bounds.Min;
    item.max = bounds.Max;
    item.window = context->CurrentWindow->Name;
    item.enabled = (context->CurrentItemFlags & ImGuiItemFlags_Disabled) == 0;
    item.visible = bounds.Overlaps(context->CurrentWindow->ClipRect);
}

void ImGuiTestEngineHook_ItemInfo(ImGuiContext*, ImGuiID id, const char* label, ImGuiItemStatusFlags) {
    // TabItemEx reports its label before ItemAdd supplies its rectangle.
    const auto [entry, inserted] = indices.emplace(id, items.size());
    if (inserted) items.emplace_back();
    auto& item = items[entry->second];
    item.id = id;
    item.label = label ? label : "";
}

void ImGuiTestEngineHook_Log(ImGuiContext*, const char*, ...) {}
const char* ImGuiTestEngine_FindItemDebugLabel(ImGuiContext*, ImGuiID id) {
    const auto it = indices.find(id);
    return it == indices.end() ? nullptr : items[it->second].label.c_str();
}

namespace s8::preview {

void begin_probe_frame() { items.clear(); indices.clear(); }
const std::vector<UiItem>& ui_items() { return items; }
std::optional<UiItem> find_item(std::string_view label, std::string_view window) {
    const auto it = std::find_if(items.rbegin(), items.rend(), [&](const UiItem& item) {
        return item.label == label && (window.empty() || item.window.find(window) != std::string::npos)
            && item.visible;
    });
    return it == items.rend() ? std::nullopt : std::optional<UiItem>(*it);
}
void dump_items(const std::string& path) {
    std::ofstream out(path);
    for (const auto& item : items) {
        out << std::quoted(item.window) << ' ' << std::quoted(item.label) << ' '
            << item.min.x << ' ' << item.min.y << ' ' << item.max.x << ' ' << item.max.y
            << ' ' << item.enabled << ' ' << item.visible << '\n';
    }
}

} // namespace s8::preview
