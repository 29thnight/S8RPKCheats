#include "notifications.h"
#include "../../third_party/imgui_legacy/imgui.h"

#include <algorithm>
#include <charconv>
#include <optional>
#include <system_error>

namespace s8::overlay {
namespace {

void remove_front_notification(Notifications& state) {
    if (state.active_count != 0) {
        state.active[state.active_begin].message.clear();
        state.active_begin = (state.active_begin + 1) % state.active.size();
        --state.active_count;
    }
}

} // namespace

void append_notification_history(Notifications& state, std::string_view message) {
    const std::size_t slot = (state.history_begin + state.history_count) % state.history.size();

    state.history[slot].assign(message);
    if (state.history_count == state.history.size()) {
        state.history_begin = (state.history_begin + 1) % state.history.size();
    } else {
        ++state.history_count;
    }
}

void clear_active_notifications(Notifications& state) {
    for (Notification& notice : state.active) {
        notice.message.clear();
    }
    state.active_begin = 0;
    state.active_count = 0;
}

void draw_notifications(Notifications& state, services::UiBridge& bridge, float speed) {
    const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();

    while (state.active_count < state.active.size()) {
        const std::optional<services::PendingNotice> notice = bridge.take_notice();

        if (!notice) {
            break;
        }

        Notification& slot = state.active[(state.active_begin + state.active_count) % state.active.size()];

        slot.message.assign(notice->message);
        if (notice->repeat > 1) {
            std::array<char, 32> digits{};
            const std::to_chars_result count = std::to_chars(digits.data(), digits.data() + digits.size(), notice->repeat);

            if (count.ec == std::errc{}) {
                slot.message.append(" (x");
                slot.message.append(digits.data(), count.ptr);
                slot.message.push_back(')');
            }
        }
        slot.x = 0;
        slot.width = 0;
        slot.created = now;
        ++state.active_count;
        append_notification_history(state, slot.message);
    }

    const float elapsed = state.last_draw == std::chrono::steady_clock::time_point{}
        ? 0.0f : std::chrono::duration<float>(now - state.last_draw).count();

    state.last_draw = now;
    while (state.active_count != 0 && now - state.active[state.active_begin].created >= std::chrono::seconds{30}) {
        remove_front_notification(state);
    }
    if (state.active_count != 0) {
        constexpr float HEIGHT = 48.0f;
        constexpr ImGuiWindowFlags FLAGS = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
            ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;
        const float width = ImGui::GetIO().DisplaySize.x / 3.0f;

        ImGui::SetNextWindowPos(ImVec2((ImGui::GetIO().DisplaySize.x - width) * 0.5f, 0), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(width, HEIGHT), ImGuiCond_Always);
        if (ImGui::Begin("##MarqueeOverlay", nullptr, FLAGS)) {
            ImDrawList* const draw = ImGui::GetWindowDrawList();
            const ImVec2 position = ImGui::GetWindowPos();
            const ImVec2 end(position.x + width, position.y + HEIGHT);
            const float font_size = ImGui::GetFontSize() * 1.5f;
            float next_x = width;

            draw->AddRectFilled(position, end, IM_COL32(0, 0, 0, 160));
            draw->AddLine(ImVec2(position.x, end.y - 1), ImVec2(end.x, end.y - 1), IM_COL32(255, 255, 50, 150), 2);
            for (std::size_t index = 0; index < state.active_count; ++index) {
                Notification& notice = state.active[(state.active_begin + index) % state.active.size()];

                if (notice.width == 0) {
                    notice.width = ImGui::CalcTextSize(notice.message.c_str()).x * 1.5f;
                    notice.x = std::max(width, next_x);
                } else {
                    // Notices accepted after a closed/minimized menu start at
                    // the edge; time before their arrival is not animation time.
                    notice.x -= speed * elapsed;
                }
                next_x = notice.x + notice.width + 80.0f;
                if (notice.x + notice.width > 0 && notice.x < width) {
                    const ImVec2 text(position.x + notice.x, position.y + (HEIGHT - font_size) * 0.5f);

                    draw->AddText(nullptr, font_size, ImVec2(text.x + 1.5f, text.y + 1.5f),
                        IM_COL32(0, 0, 0, 255), notice.message.c_str());
                    draw->AddText(nullptr, font_size, text, IM_COL32(255, 255, 60, 255), notice.message.c_str());
                }
            }
            while (state.active_count != 0) {
                const Notification& front = state.active[state.active_begin];

                if (front.x + front.width >= -100.0f) {
                    break;
                }
                remove_front_notification(state);
            }
        }
        ImGui::End();
    }
    if (state.show_history) {
        const ImVec2 display = ImGui::GetIO().DisplaySize;

        ImGui::SetNextWindowPos(ImVec2(display.x - 20, display.y * 0.12f + 5), ImGuiCond_Always, ImVec2(1, 0));
        ImGui::SetNextWindowSize(ImVec2(320, 0), ImGuiCond_Always);
        if (ImGui::Begin("알림 기록##history", &state.show_history, ImGuiWindowFlags_AlwaysAutoResize |
                         ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing)) {
            if (ImGui::Button("기록 삭제")) {
                for (std::string& message : state.history) {
                    message.clear();
                }
                state.history_begin = 0;
                state.history_count = 0;
            }
            ImGui::Separator();
            if (state.history_count == 0) {
                ImGui::TextDisabled("기록된 내용이 없습니다.");
            }
            if (ImGui::BeginChild("##history_scroll", ImVec2(0, 300), false)) {
                for (std::size_t offset = 0; offset < state.history_count; ++offset) {
                    const std::size_t index = (state.history_begin + state.history_count - 1 - offset) % state.history.size();

                    ImGui::TextWrapped("- %s", state.history[index].c_str());
                }
            }
            ImGui::EndChild();
        }
        ImGui::End();
    }
}

} // namespace s8::overlay
