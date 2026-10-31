#include "officer_notice_panel.h"

#include "../domain/city_management.h"
#include "../../third_party/imgui_legacy/imgui.h"

#include <algorithm>
#include <string>

namespace s8::overlay {
namespace {

std::string officer_name(const domain::OfficerNames* names, std::uint16_t id) {
    if (names != nullptr) {
        const domain::OfficerNames::const_iterator found = names->find(id);

        if (found != names->end() && !found->second.empty()) {
            return found->second;
        }
    }
    return "미등록 무장(ID:" + std::to_string(id) + ")";
}

std::string force_name(const domain::OfficerNames* names, std::uint16_t lord, bool has_force) {
    if (!has_force) {
        return "무소속";
    }
    return lord != 0 ? officer_name(names, lord) + " 세력" : "세력 미확인";
}

const char* city_name(std::uint8_t city) {
    return city < domain::CITY_NAMES.size() ? domain::CITY_NAMES[city].data() : "도시 미확인";
}

std::string history_line(const domain::OfficerNotice& notice, const domain::OfficerNames* names) {
    const std::string name = officer_name(names, notice.officer);
    const std::string previous = force_name(names, notice.previous_lord, notice.previous_has_force);

    if (notice.kind == domain::OfficerNoticeKind::Recruited) {
        return "[등용] " + name + " | " + previous + " → " +
            force_name(names, notice.current_lord, notice.current_has_force);
    }
    if (notice.kind == domain::OfficerNoticeKind::Died) {
        return "[사망] " + name + " | " + previous;
    }
    return "[재야 감지] " + name + " → " + city_name(notice.city);
}

void remove_front_change(OfficerNoticePanel& state) {
    if (state.changes_count != 0) {
        state.changes_begin = (state.changes_begin + 1) % state.changes.size();
        --state.changes_count;
    }
}

void draw_officer_name(const domain::OfficerNames* names, std::uint16_t id, bool force = false) {
    if (names != nullptr) {
        const domain::OfficerNames::const_iterator found = names->find(id);

        if (found != names->end() && !found->second.empty()) {
            if (force) {
                ImGui::Text("%s 세력", found->second.c_str());
            } else {
                ImGui::TextUnformatted(found->second.c_str());
            }
            return;
        }
    }
    ImGui::Text(force ? "미등록 무장(ID:%u) 세력" : "미등록 무장(ID:%u)", id);
}

void draw_force_name(const domain::OfficerNames* names, std::uint16_t lord, bool has_force) {
    if (!has_force) {
        ImGui::TextUnformatted("무소속");
    } else if (lord == 0) {
        ImGui::TextUnformatted("세력 미확인");
    } else {
        draw_officer_name(names, lord, true);
    }
}

void draw_change_rows(const domain::OfficerNotificationView& view, const domain::OfficerNames* names) {
    if (view.recruit_count != 0) {
        ImGui::Text("[등용] %zu명", view.total_recruits);
        if (ImGui::BeginTable("##recruits", 3, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("장수명");
            ImGui::TableSetupColumn("이전 세력");
            ImGui::TableSetupColumn("현재 세력");
            ImGui::TableHeadersRow();
            for (std::size_t i = 0; i < view.recruit_count; ++i) {
                const domain::OfficerNotice& notice = view.recruits[i];

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                draw_officer_name(names, notice.officer);
                ImGui::TableNextColumn();
                draw_force_name(names, notice.previous_lord, notice.previous_has_force);
                ImGui::TableNextColumn();
                draw_force_name(names, notice.current_lord, notice.current_has_force);
            }
            ImGui::EndTable();
        }
    }
    if (view.death_count != 0) {
        ImGui::Text("[사망] %zu명", view.total_deaths);
        if (ImGui::BeginTable("##deaths", 2, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("장수명");
            ImGui::TableSetupColumn("소속 세력");
            ImGui::TableHeadersRow();
            for (std::size_t i = 0; i < view.death_count; ++i) {
                const domain::OfficerNotice& notice = view.deaths[i];

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                draw_officer_name(names, notice.officer);
                ImGui::TableNextColumn();
                draw_force_name(names, notice.previous_lord, notice.previous_has_force);
            }
            ImGui::EndTable();
        }
    }
    if (view.total_recruits > view.recruit_count || view.total_deaths > view.death_count) {
        ImGui::TextWrapped("각 목록은 최대 16명까지 표시합니다. 나머지는 알림 기록에서 확인하세요.");
    }
}

} // namespace

void update_officer_notice_panel(OfficerNoticePanel& state, const domain::OfficerNoticesSnapshot& snapshot,
    Notifications& history, const domain::OfficerNames* names) {
    const domain::OfficerNotificationView& view = snapshot.view;
    const bool epoch_changed = state.epoch != view.epoch;
    const bool had_popup = state.ronin_count != 0 || state.changes_count != 0;

    if (epoch_changed) {
        state.epoch = view.epoch;
        state.ronin_sequence = 0;
        state.changes_sequence = 0;
        state.history_sequence = 0;
        state.ronin_count = 0;
        state.changes_begin = 0;
        state.changes_count = 0;
    }
    if (view.ronin_sequence != state.ronin_sequence) {
        state.ronin_sequence = view.ronin_sequence;
        if (view.ronin_count == 0) {
            state.ronin_count = 0;
        } else {
            for (std::size_t i = 0; i < view.ronin_count && state.ronin_count < state.ronins.size(); ++i) {
                state.ronins[state.ronin_count++] = {view.ronins[i], 12.0f};
            }
        }
    }
    if (view.changes_sequence != state.changes_sequence) {
        state.changes_sequence = view.changes_sequence;
        if (view.recruit_count == 0 && view.death_count == 0) {
            state.changes_begin = 0;
            state.changes_count = 0;
        } else {
            if (state.changes_count == state.changes.size()) {
                remove_front_change(state);
            }
            state.changes[(state.changes_begin + state.changes_count) % state.changes.size()] = {view, 20.0f};
            ++state.changes_count;
        }
    }
    const std::uint64_t first_sequence = snapshot.history_sequence >= snapshot.history_count
        ? snapshot.history_sequence - snapshot.history_count : 0;

    for (std::size_t i = 0; i < snapshot.history_count; ++i) {
        if (first_sequence + i + 1 > state.history_sequence) {
            append_notification_history(history, history_line(snapshot.history[i], names));
        }
    }
    state.history_sequence = snapshot.history_sequence;
    if (epoch_changed || !had_popup) {
        state.last_draw = std::chrono::steady_clock::now();
    }
}

void draw_officer_notice_panel(OfficerNoticePanel& state, bool monitor_ronins,
    const domain::OfficerNames* names, float scale) {
    const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    const float elapsed = state.last_draw == std::chrono::steady_clock::time_point{}
        ? 0.0f : std::chrono::duration<float>(now - state.last_draw).count();

    state.last_draw = now;
    if (monitor_ronins) {
        std::size_t kept = 0;

        for (std::size_t i = 0; i < state.ronin_count; ++i) {
            RoninPopupRow row = state.ronins[i];

            row.remaining_seconds -= elapsed;
            if (row.remaining_seconds > 0) {
                state.ronins[kept++] = row;
            }
        }
        state.ronin_count = kept;
    }
    const bool show_ronin = monitor_ronins && state.ronin_count != 0;

    if (!show_ronin && state.changes_count != 0) {
        state.changes[state.changes_begin].remaining_seconds -= elapsed;
        if (state.changes[state.changes_begin].remaining_seconds <= 0) {
            remove_front_change(state);
        }
    }
    if (!show_ronin && state.changes_count == 0) {
        return;
    }
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const float width = std::min(display.x - 40.0f, (show_ronin ? 330.0f : 610.0f) * scale);
    const float height = std::min(display.y * 0.75f, (show_ronin ? 350.0f : 650.0f) * scale);
    constexpr ImGuiWindowFlags FLAGS = ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoFocusOnAppearing;

    ImGui::SetNextWindowPos(ImVec2(display.x - 20.0f, display.y * 0.12f), ImGuiCond_Always, ImVec2(1, 0));
    ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);
    if (ImGui::Begin("무장 알림##officer_notice", nullptr, FLAGS)) {
        if (ImGui::BeginChild("##officer_notice_rows", ImVec2(0, -40.0f * scale), false)) {
            if (show_ronin) {
                ImGui::TextUnformatted("새로운 재야 장수");
                if (ImGui::BeginTable("##ronins", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH)) {
                    ImGui::TableSetupColumn("장수명");
                    ImGui::TableSetupColumn("도시");
                    ImGui::TableHeadersRow();
                    for (std::size_t i = 0; i < state.ronin_count; ++i) {
                        const domain::OfficerNotice& notice = state.ronins[i].notice;

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        draw_officer_name(names, notice.officer);
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(city_name(notice.city));
                    }
                    ImGui::EndTable();
                }
            } else {
                ImGui::TextUnformatted("사망장수 및 등용장수");
                draw_change_rows(state.changes[state.changes_begin].notice, names);
            }
        }
        ImGui::EndChild();
        if (ImGui::Button("확인", ImVec2(120 * scale, 0))) {
            if (show_ronin) {
                state.ronin_count = 0;
            } else {
                remove_front_change(state);
            }
        }
    }
    ImGui::End();
}

} // namespace s8::overlay
