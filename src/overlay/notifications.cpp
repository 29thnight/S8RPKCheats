#include "notifications.h"
#include "../../third_party/imgui_legacy/imgui.h"

#include <algorithm>
#include <utility>

namespace s8::overlay {

void draw_notifications(Notifications& state, services::UiBridge& bridge, float speed) {
    const auto now = std::chrono::steady_clock::now();
    while (state.active.size() < 8) {
        auto notice = bridge.take_notice();
        if (!notice) {
            break;
        }
        auto message = std::move(notice->message);
        if (notice->repeat > 1) {
            message += " (x" + std::to_string(notice->repeat) + ")";
        }
        state.history.push_back(message);
        if (state.history.size() > 50) {
            state.history.erase(state.history.begin());
        }
        state.active.push_back({std::move(message), 0, 0, now});
    }
    const float elapsed = state.last_draw == std::chrono::steady_clock::time_point{}
        ? 0.0f : std::chrono::duration<float>(now - state.last_draw).count();
    state.last_draw = now;
    if (!state.active.empty()) {
        const float width = ImGui::GetIO().DisplaySize.x / 3.0f;
        constexpr float HEIGHT = 48.0f;
        ImGui::SetNextWindowPos(ImVec2((ImGui::GetIO().DisplaySize.x - width) * 0.5f, 0), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(width, HEIGHT), ImGuiCond_Always);
        constexpr auto FLAGS = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
            ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;
        if (ImGui::Begin("##MarqueeOverlay", nullptr, FLAGS)) {
            auto* draw = ImGui::GetWindowDrawList();
            const auto position = ImGui::GetWindowPos();
            const ImVec2 end(position.x + width, position.y + HEIGHT);
            draw->AddRectFilled(position, end, IM_COL32(0, 0, 0, 160));
            draw->AddLine(ImVec2(position.x, end.y - 1), ImVec2(end.x, end.y - 1), IM_COL32(255, 255, 50, 150), 2);
            float next_x = width;
            const float font_size = ImGui::GetFontSize() * 1.5f;
            for (auto it = state.active.begin(); it != state.active.end();) {
                if (now - it->created >= std::chrono::seconds{30}) {
                    it = state.active.erase(it);
                    continue;
                }
                if (it->width == 0) {
                    it->width = ImGui::CalcTextSize(it->message.c_str()).x * 1.5f;
                    it->x = std::max(width, next_x);
                }
                it->x -= speed * elapsed;
                next_x = it->x + it->width + 80.0f;
                if (it->x + it->width > 0 && it->x < width) {
                    const ImVec2 text(position.x + it->x, position.y + (HEIGHT - font_size) * 0.5f);
                    draw->AddText(nullptr, font_size, ImVec2(text.x + 1.5f, text.y + 1.5f), IM_COL32(0, 0, 0, 255), it->message.c_str());
                    draw->AddText(nullptr, font_size, text, IM_COL32(255, 255, 60, 255), it->message.c_str());
                }
                if (it->x + it->width < -100.0f) {
                    it = state.active.erase(it);
                } else {
                    ++it;
                }
            }
        }
        ImGui::End();
    }
    if (state.show_history) {
        const auto display = ImGui::GetIO().DisplaySize;
        ImGui::SetNextWindowPos(ImVec2(display.x - 20, display.y * 0.12f + 5), ImGuiCond_Always, ImVec2(1, 0));
        ImGui::SetNextWindowSize(ImVec2(320, 0), ImGuiCond_Always);
        if (ImGui::Begin("알림 기록##history", &state.show_history, ImGuiWindowFlags_AlwaysAutoResize |
                         ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing)) {
            if (ImGui::Button("기록 삭제")) {
                state.history.clear();
            }
            ImGui::Separator();
            if (state.history.empty()) {
                ImGui::TextDisabled("기록된 내용이 없습니다.");
            }
            if (ImGui::BeginChild("##history_scroll", ImVec2(0, 300), false)) {
                for (auto it = state.history.rbegin(); it != state.history.rend(); ++it) {
                    ImGui::TextWrapped("- %s", it->c_str());
                }
            }
            ImGui::EndChild();
        }
        ImGui::End();
    }
}

} // namespace s8::overlay
