#include "ui_probe.h"
#include "imgui_internal.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <stdexcept>
#include <unordered_map>

namespace {
std::vector<s8::preview::UiItem> items;
std::unordered_map<ImGuiID, std::size_t> indices;
std::optional<ImRect> remembered_workbench;

ImGuiWindow* workbench_window() { return ImGui::FindWindowByName("###S8RPK.Workbench"); }
ImGuiWindow* launcher_window() { return ImGui::FindWindowByName("###S8RPK.Launcher"); }
bool drawn(const ImGuiWindow* window) { return window && window->Active && !window->Hidden; }
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
void require_window_fit(const ImGuiWindow* window) {
    const auto& viewport = *ImGui::GetMainViewport();
    require(drawn(window), "Expected a visible window for bounds check");
    require(window->Pos.x >= viewport.WorkPos.x - 1 && window->Pos.y >= viewport.WorkPos.y - 1
        && window->Pos.x + window->Size.x <= viewport.WorkPos.x + viewport.WorkSize.x + 1
        && window->Pos.y + window->Size.y <= viewport.WorkPos.y + viewport.WorkSize.y + 1,
        "Visible window extends outside the viewport work area");
}
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
    const auto& clip = context->CurrentWindow->ClipRect;
    item.visible = bounds.Overlaps(clip);
    // Capture during ItemAdd: EndTable later restores the whole-window clip,
    // which would conceal an item extending beyond its own column.
    item.fits_clip = bounds.Min.x >= clip.Min.x - 1 && bounds.Min.y >= clip.Min.y - 1
        && bounds.Max.x <= clip.Max.x + 1 && bounds.Max.y <= clip.Max.y + 1;
    // ImGui's native title buttons have real ItemAdd rectangles but no labels.
    // Name those observed IDs for the SDL driver without invoking their actions.
    if (context->CurrentWindow->ID == ImHashStr("###S8RPK.Workbench")) {
        // ItemAdd also runs during window construction, before its ID stack is
        // ready. Title controls use the window root seed, so hash it directly.
        if (id == ImHashStr("#CLOSE", 0, context->CurrentWindow->ID)) item.label = "workbench-close";
        if (id == ImHashStr("#COLLAPSE", 0, context->CurrentWindow->ID)) item.label = "workbench-collapse";
    }
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

bool probe_assertion(std::string_view verb, std::string_view value) {
    const auto* workbench = workbench_window();
    const auto* launcher = launcher_window();
    const float scale = ImGui::GetIO().FontGlobalScale;
    if (verb == "expect-shell") {
        if (value == "icon") {
            require(!drawn(workbench), "Minimized workbench still renders a window or title bar");
            require(drawn(launcher), "Minimized launcher is missing");
            require(find_item("편집 도구 열기##launcher").has_value(), "Launcher has no clickable item");
            require(!find_item("개요", "workbench_navigation"), "Hidden navigation remains interactive");
        } else if (value == "expanded") {
            require(drawn(workbench) && !workbench->Collapsed, "Workbench was not restored");
            require(!drawn(launcher), "Launcher remains visible over the expanded workbench");
        } else throw std::runtime_error("Unknown shell state");
    } else if (verb == "expect-compact") {
        require(drawn(workbench) && !workbench->Collapsed, "Compact layout needs an expanded workbench");
        require(workbench->Size.x <= 920.0f * scale + 1 && workbench->Size.y <= 640.0f * scale + 1,
            "Default workbench exceeds the compact size budget");
    } else if (verb == "expect-nav-tight") {
        constexpr std::array labels{"개요", "무장", "기재 · 성장", "도시 · 내정",
            "평정 · 교류", "전투", "시나리오", "환경설정"};
        std::optional<UiItem> previous;
        for (const auto* label : labels) {
            const auto item = find_item(label, "workbench_navigation");
            require(item.has_value(), "A category row is missing from the compact sidebar");
            require(item->max.y - item->min.y <= 36.0f * scale + 1,
                "A category row is taller than the compact budget");
            require(item->max.x - item->min.x <= 180.0f * scale + 1,
                "Sidebar is wider than the compact budget");
            if (previous) {
                require(std::abs(item->min.x - previous->min.x) <= 1
                    && std::abs(item->max.x - previous->max.x) <= 1,
                    "Category rows do not share a continuous column");
                require(item->min.y >= previous->max.y - 1 && item->min.y - previous->max.y <= scale + 1,
                    "Excess vertical whitespace remains between category rows");
            }
            previous = item;
        }
    } else if (verb == "remember-workbench") {
        require(drawn(workbench) && !workbench->Collapsed, "Cannot remember hidden workbench geometry");
        remembered_workbench = ImRect(workbench->Pos,
            ImVec2(workbench->Pos.x + workbench->Size.x, workbench->Pos.y + workbench->Size.y));
    } else if (verb == "expect-workbench-restored") {
        require(drawn(workbench) && !workbench->Collapsed && remembered_workbench.has_value(),
            "No restored workbench geometry to compare");
        const auto& saved = *remembered_workbench;
        require(std::abs(workbench->Pos.x - saved.Min.x) <= 1
            && std::abs(workbench->Pos.y - saved.Min.y) <= 1
            && std::abs(workbench->Size.x - saved.GetWidth()) <= 1
            && std::abs(workbench->Size.y - saved.GetHeight()) <= 1,
            "Minimize/restore changed the main window position or size");
    } else if (verb == "expect-launcher-fit") {
        require_window_fit(launcher);
        require(launcher->Size.x <= 48.0f * scale + 1 && launcher->Size.y <= 48.0f * scale + 1,
            "Minimized launcher is larger than a small icon");
    } else if (verb == "expect-mouse-capture") {
        require(ImGui::GetIO().WantCaptureMouse == (value == "yes"),
            "Unexpected mouse capture outside the minimized launcher");
    } else if (verb == "expect-item-fit") {
        const auto item = find_item(value);
        if (!item || !item->fits_clip)
            throw std::runtime_error("UI item is clipped by its window or table column: " + std::string(value));
    } else return false;
    return true;
}

} // namespace s8::preview
