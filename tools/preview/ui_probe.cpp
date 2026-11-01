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
std::optional<ImRect> remembered_launcher;

ImGuiWindow* workbench_window() { return ImGui::FindWindowByName("###S8RPK.Workbench"); }
ImGuiWindow* launcher_window() { return ImGui::FindWindowByName("###S8RPK.Launcher"); }
bool drawn(const ImGuiWindow* window) { return window && window->Active && !window->Hidden; }
ImRect window_bounds(const ImGuiWindow* window) {
    return ImRect(window->Pos, ImVec2(window->Pos.x + window->Size.x, window->Pos.y + window->Size.y));
}
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
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
            require(!find_item("개요", "S8RPK.Workbench"), "Hidden navigation remains interactive");
        } else if (value == "expanded") {
            require(drawn(workbench) && !workbench->Collapsed, "Workbench was not restored");
            require(drawn(launcher), "Fixed launcher is missing while the menu is expanded");
        } else throw std::runtime_error("Unknown shell state");
    } else if (verb == "expect-compact") {
        require(drawn(workbench) && !workbench->Collapsed, "Compact layout needs an expanded workbench");
        require(workbench->Size.x <= 940.0f * scale + 1 && workbench->Size.y <= 640.0f * scale + 1,
            "Default workbench exceeds the compact size budget");
    } else if (verb == "expect-navigation-fit") {
        constexpr std::array labels{"개요", "무장", "기재 · 성장", "도시 · 내정",
            "평정 · 교류", "전투", "시나리오", "환경설정"};
        for (const auto* label : labels) {
            const auto item = find_item(label, "S8RPK.Workbench");
            require(item.has_value(), "A category is missing from the menu or upper tabs");
            require(item->fits_clip, "A category button is clipped by its panel");
            require(item->max.y - item->min.y <= 56.0f * scale + 1,
                "A category button is taller than the compact menu budget");
        }
    } else if (verb == "remember-workbench") {
        require(drawn(workbench) && !workbench->Collapsed, "Cannot remember hidden workbench geometry");
        remembered_workbench = window_bounds(workbench);
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
        require(drawn(launcher), "Fixed launcher is missing");
        const auto& viewport = *ImGui::GetMainViewport();
        require(launcher->Pos.x >= viewport.Pos.x - 1 && launcher->Pos.y >= viewport.Pos.y - 1
            && launcher->Pos.x + launcher->Size.x <= viewport.Pos.x + viewport.Size.x + 1
            && launcher->Pos.y + launcher->Size.y <= viewport.Pos.y + viewport.Size.y + 1,
            "Fixed launcher extends outside the game client viewport");
        require(launcher->Size.x >= 48.0f - 1 && launcher->Size.x <= 168.0f + 1
            && std::abs(launcher->Size.x - launcher->Size.y) <= 1,
            "Launcher is too small to click or is no longer a square HUD control");
        const auto target = find_item("편집 도구 열기##launcher");
        require(target && target->fits_clip, "Launcher click target is missing or clipped");
    } else if (verb == "remember-launcher") {
        require(drawn(launcher), "Cannot remember a hidden launcher");
        remembered_launcher = window_bounds(launcher);
    } else if (verb == "expect-launcher-unchanged") {
        require(drawn(launcher) && remembered_launcher.has_value(), "No fixed launcher geometry to compare");
        const auto& saved = *remembered_launcher;
        require(std::abs(launcher->Pos.x - saved.Min.x) <= 1
            && std::abs(launcher->Pos.y - saved.Min.y) <= 1
            && std::abs(launcher->Size.x - saved.GetWidth()) <= 1
            && std::abs(launcher->Size.y - saved.GetHeight()) <= 1,
            "Opening, closing, or scaling the editor moved the fixed launcher");
    } else if (verb == "expect-launcher-reference") {
        require(drawn(launcher), "Reference comparison needs a visible launcher");
        const auto& viewport = *ImGui::GetMainViewport();
        require(std::abs(viewport.Size.x - 2048) <= 1 && std::abs(viewport.Size.y - 1151) <= 1,
            "Reference comparison requires the supplied screenshot's 2048 x 1151 client area");
        // Measured from the supplied game's settings gear, after excluding its
        // 36 px operating-system title bar. Allow raster rounding at its edges.
        const float center_x = launcher->Pos.x + launcher->Size.x * 0.5f - viewport.Pos.x;
        require(std::abs(center_x - 2006) <= 2 && std::abs(launcher->Pos.y - viewport.Pos.y - 113) <= 2
            && std::abs(launcher->Size.x - 60) <= 2 && std::abs(launcher->Size.y - 60) <= 2,
            "Launcher does not align below the supplied screenshot's settings gear");
    } else if (verb == "expect-toast-stack") {
        const int count = std::stoi(std::string(value));
        const auto& viewport = *ImGui::GetMainViewport();
        const float center_x = viewport.Pos.x + viewport.Size.x * 0.5f;
        const ImGuiWindow* lower = nullptr;
        for (int index = 0; index < 4; ++index) {
            const auto name = "##NotificationToast" + std::to_string(index);
            const auto* toast = ImGui::FindWindowByName(name.c_str());
            if (index >= count) {
                require(!drawn(toast), "Expired or excess notification remains visible");
                continue;
            }
            require(drawn(toast), "Expected notification strip is missing");
            require((toast->Flags & ImGuiWindowFlags_NoInputs) == ImGuiWindowFlags_NoInputs,
                "Notification strip can intercept game input");
            require(std::abs(toast->Pos.x + toast->Size.x * 0.5f - center_x) <= 1,
                "Notification strip is not horizontally centered");
            require(toast->Pos.x >= viewport.Pos.x - 1 && toast->Pos.x + toast->Size.x <= viewport.Pos.x + viewport.Size.x + 1
                && toast->Pos.y >= viewport.Pos.y - 1 && toast->Pos.y + toast->Size.y <= viewport.Pos.y + viewport.Size.y + 1,
                "Notification strip is clipped by the game client");
            if (lower) require(toast->Pos.y + toast->Size.y <= lower->Pos.y,
                "Stacked notification strips overlap");
            else require(toast->Pos.y + toast->Size.y >= viewport.Pos.y + viewport.Size.y * 0.75f,
                "Newest notification does not appear near the bottom");
            lower = toast;
        }
    } else if (verb == "expect-mouse-capture") {
        require(ImGui::GetIO().WantCaptureMouse == (value == "yes"),
            "Unexpected mouse capture outside the minimized launcher");
    } else if (verb == "expect-item-absent") {
        require(!find_item(value).has_value(), "Hidden PK editor is still rendered");
    } else if (verb == "expect-item-fit") {
        const auto item = find_item(value);
        if (!item || !item->fits_clip)
            throw std::runtime_error("UI item is clipped by its window or table column: " + std::string(value));
    } else return false;
    return true;
}

} // namespace s8::preview
