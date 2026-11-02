#include "feature_menu.h"
#include "menu_sprites.h"
#include "native_trait_editor.h"

#include "../../third_party/imgui_legacy/imgui.h"
#include "../domain/secondary_jewel_definitions.h"
#include "../domain/native_ui_layout.h"
#include "../domain/jewel_layout.h"
#include "../platform/windows/process_memory.h"
#include "../platform/windows/hook.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <array>
#include <atomic>
#include <span>
#include <algorithm>
#include <cmath>
#include <cfloat>
#include <string>
#include <string_view>
#include <utility>
#include <cstdio>

namespace s8::overlay {
namespace {

void open_officer_details(FeatureMenu& menu, std::uint16_t id);

menu_sprites::detail::SpriteRect atlas_rect(domain::native_ui::Rect rect) {
    return {static_cast<float>(rect.left), static_cast<float>(rect.top),
        static_cast<float>(rect.right), static_cast<float>(rect.bottom)};
}

void draw_native_card(const FeatureMenu& menu, ImVec2 minimum, ImVec2 maximum, float scale) {
    ImDrawList& draw = *ImGui::GetWindowDrawList();
    const menu_sprites::detail::SpriteRect region = atlas_rect(domain::native_ui::CARD);

    draw.AddRectFilled(ImVec2(minimum.x + 2 * scale, minimum.y + 3 * scale),
        ImVec2(maximum.x + 2 * scale, maximum.y + 3 * scale), IM_COL32(30, 24, 16, 42));
    if (menu_sprites::detail::available(menu.ui_atlases[0], region)) {
        draw.AddRectFilled(minimum, maximum, IM_COL32(237, 233, 220, 245));
        menu_sprites::detail::nine_slice(draw, menu.ui_atlases[0], region, minimum, maximum,
            10, 10, scale, IM_COL32(255, 255, 255, 64));
    } else {
        draw.AddRectFilled(minimum, maximum, IM_COL32(237, 233, 220, 220));
        draw.AddRect(minimum, maximum, IM_COL32(154, 139, 109, 190));
    }
}

void draw_screen_heading(const FeatureMenu& menu, domain::native_ui::Rect caption,
    const char* fallback, const char* description, float scale) {
    const ImGuiViewport& viewport = *ImGui::GetMainViewport();
    const ImVec2 origin = viewport.WorkPos;
    ImDrawList& draw = *ImGui::GetWindowDrawList();
    const menu_sprites::detail::SpriteRect title = atlas_rect(domain::native_ui::TITLE);

    if (menu_sprites::detail::available(menu.ui_atlases[0], title)) {
        menu_sprites::detail::image(draw, menu.ui_atlases[0], title, origin,
            ImVec2(origin.x + 520 * scale, origin.y + 90 * scale), IM_COL32_WHITE);
        menu_sprites::detail::image(draw, menu.ui_atlases[0], atlas_rect(caption),
            ImVec2(origin.x + 68 * scale, origin.y + 8 * scale),
            ImVec2(origin.x + 374 * scale, origin.y + 75 * scale), IM_COL32_WHITE);
    } else {
        draw.AddText(ImGui::GetFont(), 42 * scale, ImVec2(origin.x + 48 * scale, origin.y + 12 * scale),
            IM_COL32(45, 38, 26, 255), fallback);
    }

    const float description_size = 23 * scale;
    const float width = std::min(viewport.WorkSize.x * 0.52f, 1000 * scale);
    const ImVec2 strip(origin.x + (viewport.WorkSize.x - width) * 0.5f, origin.y + 16 * scale);

    menu_sprites::detail::image(draw, menu.ui_atlases[0], atlas_rect(domain::native_ui::NOTICE),
        strip, ImVec2(strip.x + width, strip.y + 40 * scale), IM_COL32(255, 255, 255, 210));
    const ImVec2 text = ImGui::GetFont()->CalcTextSizeA(description_size, width - 24 * scale, 0, description);

    draw.AddText(ImGui::GetFont(), description_size,
        ImVec2(strip.x + (width - text.x) * 0.5f, strip.y + (40 * scale - text.y) * 0.5f),
        IM_COL32(45, 38, 26, 255), description, nullptr, width - 24 * scale);
}

bool begin_editor_screen(FeatureMenu& menu, const char* id, bool& open, const char* description, bool protagonist = false) {
    const ImGuiViewport& viewport = *ImGui::GetMainViewport();
    const float scale = std::min(viewport.WorkSize.x / 1920.0f, viewport.WorkSize.y / 1080.0f);
    const ImVec2 origin = viewport.WorkPos;
    const ImVec2 size = viewport.WorkSize;

    ImGui::SetNextWindowPos(origin, ImGuiCond_Always);
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::SetNextWindowCollapsed(false, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings;

    ImGui::Begin(id, nullptr, flags);
    ImGui::GetWindowDrawList()->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y), IM_COL32(15, 12, 10, 150));
    draw_screen_heading(menu, domain::native_ui::EDIT, "편집", description, scale);

    const ImVec2 paper_min(origin.x + size.x * 0.06f, origin.y + size.y * 0.115f);
    const ImVec2 paper_max(origin.x + size.x * 0.94f, origin.y + size.y * 0.86f);

    menu_sprites::draw_panel(menu.ui_atlases, paper_min, paper_max, scale);
    ImVec2 body_min(paper_min.x + 38 * scale, paper_min.y + 28 * scale);
    ImVec2 body_max(paper_max.x - 38 * scale, paper_max.y - 28 * scale);
    if (protagonist || menu.officer_detail_open) {
    const ImVec2 subject_min = body_min;
    const ImVec2 subject_max(paper_min.x + (paper_max.x - paper_min.x) * 0.32f, body_max.y);
    body_min.x = subject_max.x + 20 * scale;
    draw_native_card(menu, subject_min, subject_max, scale);

    ImDrawList& subject = *ImGui::GetWindowDrawList();
    const float card_width = subject_max.x - subject_min.x;
    const domain::OfficerBasicValues* basic = protagonist ?
        (menu.snapshot.protagonist_basic ? &*menu.snapshot.protagonist_basic : nullptr) :
        (menu.snapshot.selected_officer ? &menu.snapshot.selected_officer->basic : nullptr);
    const std::optional<std::uint16_t> officer_id = protagonist ? menu.snapshot.protagonist_id :
        menu.snapshot.selected_officer ? std::optional<std::uint16_t>{menu.snapshot.selected_officer->id} : std::nullopt;
    const float title_height = 42 * scale;
    const ImVec2 title_min(subject_min.x + 12 * scale, subject_min.y + 12 * scale);
    const ImVec2 title_max(subject_max.x - 12 * scale, title_min.y + title_height);
    menu_sprites::detail::image(subject, menu.ui_atlases[0], atlas_rect(domain::native_ui::NAMEPLATE),
        title_min, title_max, IM_COL32_WHITE);
    const char* name = "무장 선택";
    if (officer_id && menu.officer_names) {
        const auto found = menu.officer_names->find(*officer_id);
        if (found != menu.officer_names->end()) name = found->second.c_str();
    }
    subject.AddText(ImGui::GetFont(), 26 * scale, ImVec2(title_min.x + 16 * scale, title_min.y + 7 * scale),
        IM_COL32(255, 241, 197, 255), name);
    const float portrait_edge = std::min(card_width - 48 * scale, (subject_max.y - title_max.y) * 0.47f);
    const ImVec2 portrait_min(subject_min.x + (card_width - portrait_edge) * 0.5f, title_max.y + 18 * scale);
    const ImVec2 portrait_max(portrait_min.x + portrait_edge, portrait_min.y + portrait_edge);
    if (basic && menu.portrait_face == (*basic)[13] && menu.portrait_texture.texture) {
        subject.AddImage(menu.portrait_texture.texture, portrait_min, portrait_max);
    } else {
        subject.AddText(ImVec2(portrait_min.x + 8 * scale, portrait_min.y + 20 * scale), IM_COL32(96, 78, 54, 255),
            basic ? (menu.portrait_face == (*basic)[13] ? "원본 초상화 없음" : "초상화 불러오는 중") : "무장을 선택해 주십시오");
    }
    float info_y = portrait_max.y + 16 * scale;
    if (!protagonist && menu.snapshot.selected_officer && menu.snapshot.selected_officer->city) {
        const std::string_view city = domain::CITY_NAMES[*menu.snapshot.selected_officer->city];
        subject.AddText(ImGui::GetFont(), 24 * scale, ImVec2(subject_min.x + 24 * scale, info_y),
            IM_COL32(100, 61, 29, 255), city.data(), city.data() + city.size());
        info_y += 32 * scale;
    }
    const float map_width = std::min(card_width - 40 * scale, (subject_max.y - info_y - 20 * scale) * 340 / 225);
    const ImVec2 map_min(subject_min.x + (card_width - map_width) * 0.5f, info_y);
    menu_sprites::detail::image(subject, menu.ui_atlases[0], atlas_rect(domain::native_ui::MAP), map_min,
        ImVec2(map_min.x + map_width, map_min.y + map_width * 225 / 340), IM_COL32_WHITE);

    }
    draw_native_card(menu, body_min, body_max, scale);
    const float edge = 88 * scale;

    ImGui::SetCursorScreenPos(ImVec2(origin.x + (size.x - edge) * 0.5f, origin.y + size.y * 0.93f - edge * 0.5f));
    if (menu_sprites::back_button(menu.ui_atlases, "뒤로##editor_back", edge, scale)) {
        if (!protagonist && menu.officer_relation_selection) {
            menu.officer_relation_selection.reset();
            menu.officer_detail_open = true;
        } else if (!protagonist && menu.officer_detail_open) {
            menu.officer_detail_open = false;
        } else {
            open = false;
        }
    }

    ImGui::SetCursorScreenPos(body_min);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20 * scale, 18 * scale));
    return ImGui::BeginChild("##editor_body", ImVec2(body_max.x - body_min.x, body_max.y - body_min.y),
        ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoBackground);
}

void end_editor_screen() {
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::End();
    ImGui::PopStyleVar(2);
}

void update_float_draft(MenuFloatDraft& draft, float incoming, bool reset) {

    if (reset) {
        draft = MenuFloatDraft{incoming};
        return;
    }
    // Completion can reach Present before the corresponding snapshot. Keep the
    // accepted value until its publication arrives, without losing an active drag.
    if (draft.submitted && incoming == *draft.submitted) {
        draft.submitted.reset();
    }
    if (!draft.editing && !draft.commit_requested && !draft.request && !draft.submitted) {
        draft.value = incoming;
    }
}

void complete_float_draft(MenuFloatDraft& draft, const services::CommandCompletion& completion, float current) {

    if (!draft.request || *draft.request != completion.request) {
        return;
    }
    draft.request.reset();
    if (!completion.result) {
        draft = MenuFloatDraft{current};
    }
}

const char* request_error_text(const domain::Error& error) {
    constexpr std::uint32_t NOT_READY_NATIVE_CODE = 21; // Windows ERROR_NOT_READY.

    switch (error.code) {
    case domain::ErrorCode::InvalidData:
        return "입력값 또는 대상 데이터를 확인할 수 없습니다.";
    case domain::ErrorCode::StaleEpoch:
        return "게임 상태가 바뀌어 요청이 취소되었습니다.";
    case domain::ErrorCode::UnsupportedVersion:
        return "현재 실행 환경에서 지원되지 않는 요청입니다.";
    case domain::ErrorCode::QueueFull:
        return "요청 대기열이 가득 차 접수하지 못했습니다.";
    case domain::ErrorCode::Cancelled:
        return "요청이 취소되었습니다.";
    case domain::ErrorCode::PlatformFailure:
        // ERROR_NOT_READY is returned when configuration or game data is not ready.
        return error.native_code == NOT_READY_NATIVE_CODE
            ? "요청 당시 데이터가 준비되지 않았습니다."
            : "요청 처리 중 시스템 오류가 발생했습니다.";
    default:
        return "요청을 처리하지 못했습니다.";
    }
}

void draw_previous_request_error(FeatureMenu& menu) {
    if (!menu.error) {
        return;
    }

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.68f, 0.18f, 0.10f, 1.0f));
    ImGui::TextWrapped("이전 요청 실패 · %s", request_error_text(*menu.error));
    ImGui::PopStyleColor();
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("현재 연결 상태와 별개인 이전 요청의 결과입니다.\n오류 코드: %d · 상세 코드: %u",
            static_cast<int>(menu.error->code), menu.error->native_code);
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("오류 닫기##previous_request_error")) {
        menu.error.reset();
    }
}

void draw_legacy_status(const domain::ConfigSnapshot& snapshot, domain::LegacyFeatureGroup group,
    bool same_line = true) {

    const domain::LegacyFeatureStatus& status = snapshot.legacy_features[static_cast<std::size_t>(group)];

    if (status.phase == domain::LegacyFeaturePhase::Disabled) {
        return;
    }
    if (same_line) {
        ImGui::SameLine();
    }
    switch (status.phase) {
    case domain::LegacyFeaturePhase::WaitingForPlayer:
        ImGui::TextDisabled("주인공 준비 후 3초 대기");
        break;
    case domain::LegacyFeaturePhase::Pending:
        ImGui::TextDisabled("적용 대기");
        break;
    case domain::LegacyFeaturePhase::Applied:
        ImGui::TextDisabled("적용됨");
        break;
    case domain::LegacyFeaturePhase::Retrying:
        ImGui::TextColored(ImVec4(0.68f, 0.18f, 0.10f, 1.0f), "적용 실패 · 재시도 대기");
        break;
    default:
        break;
    }
}

const char* special_assignment_phase_name(domain::SpecialAssignmentPhase phase) {

    switch (phase) {
    case domain::SpecialAssignmentPhase::Scanning: return "조건 판정 중";
    case domain::SpecialAssignmentPhase::Committing: return "판정 결과 적용 중";
    case domain::SpecialAssignmentPhase::Completed: return "완료";
    case domain::SpecialAssignmentPhase::Cancelled: return "취소됨 · 변경 없음";
    case domain::SpecialAssignmentPhase::SessionChanged: return "게임 변경으로 중단";
    default: return "대기";
    }
}

void draw_special_assignment_results(FeatureMenu& menu) {

    if (!menu.special_assignment_results_open) {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(700, 480), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("특수능력 자동 부여 결과###AutoSpecialResults", &menu.special_assignment_results_open)) {

        const domain::SpecialAssignmentSummary& summary = menu.snapshot.special_assignment;

        ImGui::Text("%s · %s", summary.annual ? "연간 자동 부여" : "수동 일괄 부여",
                    special_assignment_phase_name(summary.phase));
        ImGui::Text("판정 %u / 5102명 · 조건 충족 %u명 · 기존 능력 제외 %u명",
                    summary.scanned, summary.matched_officers, summary.excluded_existing);
        ImGui::Text("신규 부여 %u건 / %u명", summary.assigned, summary.assigned_officers);
        ImGui::Separator();

        if (menu.snapshot.special_assignment_results &&
            ImGui::BeginChild("##auto_special_results", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar)) {

            const std::vector<domain::SpecialAssignmentResult>& results = *menu.snapshot.special_assignment_results;
            ImGuiListClipper clipper;

            clipper.Begin(static_cast<int>(results.size()), ImGui::GetTextLineHeightWithSpacing() * 2.0f);

            while (clipper.Step()) {

                for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {

                    const domain::SpecialAssignmentResult& result = results[static_cast<std::size_t>(row)];
                    const domain::OfficerNames::const_iterator found = menu.officer_names
                        ? menu.officer_names->find(result.officer_id) : domain::OfficerNames::const_iterator{};
                    const char* name = menu.officer_names && found != menu.officer_names->end()
                        ? found->second.c_str() : "무장";
                    std::array<char, 1024> abilities{};
                    std::size_t used = 0;

                    for (std::size_t index = 0; index < domain::SPECIAL_ABILITY_COUNT; ++index) {

                        if ((result.abilities & (1u << index)) == 0) {
                            continue;
                        }

                        const std::string_view ability = domain::SPECIAL_ABILITY_NAMES[index];
                        const int written = std::snprintf(abilities.data() + used, abilities.size() - used,
                            "%s%.*s", used == 0 ? "" : ", ", static_cast<int>(ability.size()), ability.data());

                        if (written > 0) {
                            used += std::min(static_cast<std::size_t>(written), abilities.size() - used - 1);
                        }
                    }

                    ImGui::Text("%s (ID %u)", name, static_cast<unsigned>(result.officer_id));
                    ImGui::TextUnformatted(abilities.data());
                }
            }
        }

        if (menu.snapshot.special_assignment_results) {
            ImGui::EndChild();
        }
    }

    ImGui::End();
}

void draw_ai_growth_preview(const domain::AiGrowthPreview& preview) {

    if (!ImGui::CollapsingHeader("AI 자동성장 예상")) {
        return;
    }

    if (!preview.eligible) {
        ImGui::TextUnformatted("주인공, 무소속 무장, ID 1801 이상은 AI 성장 대상에서 제외됩니다.");
        return;
    }

    constexpr std::array<const char*, 5> STATS{"통솔", "무력", "지력", "정치", "매력"};

    for (std::size_t index = 0; index < domain::AI_GROWTH_CATEGORY_COUNT; ++index) {
        ImGui::Text("%s: %u + %d → %u / 전법 +%dLv", domain::AI_GROWTH_CATEGORY_NAMES[index].data(),
            static_cast<unsigned>(preview.aptitude_before[index]), preview.aptitude_gain[index],
            static_cast<unsigned>(preview.aptitude_after[index]), preview.tactic_level_ups[index]);
    }

    for (std::size_t index = 0; index < STATS.size(); ++index) {
        ImGui::Text("%s 누적 EXP %u · 현재 EXP %u → %u", STATS[index],
            static_cast<unsigned>(preview.cumulative_exp_before[index]),
            static_cast<unsigned>(preview.current_exp_before[index]),
            static_cast<unsigned>(preview.current_exp_after[index]));
    }

    ImGui::TextUnformatted("12월 예상치는 다음 해 1월 성장과 같은 선택 순서로 계산합니다.");
}

} // namespace

void update_feature_menu(FeatureMenu& menu, services::UiBridge& bridge) {
    menu.file_log_failed = bridge.file_log.failed();
    menu.file_log_dropped = bridge.file_log.dropped();

    menu.skill_counts = bridge.skill_counts_view();
    menu.officer_names = bridge.officer_names_view();
    menu.trait_effects = bridge.trait_effects_view();
    menu.custom_traits = bridge.custom_traits_view();
    if (auto result = bridge.take_officer_name_result()) {
        menu.officer_name_save_pending = false;
        if (*result == services::ConfigSaveStatus::Completed) {
            bridge.publish_notice("무장 이름을 저장했습니다.");
        } else if (*result == services::ConfigSaveStatus::Conflict) {
            bridge.publish_notice("무장 이름 파일이 변경되어 저장하지 못했습니다. 다시 시도해 주세요.");
        } else {
            bridge.publish_notice("무장 이름 저장에 실패했습니다.");
        }
    }
    if (menu.retry_save_requested) {
        bridge.request_config_retry();
        menu.retry_save_requested = false;
    }
    menu.save_notice = bridge.config_notice();
    while (auto completion = bridge.commands.take_completion()) {
        if (on_corps_deployment_completion(menu.corps_deployment, *completion)) {
            continue;
        }
        if (on_city_management_completion(menu.city_management, *completion)) {
            continue;
        }
        const auto batch = std::find(menu.officer_edits_in_flight.begin(), menu.officer_edits_in_flight.end(),
                                     completion->request);
        if (batch != menu.officer_edits_in_flight.end()) {
            menu.officer_edits_in_flight.erase(batch);
            ++menu.completed_officer_edits;
            if (!completion->result) {
                ++menu.failed_officer_edits;
                menu.officer_edit_error = completion->result.error();
            }
            continue;
        }
        if (menu.pending && completion->request == *menu.pending && completion->result && menu.pending_notice) {
            bridge.publish_notice(std::move(*menu.pending_notice));
        } else if (menu.pending && completion->request == *menu.pending && !completion->result && menu.pending_failure_notice) {
            bridge.publish_notice(std::move(*menu.pending_failure_notice));
        }
        on_feature_menu_completion(menu, *completion);
    }
    if (const auto snapshot = bridge.snapshot()) {
        const bool reset_float_drafts = menu.epoch != snapshot->epoch ||
            (!menu.ready && snapshot->config.config_loaded);

        if (snapshot->config.config_loaded && !menu.auto_load_applied) {
            menu.auto_load_applied = true;
            if (snapshot->config.features.get(domain::FeatureId::AutoLoadMenu)) {
                menu.open = true;
                menu.auto_load_collapse = true;
            }
        }
        if (!menu.ready && snapshot->config.config_loaded) {
            menu.cooldown_days_draft = snapshot->config.short_battle_cooldown_days;
            menu.skip_days_draft = snapshot->config.skip_days;
            menu.siege_heal_rate_draft = snapshot->config.siege_heal_rate;
        }
        menu.snapshot = snapshot->config;
        if (!menu.open || menu.collapsed) {
            menu.ui_scale_draft.editing = false;
            menu.notification_speed_draft.editing = false;
        }
        update_float_draft(menu.ui_scale_draft, menu.snapshot.ui_scale, reset_float_drafts);
        update_float_draft(menu.notification_speed_draft, menu.snapshot.notification_speed, reset_float_drafts);
        if (menu.special_assignment_generation_seen != menu.snapshot.special_assignment_generation) {
            menu.special_assignment_generation_seen = menu.snapshot.special_assignment_generation;

            if (menu.snapshot.special_assignment.annual && menu.snapshot.special_assignment.assigned != 0) {
                menu.special_assignment_results_open = true;
            }
        }
        menu.epoch = snapshot->epoch;
        menu.ready = snapshot->config.config_loaded;
        if (menu.selection_epoch != menu.epoch) {
            menu.selection_epoch = menu.epoch;
            menu.selected_officer_ids.fill(false);
            menu.officer_next_selection.reset();
            menu.officer_open_request.reset();
            menu.officer_open_refresh_requested = false;
            menu.officer_open_focus = false;
            menu.officer_detail_open = false;
            menu.officer_detail_tab = 0;
            menu.officer_relation_selection.reset();
            menu.officer_sort_dirty = true;
            menu.selected_color_draft = {};
            menu.protagonist_color_draft = {};
        }
    }
    bridge.consume_officer_roster(*menu.officer_roster, menu.epoch);
    if (const std::optional<domain::OfficerToolsView> tools = bridge.officer_tools.take()) {
        menu.officer_tools_view = *tools;
    }
    for (std::size_t index = 0; index < 4; ++index) {
        const std::optional<domain::OfficerNoticesSnapshot> notices = bridge.officer_notices.take();

        if (!notices) {
            break;
        }
        if (notices->view.epoch == menu.epoch) {
            update_officer_notice_panel(menu.officer_notices, *notices, menu.notifications,
                menu.officer_names ? &*menu.officer_names : nullptr, menu.snapshot.notification_speed,
                menu.snapshot.features.get(domain::FeatureId::MonitorRonin),
                menu.snapshot.features.get(domain::FeatureId::OfficerChangeNotify));
        }
    }
    if (!menu.pending && !menu.pending_officer_edits.empty()) {
        if (menu.officer_edit_epoch != menu.epoch) {
            menu.officer_edit_error = domain::Error{domain::ErrorCode::StaleEpoch};
            menu.next_officer_edit = menu.pending_officer_edits.size();
        }
        while (menu.next_officer_edit < menu.pending_officer_edits.size() &&
               menu.officer_edits_in_flight.size() < 16) {
            const domain::RequestId request{++menu.next_request};
            auto result = bridge.commands.push({request, menu.epoch,
                menu.pending_officer_edits[menu.next_officer_edit]});
            if (result) {
                ++menu.next_officer_edit;
                menu.officer_edits_in_flight.push_back(request);
            } else {
                if (result.error().code != domain::ErrorCode::QueueFull) {
                    menu.officer_edit_error = result.error();
                    menu.next_officer_edit = menu.pending_officer_edits.size();
                }
                break;
            }
        }
        if (menu.next_officer_edit == menu.pending_officer_edits.size() && menu.officer_edits_in_flight.empty()) {
            if (menu.officer_edit_error) {
                menu.error = menu.officer_edit_error;
            }
            bridge.publish_notice("무장 일괄 편집: " + std::to_string(menu.completed_officer_edits) +
                                  "건 처리, " + std::to_string(menu.failed_officer_edits) + "건 실패, " +
                                  std::to_string(menu.pending_officer_edits.size() - menu.completed_officer_edits) +
                                  "건 미처리");
            if (menu.officer_next_selection) {
                menu.officer_reselect_pending = true;
            }
            menu.pending_officer_edits.clear();
            menu.next_officer_edit = 0;
        }
    }

    update_trait_text_editor(menu.trait_text_editor, menu.snapshot.traits, bridge);
}

void on_feature_menu_completion(FeatureMenu& menu, const services::CommandCompletion& completion) {
    complete_float_draft(menu.ui_scale_draft, completion, menu.snapshot.ui_scale);
    complete_float_draft(menu.notification_speed_draft, completion, menu.snapshot.notification_speed);

    if (menu.pending && completion.request == *menu.pending) {
        if (completion.result) {
            menu.error.reset();
            if (menu.officer_next_selection) {
                menu.officer_reselect_pending = true;
            }
        } else {
            menu.error = completion.result.error();
            menu.cooldown_days_draft = menu.snapshot.short_battle_cooldown_days;
            menu.officer_next_selection.reset();
        }
        menu.pending.reset();
        menu.pending_notice.reset();
        menu.pending_failure_notice.reset();
    }
}

namespace {

struct FeatureItem {
    const char* label;
    domain::FeatureId id;
    const char* compact_label = nullptr;
};

// Borrows the Present-owned menu and command mailbox for this draw only.
struct FeatureMenuDraw {
    FeatureMenu& menu;
    services::CommandMailbox& mailbox;
    services::UiBridge& bridge;

    bool can_submit() const noexcept;
    bool submit(domain::CommandPayload payload);
    bool command_checkbox(const char* label, bool* value) const;
    bool command_button(const char* label, bool small = false) const;
    bool begin_popup_edit() const;
    void draw_feature(const FeatureItem& item);
    void draw_features(std::span<const FeatureItem> items);
    void draw_feature_grid(const char* id, std::span<const FeatureItem> items);
    void prepare_popup(float width, float height) const;
    void draw_range_choices(int& selected) const;
    void draw_resources();
    void draw_player_options();
    void draw_ai_growth();
    void draw_traits();
    void draw_officer_section(OfficerToolsSection section);
    void draw_overview();
    void draw_connection_status();
    void draw_tengi_settings();
    void draw_secondary_jewels();
    void draw_domestic();
    void draw_scenario();
    void draw_information();
    void draw_council();
    void draw_officer_edit();
    void draw_social();
    void draw_war();
    void draw_environment();
    void draw_tactics();
    void draw_stratagem();
    void draw_footer(float content_width);
    void draw_effect(const char* title, int& effect, int& power, int& duration);
};

void draw_menu_heading(const char* label) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.25f, 0.10f, 1.0f));
    ImGui::SeparatorText(label);
    ImGui::PopStyleColor();
}

bool FeatureMenuDraw::can_submit() const noexcept {
    return menu.ready && !menu.pending;
}

bool FeatureMenuDraw::submit(domain::CommandPayload payload) {
    if (!can_submit()) {
        return false;
    }

    const domain::RequestId request{++menu.next_request};
    const domain::Result<void> result = mailbox.push({request, menu.epoch, std::move(payload)});

    if (result) {
        menu.pending = request;
        return true;
    } else {
        menu.error = result.error();
        return false;
    }
}

bool FeatureMenuDraw::command_checkbox(const char* label, bool* value) const {
    ImGui::BeginDisabled(!can_submit());

    const bool changed = ImGui::Checkbox(label, value);

    ImGui::EndDisabled();
    return changed;
}

bool FeatureMenuDraw::command_button(const char* label, bool small) const {
    ImGui::BeginDisabled(!can_submit());

    const bool pressed = small ? ImGui::SmallButton(label) : ImGui::Button(label);

    ImGui::EndDisabled();
    return pressed;
}

bool FeatureMenuDraw::begin_popup_edit() const {
    ImGuiStorage& storage = *ImGui::GetStateStorage();
    const ImGuiID initialized_key = ImGui::GetID("##settings_draft_initialized");

    if (ImGui::IsWindowAppearing()) {
        storage.SetBool(initialized_key, false);
    }

    // Reuse one flag in this popup's window storage. A popup opened while a
    // request is pending must initialize from the snapshot after completion.
    const bool initialize = can_submit() && !storage.GetBool(initialized_key);

    if (initialize) {
        storage.SetBool(initialized_key, true);
    }
    if (!menu.ready) {
        ImGui::TextDisabled("설정을 불러온 뒤 편집할 수 있습니다.");
    } else if (menu.pending) {
        ImGui::TextDisabled("진행 중인 변경을 마친 뒤 편집할 수 있습니다.");
    }

    ImGui::BeginDisabled(!can_submit());
    return initialize;
}

void FeatureMenuDraw::draw_feature(const FeatureItem& item) {
    if (!menu.pk_editors_unlocked && (item.id == domain::FeatureId::AutoAffinityGrowth ||
        item.id == domain::FeatureId::ShowWidgetHero || item.id == domain::FeatureId::ShowWidgetAllOfficers)) return;
    const char* label = item.compact_label ? item.compact_label : item.label;
    bool enabled = menu.snapshot.features.get(item.id);

    ImGui::PushID(static_cast<int>(item.id));
    ImGui::BeginDisabled(item.id == domain::FeatureId::GovernorPrisonerConsumePrivilege &&
        !menu.snapshot.features.get(domain::FeatureId::GovernorPrisonerDisposal));

    if (command_checkbox(label, &enabled)) {
        if (submit(domain::SetFeature{item.id, enabled})) {
            switch (item.id) {
            case domain::FeatureId::InfiniteAp: case domain::FeatureId::FastJewel:
            case domain::FeatureId::InfiniteBanquet: case domain::FeatureId::InfiniteMediation:
            case domain::FeatureId::BigCity: case domain::FeatureId::BangmokCity:
            case domain::FeatureId::NonggyeongCity: case domain::FeatureId::SangeopCity:
            case domain::FeatureId::AttitudeHack: case domain::FeatureId::FastRelationship:
            case domain::FeatureId::MarriageApplied: case domain::FeatureId::TechZero:
            case domain::FeatureId::SelfHeal: case domain::FeatureId::Dongto:
            case domain::FeatureId::Celestial: case domain::FeatureId::Catapult:
            case domain::FeatureId::TerrainIgnore: case domain::FeatureId::DefBuilding:
            case domain::FeatureId::RoadBlock: case domain::FeatureId::RoadBlock2:
            case domain::FeatureId::SkillCondition: case domain::FeatureId::YumokCondition:
            case domain::FeatureId::SangbyeongCondition: case domain::FeatureId::AutoStatUp99:
            case domain::FeatureId::AllAggressive: case domain::FeatureId::ZeroInfamy:
            case domain::FeatureId::InfiniteTavernRequests: case domain::FeatureId::AutoFillSpecialties:
            case domain::FeatureId::AutoAffinityGrowth:
            case domain::FeatureId::BattleMapShuffle: case domain::FeatureId::FactionLordBonus:
            case domain::FeatureId::UndiscoveredToRonin: case domain::FeatureId::InfiniteGift:
            case domain::FeatureId::InfiniteTalk: case domain::FeatureId::InfiniteDuel:
            case domain::FeatureId::InfiniteDebate: case domain::FeatureId::Loyalty: case domain::FeatureId::HateCave:
            case domain::FeatureId::Resonance: case domain::FeatureId::Domestics:
            case domain::FeatureId::AutoLoadMenu: case domain::FeatureId::ShowWidgetNotif:
            case domain::FeatureId::ShowWidgetHero:
            case domain::FeatureId::ShowWidgetAllOfficers:
            case domain::FeatureId::FileLog:
                menu.pending_notice = std::string(item.label) + (enabled ? ": 활성화" : ": 비활성화");
                break;
            default:
                break;
            }
        }
    }

    ImGui::EndDisabled();

    if (item.compact_label && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", item.label);
    }

    if (item.id == domain::FeatureId::AutoAffinityGrowth && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("분기 평정월에 같은 세력과 도시의 AI 무장 친밀도를 상성과 흥미에 따라 올립니다.\n"
                          "이미 진행 중인 달에는 소급 적용하지 않습니다.");
    }

    if (item.id == domain::FeatureId::CouncilExecuteFreeOfficers && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("세력 도시의 재야 무장을 처단 대상 목록에 포함합니다.\n"
                         "후보를 나누어 갱신하고 처단 직전에 자격을 다시 확인합니다.");
    }
    if (item.id == domain::FeatureId::RulerTransferProposal && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("군주 직속 군사·도독의 평정에서 군주 이동 제안을 허용합니다.");
    }

    if (item.id == domain::FeatureId::FileLog) {

        if (menu.file_log_failed) {
            ImGui::TextColored(ImVec4(0.68f, 0.18f, 0.10f, 1.0f),
                "파일 로그 저장 실패: 옵션을 껐다 켜면 다시 시도합니다.");
        }

        if (menu.file_log_dropped != 0) {
            ImGui::TextDisabled("로그 대기열에서 누락된 알림: %llu",
                static_cast<unsigned long long>(menu.file_log_dropped));
        }
    }

    if (item.id == domain::FeatureId::TraitViewer) {
        ImGui::TextDisabled("%s", domain::trait_apply_status_text(menu.snapshot.traits.viewer));

        if (menu.snapshot.traits.viewer != domain::TraitApplyStatus::Applied &&
            menu.snapshot.traits.viewer != domain::TraitApplyStatus::Disabled &&
            menu.snapshot.traits.viewer != domain::TraitApplyStatus::Pending &&
            menu.snapshot.traits.viewer != domain::TraitApplyStatus::CallbackBusy) {
            if (command_button("기재 표시 다시 시도", true)) {
                submit(domain::SetFeature{domain::FeatureId::TraitViewer, enabled});
            }
        }
    }

    const std::optional<domain::NativeFeatureGroup> native_group = domain::native_feature_group(item.id);

    if (native_group) {
        const domain::NativeFeatureStatus& status = menu.snapshot.native_features[static_cast<std::size_t>(*native_group)];

        if (status.phase == domain::NativeFeaturePhase::Pending) {
            ImGui::TextDisabled("적용 대기 (현재 그룹 %s)", status.applied ? "활성" : "해제");
        } else if (status.phase == domain::NativeFeaturePhase::Applied) {
            ImGui::TextDisabled("%s", enabled ? "그룹 패치 적용됨" : "사용 설정 꺼짐");
            if (!enabled && ImGui::IsItemHovered()) {
                ImGui::SetTooltip("그룹 패치는 적용된 상태입니다.\n이 옵션의 사용 설정은 꺼져 있으며, 그룹 상태와 별개입니다.");
            }
        } else if (status.phase == domain::NativeFeaturePhase::Disabled) {
            ImGui::TextDisabled("해제됨");
        } else {
            ImGui::TextColored(ImVec4(0.68f, 0.18f, 0.10f, 1.0f), "%s: %s",
                status.phase == domain::NativeFeaturePhase::Uncertain ? "복구 확인 필요" : "적용 실패",
                domain::native_feature_error_text(status.error));

            ImGui::PushID(static_cast<int>(item.id));

            if (command_button("다시 시도", true)) {
                submit(domain::SetFeature{item.id, enabled});
            }

            ImGui::PopID();
        }
    }

    const std::optional<domain::LegacyFeatureGroup> legacy_group = domain::legacy_feature_group(item.id);

    if (legacy_group) {
        draw_legacy_status(menu.snapshot, *legacy_group, false);
    }

    ImGui::PopID();
}

void FeatureMenuDraw::draw_features(std::span<const FeatureItem> items) {
    for (const FeatureItem& item : items) {
        draw_feature(item);
    }
}

void FeatureMenuDraw::draw_feature_grid(const char* id, std::span<const FeatureItem> items) {
    constexpr ImGuiTableFlags FLAGS = ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings;

    if (ImGui::BeginTable(id, 2, FLAGS)) {
        for (const FeatureItem& item : items) {
            ImGui::TableNextColumn();
            draw_feature(item);
        }

        ImGui::EndTable();
    }
}

void FeatureMenuDraw::prepare_popup(float width, float height) const {
    const ImGuiViewport& viewport = *ImGui::GetMainViewport();
    const float margin = 16.0f * menu.dpi_scale;
    const ImVec2 maximum(std::max(1.0f, viewport.WorkSize.x - margin * 2.0f),
                        std::max(1.0f, viewport.WorkSize.y - margin * 2.0f));

    ImGui::SetNextWindowSize(ImVec2(std::min(width * menu.dpi_scale, maximum.x),
                                  std::min(height * menu.dpi_scale, maximum.y)), ImGuiCond_Appearing);

    ImGui::SetNextWindowSizeConstraints(ImVec2(std::min(320.0f * menu.dpi_scale, maximum.x),
                                             std::min(200.0f * menu.dpi_scale, maximum.y)), maximum);
}

void FeatureMenuDraw::draw_range_choices(int& selected) const {
    constexpr int RANGE_COUNT = 11;
    const float button_size = 64.0f * menu.dpi_scale;
    const ImGuiStyle& style = ImGui::GetStyle();
    const float right = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;

    for (int range = 1; range <= RANGE_COUNT; ++range) {
        const bool active = selected == range;
        void* texture = menu.range_textures[range - 1];
        std::array<char, 4> label{};

        std::snprintf(label.data(), label.size(), "%d", range);

        ImGui::PushID(range);

        if (active) {
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1, 1, 0, 1));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 3.0f * menu.dpi_scale);
        }

        ImGui::BeginGroup();

        const bool pressed = texture
            ? ImGui::ImageButton("##range", texture, ImVec2(button_size, button_size))
            : ImGui::Button(label.data(), ImVec2(button_size, button_size));

        if (pressed) {
            selected = range;
        }

        ImGui::Text(" %d", range);
        ImGui::EndGroup();

        if (active) {
            ImGui::PopStyleVar();
            ImGui::PopStyleColor();
        }

        ImGui::PopID();

        const float next_right = ImGui::GetItemRectMax().x + style.ItemSpacing.x +
            button_size + style.FramePadding.x * 2.0f;

        if (range != RANGE_COUNT && next_right <= right) {
            ImGui::SameLine();
        }
    }
}

void FeatureMenuDraw::draw_resources() {
    if (ImGui::CollapsingHeader("주인공 자원", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (menu.snapshot.player_resources) {
            const domain::PlayerResources& resources = *menu.snapshot.player_resources;

            if (menu.resource_epoch != menu.epoch || menu.resource_officer != resources.officer_id) {
                menu.resource_epoch = menu.epoch;
                menu.resource_officer = resources.officer_id;
                menu.resource_draft = resources.values;
            }
            if (ImGui::SmallButton("현재 자원 값 불러오기")) {
                menu.resource_draft = resources.values;
            }
            for (std::size_t index = 0; index < domain::PLAYER_RESOURCE_COUNT; ++index) {
                const domain::PlayerResource resource = static_cast<domain::PlayerResource>(index);

                ImGui::PushID(static_cast<int>(index));
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(domain::PLAYER_RESOURCE_LABELS[index]);
                ImGui::SameLine(170 * menu.dpi_scale);

                const float field_width = std::max(80.0f * menu.dpi_scale,
                    std::min(280.0f * menu.dpi_scale, ImGui::GetContentRegionAvail().x - 80 * menu.dpi_scale));
                const ImVec2 field_min = ImGui::GetCursorScreenPos();
                const ImVec2 field_max(field_min.x + field_width, field_min.y + ImGui::GetFrameHeight());

                menu_sprites::detail::nine_slice(*ImGui::GetWindowDrawList(), menu.ui_atlases[3],
                    menu_sprites::detail::COMPACT_BUTTONS[0], field_min, field_max,
                    10, 7, menu.dpi_scale, IM_COL32_WHITE);
                ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.48f, 0.36f, 0.15f, menu.ui_atlases[3].texture ? 0.0f : 1.0f));
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.97f, 0.85f, 1.0f));
                ImGui::SetNextItemWidth(field_width);
                ImGui::InputScalar("##resource_value", ImGuiDataType_U32, &menu.resource_draft[index]);
                ImGui::PopStyleColor(2);
                ImGui::SameLine();
                ImGui::BeginDisabled(!can_submit() ||
                    menu.resource_draft[index] > domain::player_resource_limit(resource));

                if (command_button("적용")) {
                    submit(domain::SetPlayerResource{resource, resources.officer_id,
                        resources.values[index], menu.resource_draft[index]});
                }
                ImGui::EndDisabled();
                ImGui::TextDisabled("현재 %u / 입력 한도 %u", resources.values[index], domain::player_resource_limit(resource));
                ImGui::PopID();
            }
        } else {
            ImGui::TextDisabled("주인공과 게임 자원을 확인한 뒤 편집할 수 있습니다.");
        }
    }
}

void FeatureMenuDraw::draw_ai_growth() {
    bool growth_enabled = menu.snapshot.ai_growth_settings.enabled;

    if (command_checkbox("AI 무장 자동성장", &growth_enabled)) {
        submit(domain::SetAiGrowthEnabled{growth_enabled});
    }

    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("12월에서 1월로 넘어간 뒤 평정에서 누적 EXP로 소양과 전법을 성장시킵니다.\n"
                         "게임의 능력 성장 속도도 함께 적용하며, 원래 양쪽 모두 '없음'이었다면 끌 때 복귀합니다.");
    }

    ImGui::BeginDisabled(!can_submit() || !growth_enabled);

    constexpr std::array<const char*, 3> GROWTH_SPEEDS{"느림", "보통", "빠름"};
    int growth_speed = domain::normalize_ai_growth_speed(menu.snapshot.ai_growth_settings.speed) - 1;

    if (ImGui::Combo("AI 성장 속도", &growth_speed, GROWTH_SPEEDS.data(), static_cast<int>(GROWTH_SPEEDS.size()))) {
        submit(domain::SetAiGrowthSpeed{growth_speed + 1});
    }

    bool annual_special = menu.snapshot.ai_growth_settings.annual_special_abilities;

    if (command_checkbox("성장 후 특수능력 자동 부여", &annual_special)) {
        submit(domain::SetAnnualSpecialAbilities{annual_special});
    }

    ImGui::EndDisabled();

    if (menu.snapshot.ai_growth_setting_failed) {
        ImGui::TextUnformatted("게임 능력 성장 속도를 적용하지 못했습니다. 다시 확인하는 중입니다.");
    }

    if (menu.snapshot.ai_growth_running) {
        ImGui::ProgressBar(static_cast<float>(menu.snapshot.ai_growth_cursor) /
            static_cast<float>(domain::AI_GROWTH_OFFICER_LIMIT), ImVec2(-1, 0), "AI 무장 성장 적용 중");
    }

    if (menu.snapshot.ai_growth_report) {

        const domain::AiGrowthReport& report = *menu.snapshot.ai_growth_report;

        ImGui::Text("%u년: 성장 %u명 · 누적 EXP 소비 %u명 · 적용 실패 %u명",
                    static_cast<unsigned>(report.year), report.changed, report.consumed, report.failed);
    }

    const domain::SpecialAssignmentSummary& assignment = menu.snapshot.special_assignment;
    const bool assignment_running = assignment.phase == domain::SpecialAssignmentPhase::Scanning ||
        assignment.phase == domain::SpecialAssignmentPhase::Committing;

    ImGui::BeginDisabled(!menu.skill_counts || assignment_running);

    if (command_button("모든 무장 특수능력 자동 판정")) {

        if (submit(domain::StartSpecialAbilityAssignment{})) {
            menu.special_assignment_results_open = true;
        }
    }

    ImGui::EndDisabled();
    ImGui::BeginDisabled(assignment.phase != domain::SpecialAssignmentPhase::Scanning);

    if (command_button("판정 취소")) {
        submit(domain::CancelSpecialAbilityAssignment{});
    }

    ImGui::EndDisabled();
    ImGui::SameLine();

    if (ImGui::Button("부여 결과 보기")) {
        menu.special_assignment_results_open = true;
    }

    if (assignment_running) {
        ImGui::Text("%s · 판정 %u / 5102명 · 신규 %u건",
            special_assignment_phase_name(assignment.phase), assignment.scanned, assignment.assigned);
    }

    ImGui::TextWrapped("기존 수동 능력은 유지합니다. 등갑군은 자동 부여에서 제외합니다.");
}

void FeatureMenuDraw::draw_tengi_settings() {
    if (ImGui::Button("전기 발생 · 기간 설정")) {
        ImGui::OpenPopup("전기 설정##editor");
    }

    prepare_popup(640.0f, 620.0f);

    if (ImGui::BeginPopupModal("전기 설정##editor", nullptr, ImGuiWindowFlags_NoSavedSettings)) {
        if (!menu.tengi_editing && !menu.pending) {
            menu.tengi_draft = menu.snapshot.tengi_settings;
        }

        bool editing = false;
        bool changed = false;

        if (!menu.snapshot.tengi_layout_supported) {
            ImGui::TextUnformatted("지원 게임 버전의 전기 데이터를 확인하는 중입니다.");
        } else if (menu.snapshot.tengi_ready) {
            ImGui::Text("현재 기운: %u", static_cast<unsigned>(menu.snapshot.tengi_momentum));
        }
        if (menu.snapshot.tengi_error != 0) {
            ImGui::Text("전기 기능 준비 실패 (%d)", menu.snapshot.tengi_error);
        }

        ImGui::TextUnformatted("기간 0: 기본값 / 재발생 -1: 기본값, 0: 제한 없음");
        ImGui::BeginDisabled(!can_submit());

        for (std::size_t index = 0; index < domain::TENGI_EVENT_COUNT; ++index) {
            domain::TengiEventSettings& event = menu.tengi_draft.events[index];

            ImGui::PushID(static_cast<int>(index));
            changed = ImGui::Checkbox(domain::TENGI_EVENT_NAMES[index].data(), &event.allowed) || changed;

            ImGui::SameLine();
            ImGui::SetNextItemWidth(72.0f * menu.dpi_scale);
            ImGui::InputInt("개월", &event.duration_months, 0, 0);
            editing = ImGui::IsItemActive() || editing;
            changed = ImGui::IsItemDeactivatedAfterEdit() || changed;

            ImGui::SameLine();
            ImGui::SetNextItemWidth(72.0f * menu.dpi_scale);
            ImGui::InputInt("재발생(년)", &event.cooldown_years, 0, 0);
            editing = ImGui::IsItemActive() || editing;
            changed = ImGui::IsItemDeactivatedAfterEdit() || changed;

            ImGui::PopID();
        }
        menu.tengi_editing = editing;

        if (changed) {
            menu.tengi_draft = domain::normalize_tengi_settings(menu.tengi_draft);
            submit(domain::SetTengiSettings{menu.tengi_draft});
        }
        ImGui::EndDisabled();

        if (ImGui::Button("닫기")) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void FeatureMenuDraw::draw_secondary_jewels() {
    if (!menu.secondary_jewel_window || !menu.open) return;
    const ImGuiViewport& viewport = *ImGui::GetMainViewport();
    const float scale = std::min(viewport.WorkSize.x / 1920.0f, viewport.WorkSize.y / 1080.0f);
    const ImVec2 origin = viewport.WorkPos;
    ImGui::SetNextWindowPos(origin, ImGuiCond_Always);
    ImGui::SetNextWindowSize(viewport.WorkSize, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("보주 편집###JewelTree", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings);
    ImDrawList& draw = *ImGui::GetWindowDrawList();
    draw.AddRectFilled(origin, ImVec2(origin.x + viewport.WorkSize.x, origin.y + viewport.WorkSize.y), IM_COL32(15, 12, 10, 140));
    draw_screen_heading(menu, domain::native_ui::EDIT, "보주", "보주를 눌러 개방 상태와 보조계통 적용을 변경합니다", scale);
    const ImVec2 minimum(origin.x + viewport.WorkSize.x * 0.06f, origin.y + viewport.WorkSize.y * 0.115f);
    const ImVec2 maximum(origin.x + viewport.WorkSize.x * 0.94f, origin.y + viewport.WorkSize.y * 0.86f);
    menu_sprites::draw_panel(menu.ui_atlases, minimum, maximum, scale);
    constexpr std::array<const char*, 11> categories{"공통", "재야", "두령", "일반", "군사", "태수", "도독", "군주", "악한", "무인", "문인"};
    ImGui::SetCursorScreenPos(ImVec2(minimum.x + 10 * scale, minimum.y + 12 * scale));
    ImGui::BeginChild("##jewel_categories", ImVec2(288 * scale, maximum.y - minimum.y - 24 * scale), false, ImGuiWindowFlags_NoBackground);
    const ImVec2 category_origin = ImGui::GetCursorScreenPos();
    for (int category = 0; category < static_cast<int>(categories.size()); ++category) {
        unsigned total = 0, opened = 0;
        for (const domain::SecondaryJewelDefinition& definition : domain::SECONDARY_JEWEL_DEFINITIONS) {
            if (static_cast<int>(definition.category) != category) continue;
            ++total;
            if (menu.snapshot.jewel_bitmap && ((*menu.snapshot.jewel_bitmap)[definition.raw_id >> 3] & (1u << (definition.raw_id & 7)))) ++opened;
        }
        const ImVec2 button_min(category_origin.x + 16 * scale, category_origin.y + (16 + category * 94) * scale);
        const ImVec2 button_max(button_min.x + 248 * scale, button_min.y + 72 * scale);
        ImGui::SetCursorScreenPos(button_min);
        const bool pressed = menu_sprites::detail::button_item(categories[category], ImVec2(248 * scale, 72 * scale));
        if (pressed) menu.jewel_category = category;
        const bool hovered = ImGui::IsItemHovered();
        const bool selected = menu.jewel_category == category;
        ImDrawList& sidebar = *ImGui::GetWindowDrawList();
        const unsigned state = hovered ? 1 : selected ? 2 : 0;
        menu_sprites::detail::nine_slice(sidebar, menu.ui_atlases[0],
            atlas_rect(domain::native_ui::JEWEL_CATEGORY_BUTTONS[state].atlas), button_min, button_max,
            12, 10, scale, IM_COL32_WHITE);
        if (hovered) {
            // Native thin chamfered focus rim from compact_controls (0x5889d015).
            menu_sprites::detail::nine_slice(sidebar, menu.ui_atlases[3], {1816, 0, 1932, 42},
                ImVec2(button_min.x - 2 * scale, button_min.y - 2 * scale),
                ImVec2(button_max.x + 2 * scale, button_max.y + 2 * scale), 10, 10, scale, IM_COL32_WHITE);
        }
        const ImU32 ink = hovered ? IM_COL32(66, 40, 10, 255) : IM_COL32(255, 249, 224, 255);
        const float name_size = 25 * scale;
        const ImVec2 name_extent = ImGui::GetFont()->CalcTextSizeA(name_size, 1000, 0, categories[category]);
        sidebar.AddText(ImGui::GetFont(), name_size, ImVec2(button_min.x + (248 * scale - name_extent.x) * 0.5f, button_min.y + 8 * scale), ink, categories[category]);
        std::array<char, 64> count_label{};
        if (menu.snapshot.jewel_bitmap) std::snprintf(count_label.data(), count_label.size(), "개방 %u / %u", opened, total);
        else std::snprintf(count_label.data(), count_label.size(), "개방 — / %u", total);
        const float count_size = 22 * scale;
        const ImVec2 count_extent = ImGui::GetFont()->CalcTextSizeA(count_size, 1000, 0, count_label.data());
        sidebar.AddText(ImGui::GetFont(), count_size, ImVec2(button_min.x + (248 * scale - count_extent.x) * 0.5f, button_min.y + 38 * scale), ink, count_label.data());
        const char* badge = category == 0 ? "공통" : category == 1 ? "신분" : category == 8 ? "보조" : nullptr;
        if (badge) {
            const ImVec2 badge_min(button_min.x - 10 * scale, button_min.y - 14 * scale);
            const ImVec2 badge_max(badge_min.x + 84 * scale, badge_min.y + 28 * scale);
            const bool auxiliary = category == 8;
            sidebar.AddRectFilled(badge_min, badge_max, auxiliary ? IM_COL32(238, 235, 221, 255) : IM_COL32(103, 61, 56, 255), 14 * scale);
            sidebar.AddRect(badge_min, badge_max, auxiliary ? IM_COL32(68, 58, 46, 255) : IM_COL32(178, 146, 130, 255), 14 * scale, 0, 1.5f * scale);
            const float badge_size = 22 * scale;
            const ImVec2 badge_extent = ImGui::GetFont()->CalcTextSizeA(badge_size, 1000, 0, badge);
            sidebar.AddText(ImGui::GetFont(), badge_size, ImVec2(badge_min.x + (84 * scale - badge_extent.x) * 0.5f, badge_min.y + 3 * scale),
                auxiliary ? IM_COL32(43, 36, 29, 255) : IM_COL32(255, 248, 225, 255), badge);
        }
    }
    ImGui::EndChild();
    const float side = std::min(maximum.y - minimum.y - 70 * scale, maximum.x - minimum.x - 624 * scale);
    const ImVec2 tree_min(minimum.x + 312 * scale, minimum.y + 28 * scale);
    const ImVec2 tree_max(tree_min.x + side, tree_min.y + side);
    menu_sprites::detail::image(draw, menu.ui_atlases[2], atlas_rect(domain::native_ui::JEWEL_BACKDROP), tree_min, tree_max, IM_COL32_WHITE);
    const float tree_scale = side / 1080;
    const unsigned category = static_cast<unsigned>(menu.jewel_category);
    const float left = static_cast<float>(2048 + category % 4 * 512);
    const float top = static_cast<float>(category / 4 * 512);
    menu_sprites::detail::image(draw, menu.ui_atlases[2], {left, top, left + 512, top + 512}, tree_min, tree_max, IM_COL32_WHITE);
    for (const domain::JewelLineLayout& line : domain::JEWEL_LINE_LAYOUTS) {
        if (line.category != category) continue;
        draw.AddLine(ImVec2(tree_min.x + line.x0 * tree_scale, tree_min.y + line.y0 * tree_scale),
            ImVec2(tree_min.x + line.x1 * tree_scale, tree_min.y + line.y1 * tree_scale), IM_COL32(137, 160, 157, 230), 4 * scale);
    }
    domain::SecondaryJewelMask mask = menu.snapshot.secondary_jewels.value_or(domain::SecondaryJewelMask{});
    for (const domain::JewelStartLayout& start : domain::JEWEL_START_LAYOUTS) {
        if (start.category != category) continue;
        const ImVec2 center(tree_min.x + start.x * tree_scale, tree_min.y + start.y * tree_scale);
        const float half = 20 * tree_scale;
        menu_sprites::detail::image(draw, menu.ui_atlases[0], atlas_rect(domain::native_ui::JEWEL_START.atlas),
            ImVec2(center.x - half, center.y - half), ImVec2(center.x + half, center.y + half), IM_COL32_WHITE);
    }
    for (std::size_t i = 0; i < domain::SECONDARY_JEWEL_DEFINITIONS.size(); ++i) {
        const domain::SecondaryJewelDefinition& definition = domain::SECONDARY_JEWEL_DEFINITIONS[i];
        if (static_cast<unsigned>(definition.category) != category) continue;
        const domain::JewelNodeLayout& node = domain::JEWEL_NODE_LAYOUTS[i];
        const ImVec2 center(tree_min.x + node.x * tree_scale, tree_min.y + node.y * tree_scale);
        const float radius = domain::JEWEL_ICON_DIAMETERS[node.icon] * tree_scale * 0.5f;
        const bool open = menu.snapshot.jewel_bitmap &&
            ((*menu.snapshot.jewel_bitmap)[definition.raw_id >> 3] & (1u << (definition.raw_id & 7))) != 0;
        const bool forced = domain::secondary_jewel_selected(mask, definition.id);
        // Frames 0..4 are closed; 5..9 are their native opened counterparts.
        const unsigned frame = node.icon % 5 + (open ? 5 : 0);
        const domain::native_ui::Rect ring = domain::native_ui::JEWEL_FRAMES[frame].atlas;
        const float frame_half = static_cast<float>(ring.right - ring.left) * tree_scale * 0.5f;

        menu_sprites::detail::image(draw, menu.ui_atlases[0], atlas_rect(ring),
            ImVec2(center.x - frame_half, center.y - frame_half),
            ImVec2(center.x + frame_half, center.y + frame_half), IM_COL32_WHITE);
        if (open) {
            const unsigned size = node.icon % 5 == 0 ? 0 : node.icon % 5 <= 2 ? 1 : 2;
            // Native gold jewels (0..2) and red jewels (15..17), without tinting.
            const domain::native_ui::Rect gem = domain::native_ui::JEWEL_GEMS[(forced ? 15 : 0) + size].atlas;
            const float half = static_cast<float>(gem.right - gem.left) * tree_scale * 0.5f;

            menu_sprites::detail::image(draw, menu.ui_atlases[0], atlas_rect(gem),
                ImVec2(center.x - half, center.y - half), ImVec2(center.x + half, center.y + half), IM_COL32_WHITE);
        }
        ImGui::PushID(static_cast<int>(definition.id));
        ImGui::SetCursorScreenPos(ImVec2(center.x - radius - 4 * scale, center.y - radius - 4 * scale));
        ImGui::BeginDisabled(!can_submit() || !menu.snapshot.jewel_bitmap);
        std::array<char, 40> node_label{};
        std::snprintf(node_label.data(), node_label.size(), "보주 %u##node", static_cast<unsigned>(definition.id));
        if (ImGui::InvisibleButton(node_label.data(), ImVec2((radius + 4 * scale) * 2, (radius + 4 * scale) * 2))) {
            menu.jewel_selected_id = definition.id;
            const int next = !open ? 1 : forced ? 0 : 2;
            const std::uint64_t bit = std::uint64_t{1} << (definition.id & 63);
            if (next == 2) mask.words[definition.id >> 6] |= bit;
            else mask.words[definition.id >> 6] &= ~bit;
            submit(domain::SetSecondaryJewels{mask, definition.raw_id, next != 0});
        }
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            menu.jewel_selected_id = definition.id;
            ImGui::SetTooltip("%s · %s", definition.name, open ? forced ? "보조계통 강제 적용" : "개방" : "미개방");
        }
        if (menu.jewel_selected_id == definition.id) draw.AddCircle(center, radius + 6 * scale, IM_COL32(255, 245, 202, 255), 0, 2 * scale);
        ImGui::PopID();
    }
    const ImVec2 info_min(tree_max.x + 24 * scale, minimum.y + 28 * scale);
    draw_native_card(menu, info_min, ImVec2(maximum.x - 24 * scale, maximum.y - 28 * scale), scale);
    ImGui::SetCursorScreenPos(ImVec2(info_min.x + 18 * scale, info_min.y + 20 * scale));
    ImGui::BeginChild("##jewel_description", ImVec2(maximum.x - info_min.x - 58 * scale, maximum.y - info_min.y - 70 * scale), false, ImGuiWindowFlags_NoBackground);
    ImGui::Text("%s 계통", categories[category]);
    ImGui::Separator();
    for (const domain::SecondaryJewelDefinition& definition : domain::SECONDARY_JEWEL_DEFINITIONS) {
        if (definition.id != menu.jewel_selected_id) continue;
        ImGui::TextWrapped("%s", definition.name);
        break;
    }
    ImGui::Spacing();
    ImGui::TextWrapped("클릭 순서\n미개방 → 개방 → 보조계통 강제 적용 → 미개방");
    ImGui::TextColored(ImVec4(0.65f, 0.44f, 0.02f, 1), "● 노랑: 개방");
    ImGui::TextColored(ImVec4(0.72f, 0.12f, 0.10f, 1), "● 빨강: 보조계통 강제 적용");
    ImGui::TextWrapped("강제 적용은 개방된 보주에만 적용됩니다.");
    if (menu.snapshot.all_jewels_open) ImGui::TextWrapped("보주를 개별 편집하면 전체 개방 자동 유지가 꺼집니다.");
    draw_menu_heading("적용 상태");
    const domain::LegacyFeatureStatus& jewel_status = menu.snapshot.legacy_features[
        static_cast<std::size_t>(domain::LegacyFeatureGroup::SecondaryJewels)];
    if (jewel_status.phase == domain::LegacyFeaturePhase::Disabled) {
        ImGui::TextDisabled("보조계통 강제 적용 선택 없음");
    } else {
        draw_legacy_status(menu.snapshot, domain::LegacyFeatureGroup::SecondaryJewels, false);
    }
    ImGui::BeginDisabled(!can_submit());
    const ImVec2 retry_size(ImGui::GetContentRegionAvail().x, 34 * menu.dpi_scale);
    if (menu_sprites::sprite_button(menu.ui_atlases, "보조계통 적용 재시도", retry_size)) {
        submit(domain::SetSecondaryJewels{mask});
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("현재 보조계통 선택으로 적용을 다시 요청합니다.");
    }
    if (menu.snapshot.all_jewels_open &&
        menu_sprites::sprite_button(menu.ui_atlases, "전체 개방 다시 적용", retry_size)) {
        submit(domain::SetAllJewelsOpen{true});
    }
    ImGui::EndDisabled();
    draw_previous_request_error(menu);
    ImGui::EndChild();
    const float edge = 88 * scale;
    ImGui::SetCursorScreenPos(ImVec2(origin.x + (viewport.WorkSize.x - edge) * 0.5f, origin.y + viewport.WorkSize.y * 0.93f - edge * 0.5f));
    if (menu_sprites::back_button(menu.ui_atlases, "뒤로##jewels", edge, scale)) menu.secondary_jewel_window = false;
    ImGui::End();
    ImGui::PopStyleVar();
}

void FeatureMenuDraw::draw_domestic() {
    if (!ImGui::BeginTable("##domestic_columns", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV)) return;
    ImGui::TableSetupColumn("도시 관리", ImGuiTableColumnFlags_WidthStretch, 0.60f);
    ImGui::TableSetupColumn("내정 설정", ImGuiTableColumnFlags_WidthStretch, 0.40f);
    ImGui::TableNextColumn();
    draw_city_management(menu.city_management, bridge, menu.epoch, menu.next_request,
        menu.officer_names ? &*menu.officer_names : nullptr);
    ImGui::TableNextColumn();
    const ImVec2 tool_size(std::min(ImGui::GetContentRegionAvail().x, 280.0f * menu.dpi_scale), 30.0f * menu.dpi_scale);
    if (menu.pk_editors_unlocked && menu_sprites::sprite_button(menu.ui_atlases, "명품 관리", tool_size)) menu.specialty_tools.open = true;
    if (menu_sprites::sprite_button(menu.ui_atlases, "군단 배치 / 도시 무장 이동", tool_size)) menu.corps_deployment.open = true;
    draw_menu_heading("내정");

    constexpr std::array FEATURES{
        FeatureItem{"행동력 무제한", domain::FeatureId::InfiniteAp},
        FeatureItem{"견문 시 민심 최대", domain::FeatureId::AttitudeHack},
        FeatureItem{"주점 청부 무한 유지", domain::FeatureId::InfiniteTavernRequests},
        FeatureItem{"평정 후 명품 자동 배분", domain::FeatureId::AutoFillSpecialties},
        FeatureItem{"CPU 임무에서 주인공 제외", domain::FeatureId::MissionCpuHeroExclusion},
        FeatureItem{"악명 0 유지", domain::FeatureId::ZeroInfamy},
    };

    draw_features(FEATURES);

    draw_feature({"내정 배율 적용", domain::FeatureId::Domestics});

    if (menu.snapshot.features.get(domain::FeatureId::Domestics)) {
        domain::DomesticsMultipliers multipliers = menu.snapshot.domestics;

        ImGui::BeginDisabled(!can_submit());

        const bool player_changed = ImGui::SliderFloat("플레이어 배율", &multipliers.player, 1.0f, 10.0f, "%.1fx");
        const bool force_changed = ImGui::SliderFloat("세력 배율", &multipliers.force, 1.0f, 10.0f, "%.1fx");

        if (player_changed || force_changed) {
            submit(domain::SetDomesticsMultipliers{multipliers});
        }
        ImGui::EndDisabled();
    }

    int reward_mode = static_cast<int>(menu.snapshot.domestic_reward_mode);
    constexpr const char* reward_modes[]{"OFF", "실적 200% 이상 + 3위 이내", "실적 200% 이상 + 등수 무관"};

    ImGui::BeginDisabled(!can_submit());

    if (ImGui::Combo("내정 포상 조건", &reward_mode, reward_modes, 3)) {
        const domain::DomesticRewardMode requested = static_cast<domain::DomesticRewardMode>(reward_mode);

        if (submit(domain::SetDomesticRewardMode{requested})) {
            menu.pending_notice = requested == domain::DomesticRewardMode::Off
                ? "내정 포상 조건 완화 OFF" : "내정 포상 조건 완화 적용 완료";
            menu.pending_failure_notice = "내정 포상 조건 완화 변경 실패";
        }
    }
    ImGui::EndDisabled();

    if (menu.snapshot.mission_appointment_limit && !menu.mission_appointment_limit_editing) {
        menu.mission_appointment_limit_draft = *menu.snapshot.mission_appointment_limit;
    }
    ImGui::BeginDisabled(!can_submit() || !menu.snapshot.mission_appointment_limit);
    ImGui::InputInt("임무 총 임명 한도", &menu.mission_appointment_limit_draft, 0, 0);
    menu.mission_appointment_limit_editing = ImGui::IsItemActive();

    if (ImGui::IsItemDeactivatedAfterEdit()) {
        if (submit(domain::SetMissionAppointmentLimit{menu.mission_appointment_limit_draft})) {
            menu.pending_notice = "임무 총 임명 한도 적용 완료";
        }
    }
    ImGui::EndDisabled();
    ImGui::BeginDisabled(!can_submit() || !menu.snapshot.mission_appointment_limit_managed);
    if (ImGui::Button("원본 복구##mission_appointment_limit")) {
        if (submit(domain::RestoreMissionAppointmentLimit{})) {
            menu.pending_notice = "임무 총 임명 한도 원본 복구 완료";
        }
    }
    ImGui::EndDisabled();

    if (ImGui::CollapsingHeader("게임 도시 편집기 표시")) {
        bool city_editor = menu.snapshot.tech_city_visibility_enabled;

        if (command_checkbox("도시 편집기에 기술도시 표시", &city_editor)) {
            if (submit(domain::SetTechCityVisibility{city_editor})) {
                menu.pending_notice = city_editor ? "게임 내 도시 편집기 기술도시 표시 ON 요청"
                                                   : "게임 내 도시 편집기 기술도시 표시 OFF 요청";
            }
        }
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::TextUnformatted("도시 편집기를 닫은 상태에서 전환한 뒤 다시 여세요.");
            ImGui::EndTooltip();
        }
        draw_legacy_status(menu.snapshot, domain::LegacyFeatureGroup::TechCityVisibility, false);
    }
    ImGui::EndTable();


}

void FeatureMenuDraw::draw_player_options() {
    draw_menu_heading("보주 · 추가 규칙");
    if (menu.pk_editors_unlocked) draw_resources();
    draw_feature({"보주 교체 무제한", domain::FeatureId::FastJewel});

    bool all_jewels_open = menu.snapshot.all_jewels_open;

    if (command_checkbox("보주 전체 개방", &all_jewels_open)) {
        if (submit(domain::SetAllJewelsOpen{all_jewels_open})) {
            menu.pending_notice = all_jewels_open ? "보주 전체 개방 ON" : "보주 전체 개방 OFF";
        }
    }
    if (ImGui::Button("보주 개방 · 보조계통")) {
        menu.secondary_jewel_window = true;
    }
    draw_legacy_status(menu.snapshot, domain::LegacyFeatureGroup::SecondaryJewels, false);
}

void FeatureMenuDraw::draw_scenario() {
    if (!ImGui::BeginTable("##scenario_columns", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV)) return;
    ImGui::TableNextColumn();
    draw_menu_heading("시나리오");

    constexpr std::array FEATURES{
        FeatureItem{"세력 군주 보너스", domain::FeatureId::FactionLordBonus, "군주 보너스"},
        FeatureItem{"시나리오 초기 설정 보정", domain::FeatureId::StartSetting, "시나리오 보정"},
        FeatureItem{"건녕 ↔ 교지 진로 방해 무시", domain::FeatureId::RoadBlock, "건녕↔교지"},
        FeatureItem{"교지 ↔ 회계 진로 방해 무시", domain::FeatureId::RoadBlock2, "교지↔회계"},
        FeatureItem{"세력 기술 초기화", domain::FeatureId::TechZero},
        FeatureItem{"미발견 무장을 재야로", domain::FeatureId::UndiscoveredToRonin},
    };

    draw_feature_grid("ScenarioOptions", FEATURES);

    draw_officer_section(OfficerToolsSection::Scenario);
    ImGui::TableNextColumn();
    draw_menu_heading("자녀 · 배우자");
    draw_officer_section(OfficerToolsSection::Family);
    ImGui::EndTable();
}

void FeatureMenuDraw::draw_information() {
    draw_menu_heading("정보");

    constexpr std::array FEATURES{
        FeatureItem{"무장 영입·사망 알림", domain::FeatureId::OfficerChangeNotify, "영입·사망 알림"},
        FeatureItem{"새로 등장한 재야 무장 알림", domain::FeatureId::MonitorRonin, "재야 등장 알림"},
        FeatureItem{"상성 수치 표시", domain::FeatureId::AffinityDisplay},
    };

    draw_feature_grid("InformationOptions", FEATURES);

    if (ImGui::Button("알림 내역")) {
        menu.notifications.show_history = true;
    }

}

void FeatureMenuDraw::draw_council() {
    draw_menu_heading("평정");

    constexpr std::array FEATURES{
        FeatureItem{"매 평정 새로운 전기", domain::FeatureId::InfTengi},
    };

    draw_features(FEATURES);

    draw_tengi_settings();

    constexpr std::array COUNCIL_FEATURES{
        FeatureItem{"만병 습득 조건 해제", domain::FeatureId::SkillCondition},
        FeatureItem{"유목기병 습득 조건 해제", domain::FeatureId::YumokCondition},
        FeatureItem{"상병 습득 조건 해제", domain::FeatureId::SangbyeongCondition},
        FeatureItem{"단절 영토 무장 이동 제한", domain::FeatureId::IsolatedTerritoryMovement},
        FeatureItem{"이동 후 평정 계속", domain::FeatureId::CouncilContinueAfterMove},
        FeatureItem{"세력 도시 재야 무장 처단", domain::FeatureId::CouncilExecuteFreeOfficers},
        FeatureItem{"부하의 군주 이동 제안", domain::FeatureId::RulerTransferProposal},
    };

    draw_features(COUNCIL_FEATURES);
}

void FeatureMenuDraw::draw_officer_edit() {
    draw_menu_heading("무장 · 보주");
    if (!menu.pk_editors_unlocked) {
        ImGui::TextWrapped("주인공 자원과 무장 능력·전법·특기·기재·관계·명성은 게임의 기능 → 편집에서 변경합니다.");
        draw_player_options();
        draw_information();
        return;
    }
    const ImVec2 tool_size(std::min(ImGui::GetContentRegionAvail().x, 280.0f * menu.dpi_scale), 30.0f * menu.dpi_scale);
    if (menu_sprites::sprite_button(menu.ui_atlases, "모든 무장 편집", tool_size)) {
        menu.officer_list_open = true;
    }
    ImGui::TextDisabled("공용 무장 데이터가 준비되면 메인화면에서도 편집할 수 있습니다.");

    ImGui::BeginDisabled(!menu.snapshot.protagonist_id);
    if (menu_sprites::sprite_button(menu.ui_atlases, "주인공 무장 편집", tool_size)) {
        menu.protagonist_detail = true;
    }
    ImGui::EndDisabled();
    if (!menu.snapshot.protagonist_id) {
        ImGui::TextDisabled("주인공 편집은 시나리오 연결 후 사용할 수 있습니다.");
    }
    draw_player_options();
    draw_information();
}

void FeatureMenuDraw::draw_officer_section(OfficerToolsSection section) {
    if (!menu.officer_tools_view || menu.officer_tools_view->epoch != menu.epoch) {
        ImGui::TextUnformatted("현재 게임의 무장 정보를 기다리는 중입니다.");
        return;
    }
    ImGui::PushID(static_cast<int>(section));
    const std::optional<domain::OfficerToolsRequest> command = draw_officer_tools_panel(menu.officer_tools,
        *menu.officer_tools_view, menu.officer_names ? &*menu.officer_names : nullptr, menu.dpi_scale,
        section, menu.ui_atlases, can_submit() && menu.pending_officer_edits.empty());
    ImGui::PopID();
    if (menu.officer_tools.open_officer) {
        open_officer_details(menu, *menu.officer_tools.open_officer);
        menu.officer_tools.open_officer.reset();
    }
    if (command) submit(*command);
}

void FeatureMenuDraw::draw_traits() {
    if (ImGui::BeginTabBar("##trait_growth_sections")) {
        if (ImGui::BeginTabItem("기재 편집")) {
            draw_trait_text_editor_content(menu, mailbox);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("기재 성장 · 랜덤 배정")) {
            draw_feature({"기재 3칸 표시", domain::FeatureId::TraitViewer});
            draw_feature({"능력치 한계 돌파", domain::FeatureId::AutoStatUp99});
            draw_ai_growth();
            ImGui::Separator();
            draw_officer_section(OfficerToolsSection::RandomTraits);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("전법 · 특기 일괄 편집")) {
            draw_officer_section(OfficerToolsSection::Training);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}

void FeatureMenuDraw::draw_social() {
    draw_menu_heading("교류");

    constexpr std::array FEATURES{
        FeatureItem{"선물 무제한", domain::FeatureId::InfiniteGift},
        FeatureItem{"즉시 경애 맺기", domain::FeatureId::FastRelationship, "즉시 경애"},
        FeatureItem{"담화 무제한", domain::FeatureId::InfiniteTalk},
        FeatureItem{"혐오/상극 무시 경애", domain::FeatureId::HateCave, "혐오/상극 무시"},
        FeatureItem{"대련 무제한", domain::FeatureId::InfiniteDuel},
        FeatureItem{"무조건 공명 발생", domain::FeatureId::Resonance, "무조건 공명"},
        FeatureItem{"설전 무제한", domain::FeatureId::InfiniteDebate},
        FeatureItem{"무장 충성도 100", domain::FeatureId::Loyalty, "충성도 100"},
        FeatureItem{"중재 무제한", domain::FeatureId::InfiniteMediation},
        FeatureItem{"AI 친밀도 가속", domain::FeatureId::AutoAffinityGrowth},
        FeatureItem{"연회 무제한", domain::FeatureId::InfiniteBanquet},
        FeatureItem{"결혼 무제한", domain::FeatureId::MarriageApplied},
    };

    draw_feature_grid("SocialOptions", FEATURES);
}

void FeatureMenuDraw::draw_war() {
    draw_menu_heading("전쟁 관련");

    constexpr std::array FEATURES{
        FeatureItem{"모든 무장 성향 적극", domain::FeatureId::AllAggressive, "성향 적극"},
        FeatureItem{"AI 전쟁 개선", domain::FeatureId::AiWarImprove},
        FeatureItem{"평정 전투맵 셔플", domain::FeatureId::BattleMapShuffle, "전투맵 셔플"},
        FeatureItem{"태수 포로 처분", domain::FeatureId::GovernorPrisonerDisposal},
        FeatureItem{"포로 포획 관리", domain::FeatureId::PrisonerCaptureManagement},
        FeatureItem{"태수 포로 처분 시 특권 소모", domain::FeatureId::GovernorPrisonerConsumePrivilege, "특권 소모"},
        FeatureItem{"수비측 증원 배치 개선", domain::FeatureId::ReinforcementDefenderPlacement, "수비 증원 배치"},
    };

    draw_feature_grid("WarOptions", FEATURES);

    bool troop = menu.snapshot.troop_count_combat_enabled;

    if (command_checkbox("병력수 공방 반영", &troop)) {
        submit(domain::SetTroopCountCombat{troop});
    }
    draw_legacy_status(menu.snapshot, domain::LegacyFeatureGroup::TroopCountCombat, false);
    bool reinforcement = menu.snapshot.reinforcement_arrival_enabled;

    if (command_checkbox("원군 도착 턴 즉시 행동", &reinforcement)) {
        submit(domain::SetReinforcementArrival{reinforcement});
    }
    draw_legacy_status(menu.snapshot, domain::LegacyFeatureGroup::ReinforcementArrival, false);

    bool cooldown = menu.snapshot.short_battle_cooldown_enabled;

    if (command_checkbox("단기접전 대기일수 변경", &cooldown)) {
        submit(domain::SetShortBattleCooldown{cooldown, menu.cooldown_days_draft});
    }
    draw_legacy_status(menu.snapshot, domain::LegacyFeatureGroup::ShortBattleCooldown, false);

    ImGui::BeginDisabled(!can_submit());

    if (ImGui::InputInt("대기일수", &menu.cooldown_days_draft, 0, 0,
                        ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_EnterReturnsTrue)) {
        submit(domain::SetShortBattleCooldown{cooldown, menu.cooldown_days_draft});
    }
    ImGui::EndDisabled();

    if (ImGui::Button("전투 환경 설정")) {
        ImGui::OpenPopup("전투 환경##settings");
    }

    prepare_popup(560.0f, 640.0f);

    if (ImGui::BeginPopupModal("전투 환경##settings", nullptr, ImGuiWindowFlags_NoSavedSettings)) {
        draw_environment();

        if (ImGui::Button("닫기")) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    if (ImGui::Button("전법 · 방어 건물 설정")) {
        ImGui::OpenPopup("전법 · 방어 건물##settings");
    }

    prepare_popup(980.0f, 780.0f);

    if (ImGui::BeginPopupModal("전법 · 방어 건물##settings", nullptr, ImGuiWindowFlags_NoSavedSettings)) {
        if (begin_popup_edit()) {
            menu.self_heal_draft = menu.snapshot.self_heal_settings;
            menu.dongto_draft = menu.snapshot.dongto_settings;
            menu.celestial_draft = menu.snapshot.celestial_settings;
            menu.catapult_draft = menu.snapshot.catapult_settings;
            menu.terrain_tactics_draft = menu.snapshot.terrain_tactics_settings;
            menu.def_building_draft = menu.snapshot.def_building_settings;
        }
        ImGui::TextUnformatted("탭에서 연속 편집할 수 있습니다. 설정 적용 후에도 이 창은 유지됩니다.");
        draw_tactics();

        if (ImGui::Button("닫기")) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void FeatureMenuDraw::draw_environment() {
    constexpr std::array FEATURES{
        FeatureItem{"전투 상시 15일", domain::FeatureId::DateAlways15},
        FeatureItem{"전투 일자 유동 조절", domain::FeatureId::DateDynamic},
        FeatureItem{"전투 시작 경과일 스킵", domain::FeatureId::SkipDaysEnabled},
        FeatureItem{"날씨 간단 변경", domain::FeatureId::WeatherSkillSimple},
        FeatureItem{"날씨 대폭 변경", domain::FeatureId::WeatherSkillComplex},
        FeatureItem{"지형 능력 공방만", domain::FeatureId::TerrainAbilityAtkDef},
        FeatureItem{"지형 능력 전체", domain::FeatureId::TerrainAbilityAll},
        FeatureItem{"공성전 성 주변 1칸", domain::FeatureId::SiegeWarfare},
        FeatureItem{"공성전 성 주변 2칸", domain::FeatureId::SiegeWarfare2},
    };

    draw_features(FEATURES);

    ImGui::BeginDisabled(!can_submit());

    if (ImGui::InputInt("시작 경과일", &menu.skip_days_draft, 0, 0)) {
        menu.skip_days_draft = std::max(menu.skip_days_draft, 0);
        submit(domain::SetSkipDays{menu.skip_days_draft});
    }
    ImGui::EndDisabled();
    ImGui::BeginDisabled(!can_submit());

    if (ImGui::InputInt("수비 부대 회복 비율 (%)", &menu.siege_heal_rate_draft, 1, 5)) {
        menu.siege_heal_rate_draft = std::clamp(menu.siege_heal_rate_draft, 0, 100);
        submit(domain::SetSiegeHealRate{menu.siege_heal_rate_draft});
    }
    ImGui::EndDisabled();
    if (ImGui::Button("지형 보너스 설정")) {
        ImGui::OpenPopup("지형 보너스 설정##editor");
    }
    prepare_popup(560.0f, 620.0f);

    if (ImGui::BeginPopupModal("지형 보너스 설정##editor", nullptr, ImGuiWindowFlags_NoSavedSettings)) {
        if (begin_popup_edit()) {
            menu.terrain_bonus_draft = menu.snapshot.terrain_bonus;
        }

        constexpr std::array<const char*, 12> TROOPS{"경보병", "중보병", "정예보병", "만병", "경기병", "중기병",
                                                       "정예기병", "유목기병", "궁병", "노병", "정예궁병", "연노병"};
        constexpr std::array<const char*, 11> TERRAINS{"가도", "평지", "황무지", "초원", "삼림", "습지",
                                                        "산지", "여울", "다리", "도하", "암석"};
        for (std::size_t troop = 0; troop < TROOPS.size(); ++troop) {
            ImGui::PushID(static_cast<int>(troop));

            if (ImGui::TreeNode(TROOPS[troop])) {
                for (std::size_t column = 0; column < TERRAINS.size(); ++column) {
                    std::int16_t& entry = menu.terrain_bonus_draft.values[troop + 1][domain::TERRAIN_BONUS_COLUMNS[column]];
                    int value = entry;

                    if (ImGui::InputInt(TERRAINS[column], &value, 0, 0)) {
                        entry = static_cast<std::int16_t>(std::clamp(value, -30, 30));
                    }
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
        if (ImGui::Button("기본값 복원")) {
            menu.terrain_bonus_draft = {};
            menu.terrain_bonus_draft.values[11][16] = 0;
            menu.terrain_bonus_draft.values[11][17] = 10;
        }
        ImGui::SameLine();

        if (command_button("저장 후 닫기")) {
            if (submit(domain::SetTerrainBonus{menu.terrain_bonus_draft})) {
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();

        if (ImGui::Button("취소")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void FeatureMenuDraw::draw_tactics() {
    if (!ImGui::BeginTabBar("##tactics_building_tabs")) {
        return;
    }
    if (ImGui::BeginTabItem("치료")) {
        ImGui::PushID("치료");
        draw_feature({"치료", domain::FeatureId::SelfHeal});

        domain::SelfHealSettings& draft = menu.self_heal_draft;

        for (int i = 0; i < 3; ++i) {
            ImGui::PushID(i);
            ImGui::Text("Level %d", i + 1);
            ImGui::Checkbox("자기치료 활성화", &draft.self[i]);

            if (ImGui::InputInt("치료량", &draft.amount[i])) {
                draft.amount[i] = std::clamp(draft.amount[i], 0, 32767);
            }
            if (i > 0) {
                ImGui::TextUnformatted("치료 범위 선택:");
                draw_range_choices(draft.range[i - 1]);
            }
            ImGui::PopID();
        }
        if (command_button("설정 적용")) {
            submit(domain::SetSelfHealSettings{draft});
        }
        ImGui::EndDisabled();
        ImGui::SameLine();

        if (ImGui::Button("현재 설정 다시 읽기")) {
            menu.self_heal_draft = menu.snapshot.self_heal_settings;
        }
        ImGui::PopID();
        ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("동토")) {
        ImGui::PushID("동토");
        draw_feature({"동토", domain::FeatureId::Dongto});

        domain::DongtoSettings& draft = menu.dongto_draft;

        for (int i = 0; i < 3; ++i) {
            ImGui::PushID(i);
            ImGui::Text("Level %d", i + 1);
            ImGui::SliderInt("성공 확률 (%)", &draft.probability[i], 0, 100);
            ImGui::InputInt("직접 입력##확률", &draft.probability[i], 0);
            ImGui::SliderInt("상태이상 확률 (%)", &draft.status_probability[i], 0, 100);
            ImGui::InputInt("직접 입력##상태", &draft.status_probability[i], 0);
            ImGui::PopID();
        }
        if (command_button("설정 적용")) {
            submit(domain::SetDongtoSettings{draft});
        }
        ImGui::EndDisabled();
        ImGui::SameLine();

        if (ImGui::Button("현재 설정 다시 읽기")) {
            menu.dongto_draft = menu.snapshot.dongto_settings;
        }
        ImGui::PopID();
        ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("천계")) {
        ImGui::PushID("천계");
        draw_feature({"천계", domain::FeatureId::Celestial});

        domain::CelestialSettings& draft = menu.celestial_draft;

        for (int i = 0; i < 3; ++i) {
            ImGui::PushID(i);
            ImGui::Text("Level %d", i + 1);
            ImGui::SliderInt("성공 확률 (%)", &draft.probability[i], 0, 100);
            ImGui::InputInt("직접 입력##확률", &draft.probability[i], 0);
            if (ImGui::InputInt("회복량", &draft.amount[i], 0)) {
                draft.amount[i] = std::max(draft.amount[i], 0);
            }
            draw_range_choices(draft.range[i]);

            ImGui::PopID();
        }
        if (command_button("설정 적용")) {
            submit(domain::SetCelestialSettings{draft});
        }
        ImGui::EndDisabled();
        ImGui::SameLine();

        if (ImGui::Button("현재 설정 다시 읽기")) {
            menu.celestial_draft = menu.snapshot.celestial_settings;
        }
        ImGui::PopID();
        ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("투석")) {
        ImGui::PushID("투석");
        draw_feature({"투석", domain::FeatureId::Catapult});

        domain::CatapultSettings& draft = menu.catapult_draft;

        ImGui::SliderInt("최소 사거리", &draft.min_range, 1, 20);
        ImGui::InputInt("직접 입력##최소", &draft.min_range, 0);
        ImGui::SliderInt("최대 사거리", &draft.max_range, 1, 20);
        ImGui::InputInt("직접 입력##최대", &draft.max_range, 0);

        if (ImGui::InputInt("부대 위력", &draft.amount, 0)) {
            draft.amount = std::max(draft.amount, 0);
        }
        ImGui::TextUnformatted("범위 선택:");
        draw_range_choices(draft.range);

        if (command_button("설정 적용")) {
            submit(domain::SetCatapultSettings{draft});
        }
        ImGui::EndDisabled();
        ImGui::SameLine();

        if (ImGui::Button("현재 설정 다시 읽기")) {
            menu.catapult_draft = menu.snapshot.catapult_settings;
        }
        ImGui::PopID();
        ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("격류/낙석")) {
        ImGui::PushID("격류/낙석");
        draw_feature({"격류/낙석", domain::FeatureId::TerrainIgnore});

        domain::TerrainTacticsSettings& draft = menu.terrain_tactics_draft;

        for (int kind = 0; kind < 2; ++kind) {
            ImGui::PushID(kind);
            ImGui::TextUnformatted(kind == 0 ? "격류" : "낙석");
            std::array<int, 3>& amounts = kind == 0 ? draft.water_amount : draft.stone_amount;
            std::array<int, 3>& ranges = kind == 0 ? draft.water_range : draft.stone_range;

            for (int level = 0; level < 3; ++level) {
                ImGui::PushID(level);
                ImGui::Text("Level %d", level + 1);
                ImGui::InputInt("위력", &amounts[level], 0);
                draw_range_choices(ranges[level]);

                ImGui::PopID();
            }
            ImGui::PopID();
        }
        if (command_button("설정 적용")) {
            submit(domain::SetTerrainTacticsSettings{draft});
        }
        ImGui::EndDisabled();
        ImGui::SameLine();

        if (ImGui::Button("현재 설정 다시 읽기")) {
            menu.terrain_tactics_draft = menu.snapshot.terrain_tactics_settings;
        }
        ImGui::PopID();
        ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("방어 건물")) {
        ImGui::PushID("방어 건물");
        draw_feature({"방어 건물 강화", domain::FeatureId::DefBuilding});

        constexpr std::array<const char*, 5> BUILDINGS{"도시", "관문", "망루", "성벽 투석대", "봉화대"};
        std::array<int, 19>& values = menu.def_building_draft.values;

        if (ImGui::BeginTable("##def_building_values", 6, ImGuiTableFlags_BordersInner | ImGuiTableFlags_RowBg |
                ImGuiTableFlags_SizingStretchSame)) {
            ImGui::TableSetupColumn("건물", ImGuiTableColumnFlags_WidthFixed, 140.0f * menu.dpi_scale);
            for (const char* label : {"내구도", "사거리", "공격력", "시야", "전의"}) {
                ImGui::TableSetupColumn(label);
            }
            ImGui::TableHeadersRow();

            for (int building = 0; building < 5; ++building) {
                ImGui::PushID(building);
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(BUILDINGS[building]);
                for (int column = 1; column < 6; ++column) {
                    ImGui::TableNextColumn();
                    const int field = building == 4 ? (column == 1 ? 0 : column == 4 ? 2 : column == 5 ? 1 : -1)
                        : column <= 4 ? column - 1 : -1;

                    if (field < 0) {
                        ImGui::TextDisabled("—");
                        continue;
                    }
                    ImGui::PushID(field);
                    ImGui::SetNextItemWidth(-1);
                    ImGui::InputInt("##value", &values[building * 4 + field], 0);
                    ImGui::PopID();
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        if (command_button("설정 적용")) {
            submit(domain::SetDefBuildingSettings{menu.def_building_draft});
        }
        ImGui::EndDisabled();
        ImGui::SameLine();

        if (ImGui::Button("현재 설정 다시 읽기")) {
            menu.def_building_draft = menu.snapshot.def_building_settings;
        }
        ImGui::PopID();
        ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
}

void FeatureMenuDraw::draw_effect(const char* title, int& effect, int& power, int& duration) {
    constexpr int EFFECT_CODES[]{0, 3, 10, 11, 12};
    constexpr const char* effects[]{"없음", "상태이상", "사기 증감", "직접 데미지", "화계"};
    ImGui::PushID(title);
    ImGui::TextUnformatted(title);
    int selected = 0;

    for (int i = 0; i < IM_ARRAYSIZE(EFFECT_CODES); ++i) {
        if (EFFECT_CODES[i] == effect) {
            selected = i;
            break;
        }
    }
    if (ImGui::Combo("효과", &selected, effects, IM_ARRAYSIZE(effects))) {
        effect = EFFECT_CODES[selected];

        if (effect == 0) {
            power = 0;
            duration = 0;
        } else if (effect == 3) {
            if (power < 1 || power > 3) {
                power = 1;
            }
            duration = std::max(duration, 0);
        } else if (effect == 12) {
            if (power < 0 || power > 100) {
                power = 100;
            }
            duration = 0;
        } else {
            duration = 0;
        }
    }
    if (effect == 3) {
        constexpr const char* statuses[]{"저지", "혼란", "공황"};
        int status = std::clamp(power - 1, 0, 2);

        if (ImGui::Combo("상태", &status, statuses, IM_ARRAYSIZE(statuses))) {
            power = status + 1;
        }
        if (ImGui::InputInt("지속일", &duration, 1, 3)) {
            duration = std::clamp(duration, 0, 30);
        }
    } else if (effect == 10) {
        if (ImGui::InputInt("사기 증감", &power, 5, 10)) {
            power = std::clamp(power, -100, 100);
        }
    } else if (effect == 11) {
        if (ImGui::InputInt("위력", &power, 50, 100)) {
            power = std::clamp(power, 0, 10000);
        }
    } else if (effect == 12) {
        if (ImGui::InputInt("발동 확률(%)", &power, 5, 10)) {
            power = std::clamp(power, 0, 100);
        }
    } else {
        ImGui::TextDisabled("추가 설정 없음");
    }
    ImGui::PopID();
}

void FeatureMenuDraw::draw_stratagem() {
    draw_menu_heading("책략");

    constexpr std::array FEATURES{
        FeatureItem{"공격측 책략 게이지 최대", domain::FeatureId::MaxAttackStratagemGauge},
        FeatureItem{"수비측 책략 게이지 최대", domain::FeatureId::MaxDefenseStratagemGauge},
    };

    draw_features(FEATURES);

    bool stratagem = menu.snapshot.stratagem5_enabled;

    if (command_checkbox("5번 책략 활성화", &stratagem)) {
        submit(domain::SetStratagem5Enabled{stratagem});
    }
    ImGui::SameLine();

    if (ImGui::Button("5번 책략 설정")) {
        ImGui::OpenPopup("5번 책략 설정##editor");
    }
    prepare_popup(560.0f, 620.0f);

    if (ImGui::BeginPopupModal("5번 책략 설정##editor", nullptr, ImGuiWindowFlags_NoSavedSettings)) {
        if (begin_popup_edit()) {
            menu.stratagem5_draft = menu.snapshot.stratagem5_settings;
        }

        domain::Stratagem5Settings& draft = menu.stratagem5_draft;

        ImGui::TextDisabled("책략 ID 5/5/5와 사용횟수 1회는 고정합니다.");
        constexpr const char* targets[]{"아군", "적군", "피아불문"};
        int target = draft.target - 1;

        if (ImGui::Combo("대상", &target, targets, IM_ARRAYSIZE(targets))) {
            draft.target = target + 1;
        }
        draw_effect("효과 1", draft.effect1, draft.power1, draft.duration1);
        draw_effect("효과 2", draft.effect2, draft.power2, draft.duration2);

        if (ImGui::InputInt("범위", &draft.range)) {
            draft.range = std::clamp(draft.range, 1, 100);
        }
        if (ImGui::InputInt("추가 병력 회복", &draft.heal_amount, 100, 500)) {
            draft.heal_amount = std::clamp(draft.heal_amount, 0, 65535);
        }
        if (ImGui::Button("현재 기본값 복원")) {
            draft = domain::Stratagem5Settings{};
        }
        if (command_button("적용")) {
            if (submit(domain::SetStratagem5Settings{draft})) {
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();

        if (ImGui::Button("취소")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void FeatureMenuDraw::draw_footer(float content_width) {
    constexpr ImGuiTableFlags FLAGS = ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings;
    std::array<char, 32> slider_epoch_id{};

    // Preserve per-session slider identities and draft/acknowledgement handling.
    std::snprintf(slider_epoch_id.data(), slider_epoch_id.size(), "scalar-%llu",
        static_cast<unsigned long long>(menu.epoch.value));

    if (ImGui::BeginTable("FooterPrimaryLayout", 2, FLAGS, ImVec2(content_width, 0))) {
        ImGui::TableSetupColumn("Widgets", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("Notifications", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        draw_menu_heading("아이콘 우클릭 바로가기");
        draw_feature({"주인공", domain::FeatureId::ShowWidgetHero});
        draw_feature({"모든 무장", domain::FeatureId::ShowWidgetAllOfficers});
        draw_feature({"알림확인", domain::FeatureId::ShowWidgetNotif});

        ImGui::TableSetColumnIndex(1);
        draw_menu_heading("알림");

        ImGui::PushID(slider_epoch_id.data());
        ImGui::BeginDisabled(!menu.ready);
        const float notification_slider_width = ImGui::GetContentRegionAvail().x -
            ImGui::CalcTextSize("알림 표시 시간").x - ImGui::GetStyle().ItemInnerSpacing.x;
        ImGui::SetNextItemWidth(std::clamp(notification_slider_width, 60.0f * menu.dpi_scale, 150.0f * menu.dpi_scale));
        float notification_seconds = notification_duration_seconds(menu.notification_speed_draft.value);
        if (ImGui::SliderFloat("알림 표시 시간", &notification_seconds, 2.5f, 15.0f, "%.1f초", ImGuiSliderFlags_AlwaysClamp)) {
            // Keep the existing configuration key and request acknowledgement path.
            menu.notification_speed_draft.value = 600.0f / notification_seconds;
        }
        menu.notification_speed_draft.editing = ImGui::IsItemActive();
        menu.notification_speed_draft.commit_requested = ImGui::IsItemDeactivatedAfterEdit() || menu.notification_speed_draft.commit_requested;
        ImGui::EndDisabled();

        if (menu.notification_speed_draft.commit_requested && !menu.notification_speed_draft.editing && can_submit()) {
            if (submit(domain::SetNotificationSpeed{menu.notification_speed_draft.value})) {
                menu.notification_speed_draft.commit_requested = false;
                menu.notification_speed_draft.request = menu.pending;
                menu.notification_speed_draft.submitted = menu.notification_speed_draft.value;
            } else {
                menu.notification_speed_draft = MenuFloatDraft{menu.snapshot.notification_speed};
            }
        }
        ImGui::PopID();
        if (ImGui::Button("알림 비우기")) {
            clear_active_notifications(menu.notifications);
        }

        ImGui::EndTable();
    }

    if (ImGui::BeginTable("FooterSecondaryLayout", 2, FLAGS, ImVec2(content_width, 0))) {
        ImGui::TableSetupColumn("Left", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("Right", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        draw_menu_heading("로그");
        draw_feature({"알림 파일 로그", domain::FeatureId::FileLog});

        ImGui::TableSetColumnIndex(1);
        draw_menu_heading("게임 속도");
        draw_feature({"게임 배속", domain::FeatureId::SpeedHack});
        ImGui::PushItemWidth(85.0f * menu.dpi_scale);

        if (menu.snapshot.features.get(domain::FeatureId::SpeedHack)) {
            constexpr std::array<const char*, 9> SPEED_LABELS{
                "1.0x", "1.5x", "2.0x", "2.5x", "3.0x", "3.5x", "4.0x", "4.5x", "5.0x"};
            int selected_speed = static_cast<int>((menu.snapshot.speed_multiplier - 1.0f) * 2.0f);

            ImGui::BeginDisabled(!can_submit());

            if (ImGui::Combo("배속 배율", &selected_speed, SPEED_LABELS.data(), static_cast<int>(SPEED_LABELS.size()))) {
                submit(domain::SetSpeedMultiplier{1.0f + static_cast<float>(selected_speed) * 0.5f});
            }
            ImGui::EndDisabled();
        }
        ImGui::PopItemWidth();
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        draw_menu_heading("UI");

        ImGui::PushID(slider_epoch_id.data());
        ImGui::BeginDisabled(!menu.ready);
        ImGui::SetNextItemWidth(110.0f * menu.dpi_scale);
        ImGui::SliderFloat("UI 배율", &menu.ui_scale_draft.value, 0.5f, 3.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp);
        menu.ui_scale_draft.editing = ImGui::IsItemActive();
        menu.ui_scale_draft.commit_requested = ImGui::IsItemDeactivatedAfterEdit() || menu.ui_scale_draft.commit_requested;
        ImGui::EndDisabled();

        if (menu.ui_scale_draft.commit_requested && !menu.ui_scale_draft.editing && can_submit()) {
            if (submit(domain::SetUiScale{menu.ui_scale_draft.value})) {
                menu.ui_scale_draft.commit_requested = false;
                menu.ui_scale_draft.request = menu.pending;
                menu.ui_scale_draft.submitted = menu.ui_scale_draft.value;
            } else {
                menu.ui_scale_draft = MenuFloatDraft{menu.snapshot.ui_scale};
            }
        }
        ImGui::PopID();
        ImGui::TableSetColumnIndex(1);
        draw_menu_heading("시작");
        draw_feature({"시작 시 편집 아이콘 표시", domain::FeatureId::AutoLoadMenu});
        ImGui::EndTable();
    }

    draw_menu_heading("추가 편집기");
    if (ImGui::Checkbox("PK 중복 편집기 잠금 해제", &menu.pk_editors_unlocked) && !menu.pk_editors_unlocked) {
        menu.officer_list_open = false;
        menu.protagonist_detail = false;
        menu.specialty_tools.open = false;
    }
    menu.city_management.manual_editing = menu.pk_editors_unlocked;
    menu.officer_tools.manual_editing = menu.pk_editors_unlocked;
    ImGui::TextWrapped("기본값은 게임 내 편집을 사용합니다. 시나리오에서 편집을 제한한 경우 이 항목으로 자체 편집기를 표시할 수 있습니다.");
    draw_menu_heading("도구");
    if (menu_sprites::sprite_button(menu.ui_atlases, "메모장 / 메모리 도구",
            ImVec2(std::min(content_width, 280.0f * menu.dpi_scale), 30.0f * menu.dpi_scale))) {
        menu.debug_tools.open = true;
    }
}

struct MenuPageInfo {
    const char* label;
    const char* description;
    int icon;
};

constexpr std::array<MenuPageInfo, static_cast<std::size_t>(MenuPage::Count)> MENU_PAGES{{
    {"개요", "연결 상태와 적용 결과를 확인합니다.", 0},
    {"무장", "무장 목록, 주인공 자원과 관계 정보를 편집합니다.", 1},
    {"기재 · 성장", "기재 문구, 일괄 전법과 성장 설정을 관리합니다.", 2},
    {"도시 · 내정", "도시와 군단, 명품 및 내정 설정을 관리합니다.", 4},
    {"평정 · 교류", "평정 규칙과 무장 간 교류를 설정합니다.", 0},
    {"전투", "전투 규칙, 환경과 책략을 설정합니다.", 2},
    {"시나리오", "시나리오 보정, 가족과 무장 등장 상태를 관리합니다.", 3},
    {"환경설정", "위젯, 알림, 배속과 화면 배율을 조정합니다.", 5},
}};

void select_menu_page(FeatureMenu& menu, MenuPage page) {
    menu.home_screen = false;
    if (menu.page != page) {
        menu.page = page;
        // Hidden sliders no longer get a deactivation frame. Release their
        // edit locks while retaining any submitted request/acknowledgement.
        menu.ui_scale_draft.editing = false;
        menu.notification_speed_draft.editing = false;
        menu.mission_appointment_limit_editing = false;
    }
}

enum class ReportState { Off, Applied, Waiting, Attention, External };

struct FeatureReport {
    const char* label;
    const char* detail;
    MenuPage page;
    ReportState state;
};

MenuPage native_report_page(domain::NativeFeatureGroup group) {
    switch (group) {
    case domain::NativeFeatureGroup::MissionCpuExclusion: return MenuPage::Domestic;
    case domain::NativeFeatureGroup::AffinityDisplay: return MenuPage::Officers;
    case domain::NativeFeatureGroup::IsolatedTerritory:
    case domain::NativeFeatureGroup::CouncilContinue:
    case domain::NativeFeatureGroup::CouncilExecuteFree:
    case domain::NativeFeatureGroup::RulerTransferProposal: return MenuPage::CouncilSocial;
    default: return MenuPage::Battle;
    }
}

MenuPage legacy_report_page(domain::LegacyFeatureGroup group) {
    switch (group) {
    case domain::LegacyFeatureGroup::SecondaryJewels: return MenuPage::Officers;
    case domain::LegacyFeatureGroup::SpeedClock: return MenuPage::Settings;
    case domain::LegacyFeatureGroup::FactionLordBonus:
    case domain::LegacyFeatureGroup::RoadBlock:
    case domain::LegacyFeatureGroup::RoadBlock2: return MenuPage::Scenario;
    case domain::LegacyFeatureGroup::ReinforcementArrival:
    case domain::LegacyFeatureGroup::ShortBattleCooldown:
    case domain::LegacyFeatureGroup::TroopCountCombat: return MenuPage::Battle;
    case domain::LegacyFeatureGroup::TechCityVisibility:
    case domain::LegacyFeatureGroup::Domestics:
    case domain::LegacyFeatureGroup::BigCity:
    case domain::LegacyFeatureGroup::GrazingCity:
    case domain::LegacyFeatureGroup::FarmingCity:
    case domain::LegacyFeatureGroup::CommerceCity:
    case domain::LegacyFeatureGroup::Attitude: return MenuPage::Domestic;
    default: return MenuPage::CouncilSocial;
    }
}

ReportState trait_report_state(domain::TraitApplyStatus status) {
    switch (status) {
    case domain::TraitApplyStatus::Disabled: return ReportState::Off;
    case domain::TraitApplyStatus::Applied: return ReportState::Applied;
    case domain::TraitApplyStatus::ExternalRuntime: return ReportState::External;
    case domain::TraitApplyStatus::Pending:
    case domain::TraitApplyStatus::WaitingForTable:
    case domain::TraitApplyStatus::CallbackBusy:
    case domain::TraitApplyStatus::LiveObjects: return ReportState::Waiting;
    default: return ReportState::Attention;
    }
}

auto feature_reports(const domain::ConfigSnapshot& snapshot) {
    std::array<FeatureReport, domain::NATIVE_FEATURE_GROUP_COUNT + domain::LEGACY_FEATURE_GROUP_COUNT + 3> reports{};
    std::size_t cursor = 0;
    for (std::size_t index = 0; index < snapshot.native_features.size(); ++index) {
        const auto group = static_cast<domain::NativeFeatureGroup>(index);
        const auto& status = snapshot.native_features[index];
        ReportState state = ReportState::Off;
        const char* detail = "사용 설정 꺼짐";
        switch (status.phase) {
        case domain::NativeFeaturePhase::Applied:
            state = ReportState::Applied; detail = "공유 패치 그룹 적용 확인"; break;
        case domain::NativeFeaturePhase::Pending:
            state = ReportState::Waiting; detail = "변경 요청 적용 대기"; break;
        case domain::NativeFeaturePhase::Failed:
        case domain::NativeFeaturePhase::Uncertain:
            state = ReportState::Attention; detail = domain::native_feature_error_text(status.error); break;
        default: break;
        }
        reports[cursor++] = {domain::native_feature_label(group), detail, native_report_page(group), state};
    }
    for (std::size_t index = 0; index < snapshot.legacy_features.size(); ++index) {
        const auto group = static_cast<domain::LegacyFeatureGroup>(index);
        const auto& status = snapshot.legacy_features[index];
        ReportState state = ReportState::Off;
        const char* detail = "사용 설정 꺼짐";
        switch (status.phase) {
        case domain::LegacyFeaturePhase::Applied:
            state = ReportState::Applied; detail = "기능군 적용 확인"; break;
        case domain::LegacyFeaturePhase::WaitingForPlayer:
            state = ReportState::Waiting; detail = "주인공 준비 후 3초 대기"; break;
        case domain::LegacyFeaturePhase::Pending:
            state = ReportState::Waiting; detail = "변경 요청 적용 대기"; break;
        case domain::LegacyFeaturePhase::Retrying:
            state = ReportState::Attention; detail = "적용 실패 · 자동 재시도 대기"; break;
        default: break;
        }
        reports[cursor++] = {domain::legacy_feature_label(group), detail, legacy_report_page(group), state};
    }
    reports[cursor++] = {"기재 3칸 표시", domain::trait_apply_status_text(snapshot.traits.viewer),
        MenuPage::Traits, trait_report_state(snapshot.traits.viewer)};
    reports[cursor++] = {"확장 기재", domain::trait_apply_status_text(snapshot.traits.catalog),
        MenuPage::Traits, trait_report_state(snapshot.traits.catalog)};
    reports[cursor] = {"기본 기재 이름·설명", domain::trait_apply_status_text(snapshot.traits.legacy_text),
        MenuPage::Traits, trait_report_state(snapshot.traits.legacy_text)};
    return reports;
}

void FeatureMenuDraw::draw_connection_status() {
    using Tone = menu_sprites::StatusTone;
    constexpr ImGuiTableFlags flags = ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings;
    if (!ImGui::BeginTable("##connection_cards", 3, flags)) {
        return;
    }
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextDisabled("설정 불러오기");
    menu_sprites::status_badge(menu.ui_atlases, menu.ready ? "준비됨" : "대기 중",
        menu.ready ? Tone::Good : Tone::Waiting, menu.dpi_scale);
    ImGui::TextWrapped(menu.ready ? "설정 파일 또는 기본값을 불러왔습니다." : "파일과 설정 서비스의 준비를 기다립니다.");

    ImGui::TableSetColumnIndex(1);
    ImGui::TextDisabled("시나리오 · 주인공");
    const auto status = menu.snapshot.game_memory_status;
    const bool connected = status == domain::GameMemoryStatus::Ready;
    const bool failed = status == domain::GameMemoryStatus::ModuleReadFailed ||
        status == domain::GameMemoryStatus::RootReadFailed || status == domain::GameMemoryStatus::PlayerReadFailed;
    menu_sprites::status_badge(menu.ui_atlases, connected ? "연결됨" : failed ? "조회 재시도" : "연결 대기",
        connected ? Tone::Good : failed ? Tone::Error : Tone::Waiting, menu.dpi_scale);
    if (connected && menu.snapshot.protagonist_id) {
        ImGui::Text("주인공 ID %u", *menu.snapshot.protagonist_id);
    } else if (failed) {
        ImGui::TextWrapped("%s", status == domain::GameMemoryStatus::ModuleReadFailed ? "실행 파일 메모리 읽기 실패" :
            status == domain::GameMemoryStatus::RootReadFailed ? "게임 기본 정보 읽기 실패" : "주인공 정보 읽기 실패");
    } else if (status == domain::GameMemoryStatus::Searching) {
        const auto total = menu.snapshot.game_memory_image_size;
        const double progress = total == 0 ? 0.0 :
            100.0 * static_cast<double>(menu.snapshot.game_memory_scan_cursor) / static_cast<double>(total);
        ImGui::Text("게임 주소 검색 · %.0f%%", progress);
    } else if (status == domain::GameMemoryStatus::SearchWaiting) {
        ImGui::TextWrapped("게임 준비 후 자동 재검색 · 조회 %llu회",
            static_cast<unsigned long long>(menu.snapshot.game_memory_scan_passes));
    } else {
        ImGui::TextWrapped("공용 데이터 편집은 시나리오 연결과 별도로 사용할 수 있습니다.");
    }

    ImGui::TableSetColumnIndex(2);
    if (!menu.pk_editors_unlocked) {
        ImGui::TextDisabled("게임 기본 편집");
        ImGui::TextWrapped("기능 → 편집에서 주인공·무장·도시·세력·명품을 변경합니다.");
        ImGui::EndTable();
        return;
    }
    ImGui::TextDisabled("공용 무장 목록");
    const auto& roster = *menu.officer_roster;
    const bool current = roster.epoch == menu.epoch;
    const bool loaded = current && roster.available && !roster.loading;
    const bool loading = current && roster.loading;
    menu_sprites::status_badge(menu.ui_atlases, loading ? "조회 중" : loaded ? "조회 완료" : "미조회",
        loading ? Tone::Waiting : loaded && roster.entry_count ? Tone::Good : Tone::Muted, menu.dpi_scale);
    if (loading || loaded) {
        ImGui::Text("%zu명 %s", roster.entry_count, loading ? "수신" : "조회됨");
    } else {
        ImGui::TextWrapped("'모든 무장 편집'을 열어 조회합니다.");
    }
    ImGui::EndTable();
}

void FeatureMenuDraw::draw_overview() {
    using Tone = menu_sprites::StatusTone;
    draw_connection_status();
    draw_menu_heading("바로 열기");
    const int columns = menu.pk_editors_unlocked && ImGui::GetContentRegionAvail().x >= 620.0f * menu.dpi_scale ? 3 : 2;
    if (ImGui::BeginTable("##quick_editors", columns, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings)) {
        if (menu.pk_editors_unlocked) {
            ImGui::TableNextColumn();
            if (menu_sprites::sprite_button(menu.ui_atlases, "모든 무장 편집##quick",
                    ImVec2(ImGui::GetContentRegionAvail().x, 30.0f * menu.dpi_scale), false, 1)) menu.officer_list_open = true;
        }
        ImGui::TableNextColumn();
        if (menu_sprites::sprite_button(menu.ui_atlases, "기재 효과·이름·설명 편집##quick",
                ImVec2(ImGui::GetContentRegionAvail().x, 30.0f * menu.dpi_scale), false, 2)) {
            menu.page = MenuPage::Traits;
            menu.home_screen = false;
        }
        ImGui::TableNextColumn();
        if (menu_sprites::sprite_button(menu.ui_atlases, "도시 관리##quick",
                ImVec2(ImGui::GetContentRegionAvail().x, 30.0f * menu.dpi_scale), false, 4)) {
            menu.page = MenuPage::Domestic;
            menu.home_screen = false;
        }
        ImGui::EndTable();
    }
    draw_menu_heading("적용 현황");
    ImGui::TextWrapped("체크는 사용 설정입니다. 실제 적용 상태는 아래 기능군별로 확인하며, 항목을 누르면 설정으로 이동합니다.");
    if (!menu.ready) {
        ImGui::TextWrapped("설정을 불러온 뒤 적용 현황을 확인할 수 있습니다.");
        return;
    }

    const auto reports = feature_reports(menu.snapshot);
    std::array<std::size_t, 5> counts{};
    for (const auto& report : reports) {
        ++counts[static_cast<std::size_t>(report.state)];
    }
    const int status_columns = ImGui::GetContentRegionAvail().x >= 560.0f * menu.dpi_scale ? 4 : 2;
    if (ImGui::BeginTable("##application_totals", status_columns, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings)) {
        constexpr std::array STATES{ReportState::Applied, ReportState::Waiting, ReportState::Attention, ReportState::External};
        constexpr std::array LABELS{"적용 확인", "대기", "확인 필요", "외부 관리"};
        constexpr std::array TONES{Tone::Good, Tone::Waiting, Tone::Error, Tone::Muted};
        for (std::size_t i = 0; i < STATES.size(); ++i) {
            ImGui::TableNextColumn();
            std::array<char, 64> label{};
            std::snprintf(label.data(), label.size(), "%s %zu", LABELS[i], counts[static_cast<std::size_t>(STATES[i])]);
            menu_sprites::status_badge(menu.ui_atlases, label.data(), TONES[i], menu.dpi_scale);
        }
        ImGui::EndTable();
    }
    ImGui::RadioButton("대기·오류·외부", &menu.status_filter, 0);
    ImGui::SameLine();
    ImGui::RadioButton("꺼짐 제외", &menu.status_filter, 1);
    ImGui::SameLine();
    ImGui::RadioButton("전체", &menu.status_filter, 2);
    constexpr ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
        ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings;
    std::size_t shown = 0;
    if (ImGui::BeginTable("##feature_reports", 3, flags)) {
        ImGui::TableSetupColumn("기능군", ImGuiTableColumnFlags_WidthStretch, 1.2f);
        ImGui::TableSetupColumn("상태", ImGuiTableColumnFlags_WidthFixed, 80.0f * menu.dpi_scale);
        ImGui::TableSetupColumn("상세", ImGuiTableColumnFlags_WidthStretch, 1.4f);
        ImGui::TableHeadersRow();
        for (std::size_t i = 0; i < reports.size(); ++i) {
            const auto& report = reports[i];
            if ((menu.status_filter == 0 && (report.state == ReportState::Off || report.state == ReportState::Applied)) ||
                (menu.status_filter == 1 && report.state == ReportState::Off)) {
                continue;
            }
            ++shown;
            ImGui::PushID(static_cast<int>(i));
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            if (ImGui::Selectable(report.label)) {
                select_menu_page(menu, report.page);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s · %s 화면 열기", report.label,
                    MENU_PAGES[static_cast<std::size_t>(report.page)].label);
            }
            ImGui::TableSetColumnIndex(1);
            const ImVec4 color = report.state == ReportState::Attention ? ImVec4(0.68f, 0.18f, 0.10f, 1.0f) :
                report.state == ReportState::Waiting ? ImVec4(0.54f, 0.36f, 0.05f, 1.0f) :
                report.state == ReportState::Applied ? ImVec4(0.19f, 0.39f, 0.22f, 1.0f) : ImVec4(0.39f, 0.37f, 0.34f, 1.0f);
            constexpr std::array STATE_LABELS{"꺼짐", "적용됨", "대기", "확인 필요", "외부 관리"};
            ImGui::TextColored(color, "%s", STATE_LABELS[static_cast<std::size_t>(report.state)]);
            ImGui::TableSetColumnIndex(2);
            ImGui::TextWrapped("%s", report.detail);
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    if (shown == 0) {
        ImGui::TextDisabled("이 필터에 해당하는 상태 보고가 없습니다.");
    }
}

void minimize_workbench(FeatureMenu& menu) {
    menu.collapsed = true;
    menu.toggle_collapse = false;
    menu.auto_load_collapse = false;
    menu.ui_scale_draft.editing = false;
    menu.notification_speed_draft.editing = false;
    ImGui::SetWindowFocus(nullptr);
}

void draw_home_menu(FeatureMenu& menu, bool popup_open, float scale) {
    constexpr std::array ORDER{MenuPage::Officers, MenuPage::Traits, MenuPage::Domestic,
        MenuPage::CouncilSocial, MenuPage::Battle, MenuPage::Scenario, MenuPage::Settings, MenuPage::Overview};
    const float available = ImGui::GetContentRegionAvail().x;
    const float grid_width = std::min(600.0f * scale, available);
    const float left = ImGui::GetCursorPosX() + (available - grid_width) * 0.5f;
    ImGui::SetCursorPosX(left);
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(8.0f * scale, 5.0f * scale));
    ImGui::BeginDisabled(popup_open);
    if (ImGui::BeginTable("##home_categories", 2,
            ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings, ImVec2(grid_width, 0))) {
        for (const auto page : ORDER) {
            ImGui::TableNextColumn();
            const auto& info = MENU_PAGES[static_cast<std::size_t>(page)];
            if (menu_sprites::sprite_button(menu.ui_atlases, info.label,
                    ImVec2(ImGui::GetContentRegionAvail().x, 38.0f * scale))) {
                select_menu_page(menu, page);
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
                ImGui::SetTooltip("%s", info.description);
            }
        }
        ImGui::EndTable();
    }
    ImGui::EndDisabled();
    ImGui::PopStyleVar();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    const float history_width = std::min(240.0f * scale, ImGui::GetContentRegionAvail().x);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (ImGui::GetContentRegionAvail().x - history_width) * 0.5f);
    if (menu_sprites::sprite_button(menu.ui_atlases, "알림 내역", ImVec2(history_width, 32.0f * scale))) {
        menu.notifications.show_history = true;
    }
}

void draw_category_tabs(FeatureMenu& menu, bool popup_open, float scale, float available_width) {
    const float right = ImGui::GetCursorScreenPos().x + available_width;
    const float gap = 3.0f * scale;
    ImGui::BeginDisabled(popup_open);
    for (std::size_t index = 0; index < MENU_PAGES.size(); ++index) {
        const auto& info = MENU_PAGES[index];
        const float width = ImGui::CalcTextSize(info.label).x + 20.0f * scale;
        if (index != 0 && ImGui::GetItemRectMax().x + gap + width <= right) {
            ImGui::SameLine(0, gap);
        }
        if (menu_sprites::navigation_row(menu.ui_atlases, info.label,
                ImVec2(std::min(width, ImGui::GetContentRegionAvail().x), 34.0f * scale),
                menu.page == static_cast<MenuPage>(index))) {
            select_menu_page(menu, static_cast<MenuPage>(index));
        }
    }
    ImGui::EndDisabled();
}

template<class T>
bool read_native_ui(std::uintptr_t address, T& value) {
    return platform::windows::read_memory(address, std::as_writable_bytes(std::span(&value, 1)));
}

std::uintptr_t native_ui_pointer(std::uintptr_t address) {
    std::uintptr_t value = 0;
    read_native_ui(address, value);
    return value;
}

struct NativeUiRect {
    float x = 0, y = 0, width = 0, height = 0;
};

bool native_ui_rect(std::uintptr_t object, NativeUiRect& rect) {
    // CCmnLayout::GetAbsolutePosition (RVA 0x1EDECA0) adds up to 30 parents.
    // Read only: calling the visibility method would mutate its parent cache.
    struct Layout {
        std::uintptr_t vtable, parent;
        std::uint64_t parent_cache, interfaces;
        float x, y;
        std::int32_t width, height;
        std::uint64_t opacity, interface38;
        std::uint32_t flags;
    } node{};
    for (int depth = 0; object && depth < 30; ++depth) {
        if (!read_native_ui(object, node) || (node.flags & 3) != 3 || !(node.opacity & 0xff) ||
            !std::isfinite(node.x) || !std::isfinite(node.y)) return false;
        if (depth == 0) {
            if (node.width <= 0 || node.height <= 0) return false;
            rect.width = static_cast<float>(node.width);
            rect.height = static_cast<float>(node.height);
        }
        rect.x += node.x;
        rect.y += node.y;
        object = node.parent;
    }
    return !object && rect.width > 0;
}

struct NativeLauncher {
    enum class Kind { Hidden, Toolbar, Function, TitleEdit } kind = Kind::Hidden;
    bool editor_allowed = false;
    NativeUiRect button, panel;
    ImVec2 scale, origin;
};

// Append native controls to the edit/function menu's own
// descriptor group. The game creates, registers and destroys it with that group.
struct NativeEditControl {
    std::int32_t type, x, y, width, height, padding;
    std::uint64_t text_id;
    const wchar_t* text;
    std::array<std::byte, 0x38> properties;
};
static_assert(sizeof(NativeEditControl) == 0x60);

platform::windows::Hook native_edit_factory_hook{};
platform::windows::Hook native_edit_click_hook{};
platform::windows::Hook native_config_click_hook{};
platform::windows::Hook native_config_navigation_hook{};
platform::windows::Hook native_trait_menu_click_hook{};
bool native_config_installed = false;
bool native_trait_menu_installed = false;
std::uintptr_t native_edit_base = 0;
std::atomic<bool> native_edit_open_requested{false};
using NativeEditFactory = void(*)(void*, const NativeEditControl*, int, void*);
using NativeEditClick = void(*)(void*, void**);

void native_edit_controls(void* group, const NativeEditControl* controls, int count, void* parent) {
    const auto original = reinterpret_cast<NativeEditFactory>(native_edit_factory_hook.original);
    if (native_trait_menu_installed && count == 6 && parent &&
        *static_cast<std::uintptr_t*>(parent) == native_edit_base + 0x271EDC0) {
        std::array<NativeEditControl, 7> extended;
        std::copy_n(controls, 6, extended.begin());
        const int spacing = controls[5].y - controls[4].y;
        extended[0].height += spacing;
        extended[6] = controls[5];
        extended[6].y += spacing;
        extended[6].text = L"기재";
        original(group, extended.data(), static_cast<int>(extended.size()), parent);
        return;
    }
    if (native_config_installed && count == 13 && parent &&
        *static_cast<std::uintptr_t*>(parent) == native_edit_base + 0x2701888) {
        std::array<NativeEditControl, 14> extended;
        std::copy_n(controls, 13, extended.begin());
        const int row_spacing = controls[6].y - controls[5].y;
        extended[0].height += row_spacing;
        for (int index = 5; index < 13; ++index) extended[index].y += row_spacing;
        // Preserve all original IDs, including the two separators. Only the
        // visual position is inserted after Edit (ID 4); the new ID is 13.
        extended[13] = controls[4];
        extended[13].y += row_spacing;
        extended[13].text = L"치트 편집";
        original(group, extended.data(), static_cast<int>(extended.size()), parent);
        return;
    }
    if (count != 6 || !parent || *static_cast<std::uintptr_t*>(parent) != native_edit_base + 0x271EB28) {
        original(group, controls, count, parent);
        return;
    }
    std::array<NativeEditControl, 8> extended;
    std::copy_n(controls, 6, extended.begin());
    const int row_spacing = controls[5].y - controls[4].y;
    extended[0].height += row_spacing * 2;
    extended[6] = controls[5];
    extended[6].x = controls[1].x;
    extended[6].y = controls[5].y + row_spacing;
    extended[6].text = L"기재 편집";
    extended[7] = extended[6];
    extended[7].y += row_spacing;
    extended[7].text = L"치트 편집";
    original(group, extended.data(), static_cast<int>(extended.size()), parent);
}

void native_edit_clicked(void* callback, void** argument) {
    // This is EditUIMainLayout's existing click callback. Its indices 1..5
    // retain the original behavior; 6 opens native traits and 7 opens ImGui.
    const auto button = reinterpret_cast<std::uintptr_t>(*argument);
    if (button && *reinterpret_cast<const std::int32_t*>(button + 0x88) == 6) {
        open_native_trait_editor();
        return;
    }
    if (button && *reinterpret_cast<const std::int32_t*>(button + 0x88) == 7) {
        native_edit_open_requested.store(true, std::memory_order_release);
        return;
    }
    reinterpret_cast<NativeEditClick>(native_edit_click_hook.original)(callback, argument);
}

void native_config_clicked(void* callback, void** argument) {
    const auto button = reinterpret_cast<std::uintptr_t>(*argument);
    if (button && *reinterpret_cast<const std::int32_t*>(button + 0x88) == 13) {
        native_edit_open_requested.store(true, std::memory_order_release);
        return;
    }
    reinterpret_cast<NativeEditClick>(native_config_click_hook.original)(callback, argument);
}

void native_trait_menu_clicked(void* callback, void** argument) {
    const auto button = reinterpret_cast<std::uintptr_t>(*argument);
    if (button && *reinterpret_cast<const std::int32_t*>(button + 0x88) == 6) {
        open_native_trait_editor();
        return;
    }
    reinterpret_cast<NativeEditClick>(native_trait_menu_click_hook.original)(callback, argument);
}

void native_config_navigation(void* layout, void* output) {
    reinterpret_cast<void(*)(void*, void*)>(native_config_navigation_hook.original)(layout, output);
    const auto address = reinterpret_cast<std::uintptr_t>(layout);
    if (*reinterpret_cast<const std::int32_t*>(address + 0x150) != 14) return;
    const auto lookup = reinterpret_cast<void*(*)(std::uintptr_t, int)>(native_edit_base + 0x1D15440);
    void* button = lookup(address + 0x140, 13);
    void* edit = lookup(address + 0x140, 4);
    // The game's output is a vector of control pointers. Use its own growth
    // routine so allocation and destruction remain owned by the game.
    struct Controls { void** begin; void** end; void** capacity; };
    auto& items = *static_cast<Controls*>(output);
    const auto insertion = std::find(items.begin, items.end, edit);
    if (!button || insertion == items.end) return;
    const auto index = insertion - items.begin + 1;
    if (items.end == items.capacity) {
        reinterpret_cast<void(*)(void*, void**, void**)>(native_edit_base + 0x33F50)(output, items.end, &button);
    } else {
        *items.end++ = button;
    }
    std::rotate(items.begin + index, items.end - 1, items.end);
}

NativeLauncher native_launcher() {
    NativeLauncher result;
    // Layout RVAs belong to this executable revision. Validate at the memory boundary.
    static const std::uintptr_t base = [] {
        const auto image = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
        IMAGE_DOS_HEADER dos{};
        IMAGE_NT_HEADERS64 nt{};
        if (!read_native_ui(image, dos) || dos.e_magic != IMAGE_DOS_SIGNATURE ||
            !read_native_ui(image + dos.e_lfanew, nt) || nt.Signature != IMAGE_NT_SIGNATURE ||
            nt.FileHeader.TimeDateStamp != 0x69A67ED1 || nt.OptionalHeader.SizeOfImage != 0x440F000)
            return std::uintptr_t{0};
        return image;
    }();
    if (!base) return result;

    // Present starts after MinHook initialization. Both detours run on the
    // game's original UI thread; the click crosses to ImGui only via the flag.
    // The runtime pins this DLL for the process lifetime (game_loop.cpp).
    static const bool native_edit_installed = [=] {
        native_edit_base = base;
        const auto click = platform::windows::create_hook(reinterpret_cast<void*>(base + 0x76EC40),
            reinterpret_cast<void*>(&native_edit_clicked));
        if (!click) return false;
        native_edit_click_hook = *click;
        const auto factory = platform::windows::create_hook(reinterpret_cast<void*>(base + 0x1D13E60),
            reinterpret_cast<void*>(&native_edit_controls));
        if (!factory) return false;
        native_edit_factory_hook = *factory;
        const auto trait_click = platform::windows::create_hook(reinterpret_cast<void*>(base + 0x76E930),
            reinterpret_cast<void*>(&native_trait_menu_clicked));
        if (trait_click && install_native_trait_editor(base)) {
            native_trait_menu_click_hook = *trait_click;
            native_trait_menu_installed = platform::windows::enable_hook(native_trait_menu_click_hook) == 0;
        }
        const auto config_click = platform::windows::create_hook(reinterpret_cast<void*>(base + 0x744990),
            reinterpret_cast<void*>(&native_config_clicked));
        const auto config_navigation = platform::windows::create_hook(reinterpret_cast<void*>(base + 0x1D04F90),
            reinterpret_cast<void*>(&native_config_navigation));
        if (config_click && config_navigation) {
            native_config_click_hook = *config_click;
            native_config_navigation_hook = *config_navigation;
            if (platform::windows::enable_hook(native_config_click_hook) == 0) {
                if (platform::windows::enable_hook(native_config_navigation_hook) == 0) {
                    native_config_installed = true;
                } else {
                    platform::windows::disable_hook(native_config_click_hook);
                }
            }
        }
        if (platform::windows::enable_hook(native_edit_click_hook) != 0) return false;
        if (platform::windows::enable_hook(native_edit_factory_hook) != 0) {
            platform::windows::disable_hook(native_edit_click_hook);
            return false;
        }
        return true;
    }();

    struct StateStack { std::uint64_t unused[2], count, capacity; std::uintptr_t entries; } stack{};
    std::array<std::uintptr_t, 32> states{};
    if (!read_native_ui(base + 0x2E99210, stack) || !stack.count || stack.count > states.size() ||
        !platform::windows::read_memory(stack.entries,
            std::as_writable_bytes(std::span(states.data(), static_cast<std::size_t>(stack.count))))) return result;
    bool in_game = false, config_open = false;
    std::string_view top;
    std::array<char, 32> name{};
    for (std::size_t index = 0; index < stack.count; ++index) {
        if (!read_native_ui(states[index] + 0x71, name)) return result;
        top = std::string_view(name.data(), std::find(name.begin(), name.end(), '\0') - name.begin());
        in_game |= top == "InGameState";
        config_open |= top == "ConfigDlgState";
        if (top == "MainGameState") {
            NativeUiRect loading;
            if (native_ui_rect(native_ui_pointer(states[index] + 0x478), loading)) return result;
        }
    }
    result.editor_allowed = in_game || top == "EditGameState";
    if (!result.editor_allowed) return result;
    const auto active = states[stack.count - 1];
    if (top == "EditGameState" && !in_game) {
        const auto owner = native_ui_pointer(native_ui_pointer(active + 0x478) + 8);
        const auto root = native_ui_pointer(native_ui_pointer(owner + 0x58) + 0x10);
        const auto layout = native_ui_pointer(root + 0x1D0);
        std::int32_t control_count = 0;
        if (native_edit_installed && read_native_ui(layout + 0x150, control_count) && control_count == 8)
            return result; // The native button supplies this screen's launcher.
        const auto buttons = native_ui_pointer(layout + 0x1A8);
        NativeUiRect first, last, parent;
        if (!native_ui_rect(native_ui_pointer(buttons + 8), first) ||
            !native_ui_rect(native_ui_pointer(buttons + 0x90), last) ||
            !native_ui_rect(layout, parent)) return result;
        result.panel = {first.x - (parent.width - first.width) * 0.5f,
            last.y + last.height + 40, parent.width, 140};
        result.button = {first.x,
            result.panel.y + (result.panel.height - first.height) * 0.5f, first.width, first.height};
        result.kind = NativeLauncher::Kind::TitleEdit;
    } else if (top == "ConfigDlgState" && in_game) {
        const auto layout = native_ui_pointer(active + 0x488);
        std::int32_t control_count = 0;
        if (native_config_installed && read_native_ui(layout + 0x150, control_count) && control_count == 14)
            return result;
        const auto buttons = native_ui_pointer(layout + 0x1A8);
        if (!native_ui_rect(native_ui_pointer(buttons + 0x30), result.button)) return result;
        result.button.x += result.button.width + 20;
        result.kind = NativeLauncher::Kind::Function;
    } else if (in_game && !config_open) {
        const auto toolbar = native_ui_pointer(native_ui_pointer(base + 0x2E99460) + 0x28);
        if (!native_ui_rect(native_ui_pointer(toolbar + 0x1D0 + 5 * 8), result.button)) return result;
        result.button.x -= result.button.width + 14;
        result.kind = NativeLauncher::Kind::Toolbar;
    }

    // Inverse of the game's mouse conversion at RVA 0x17EC57A:
    // fit 1920x1080 into the render target, then map that target to the client.
    // Render and client sizes differ in fullscreen and with Windows display scaling.
    std::array<std::int32_t, 2> render_size{};
    if (!read_native_ui(native_ui_pointer(base + 0x2EA67D8) + 0x10, render_size) ||
        render_size[0] <= 0 || render_size[1] <= 0) {
        result.kind = NativeLauncher::Kind::Hidden;
        return result;
    }
    const ImGuiViewport& viewport = *ImGui::GetMainViewport();
    const float fit = std::min(render_size[0] / 1920.0f, render_size[1] / 1080.0f);
    result.scale = ImVec2(fit * viewport.Size.x / render_size[0], fit * viewport.Size.y / render_size[1]);
    result.origin = ImVec2(viewport.Pos.x + (viewport.Size.x - 1920 * result.scale.x) * 0.5f,
        viewport.Pos.y + (viewport.Size.y - 1080 * result.scale.y) * 0.5f);
    return result;
}

void draw_menu_launcher(FeatureMenu& menu, bool popup_open, const NativeLauncher& anchor) {
    if (anchor.kind == NativeLauncher::Kind::Hidden) return;
    const float hud_scale = std::min(anchor.scale.x, anchor.scale.y);
    const bool title_edit = anchor.kind == NativeLauncher::Kind::TitleEdit;
    const bool toolbar = anchor.kind == NativeLauncher::Kind::Toolbar;
    const NativeUiRect& window = title_edit ? anchor.panel : anchor.button;
    const ImVec2 position(anchor.origin.x + window.x * anchor.scale.x,
        anchor.origin.y + window.y * anchor.scale.y);
    const ImVec2 size(window.width * anchor.scale.x, window.height * anchor.scale.y);
    const ImVec2 button_position(anchor.origin.x + anchor.button.x * anchor.scale.x,
        anchor.origin.y + anchor.button.y * anchor.scale.y);
    const ImVec2 button_size(anchor.button.width * anchor.scale.x, anchor.button.height * anchor.scale.y);
    ImGui::SetNextWindowPos(position, ImGuiCond_Always);
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(1, 1));
    constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing;
    if (ImGui::Begin("###S8RPK.Launcher", nullptr, flags)) {
        if (title_edit) {
            menu_sprites::draw_panel(menu.ui_atlases, position,
                ImVec2(position.x + size.x, position.y + size.y), hud_scale);
        }
        ImGui::SetCursorScreenPos(button_position);
        ImGui::BeginDisabled(popup_open);
        bool pressed;
        if (toolbar) {
            pressed = menu_sprites::launcher_button(menu.ui_atlases, "치트 편집##launcher", button_size.x, hud_scale);
        } else {
            pressed = menu_sprites::detail::button_item("치트 편집##launcher", button_size);
            ImDrawList& draw = *ImGui::GetWindowDrawList();
            const ImVec2 maximum(button_position.x + button_size.x, button_position.y + button_size.y);
            const bool hovered = ImGui::IsItemHovered();
            const bool held = ImGui::IsItemActive();
            // Tint the native ornamental button; text remains white in every state.
            draw.AddRectFilled(button_position, maximum, IM_COL32(112, 15, 29, 255), 5 * hud_scale);
            menu_sprites::detail::nine_slice(draw, menu.ui_atlases[0],
                menu_sprites::detail::BUTTONS[held ? 3 : hovered ? 1 : 0], button_position, maximum,
                12, 10, hud_scale, ImGui::GetColorU32(ImVec4(1, hovered ? 0.38f : 0.22f, 0.25f, 1)));
            const float font_size = 28 * hud_scale;
            const ImVec2 text_size = ImGui::GetFont()->CalcTextSizeA(font_size, FLT_MAX, 0, "치트 편집");
            draw.AddText(ImGui::GetFont(), font_size,
                ImVec2(button_position.x + (button_size.x - text_size.x) * 0.5f,
                    button_position.y + (button_size.y - text_size.y) * 0.5f),
                ImGui::GetColorU32(ImVec4(1, 1, 1, 1)), "치트 편집");
            ImGui::RenderNavHighlight(ImRect(button_position, maximum), ImGui::GetItemID());
        }
        if (pressed) {
            if (menu.collapsed) {
                menu.collapsed = false;
                menu.toggle_collapse = false;
                menu.auto_load_collapse = false;
            } else {
                minimize_workbench(menu);
            }
        }
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered() && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            ImGui::SetNextWindowPos(ImVec2(button_position.x + button_size.x * 0.5f,
                button_position.y + button_size.y + 6.0f * hud_scale), ImGuiCond_Always, ImVec2(0.5f, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8 * hud_scale, 3 * hud_scale));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.23f, 0.21f, 0.22f, 1));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.58f, 0.54f, 0.49f, 1));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.96f, 0.94f, 0.90f, 1));
            ImGui::BeginTooltip();
            ImGui::TextUnformatted("편집 도구");
            ImGui::EndTooltip();
            ImGui::PopStyleColor(3);
            ImGui::PopStyleVar(3);
        }
        if (ImGui::BeginPopupContextItem("##launcher_shortcuts")) {
            if (ImGui::MenuItem("편집 도구 열기")) {
                menu.collapsed = false;
                menu.toggle_collapse = false;
                menu.auto_load_collapse = false;
            }
            if (menu.snapshot.features.get(domain::FeatureId::ShowWidgetNotif) &&
                ImGui::MenuItem("알림 내역")) {
                menu.notifications.show_history = true;
            }
            if (menu.pk_editors_unlocked && menu.snapshot.features.get(domain::FeatureId::ShowWidgetHero) &&
                ImGui::MenuItem("주인공 무장 편집", nullptr, false, menu.snapshot.protagonist_id.has_value())) {
                menu.protagonist_detail = true;
            }
            if (menu.pk_editors_unlocked && menu.snapshot.features.get(domain::FeatureId::ShowWidgetAllOfficers) &&
                ImGui::MenuItem("모든 무장 편집")) {
                menu.officer_list_open = true;
            }
            ImGui::EndPopup();
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(3);
}

} // namespace

void request_feature_menu_toggle(FeatureMenu& menu) {
    // A first hotkey press opens the workbench instead of immediately folding it.
    if (!menu.open) {
        menu.open = true;
        menu.collapsed = true;
    }
    menu.toggle_collapse = !menu.toggle_collapse;
    menu.auto_load_collapse = false;
}

void draw_feature_menu(FeatureMenu& menu, services::UiBridge& bridge) {
    const NativeLauncher launcher = native_launcher();
    update_native_trait_editor(menu, bridge);
    if (!launcher.editor_allowed) {
        menu.collapsed = true;
        menu.toggle_collapse = false;
        return;
    }
    if (native_edit_open_requested.exchange(false, std::memory_order_acq_rel)) {
        menu.open = true;
        menu.collapsed = false;
        menu.toggle_collapse = false;
        menu.auto_load_collapse = false;
        menu.home_screen = true;
    }
    services::CommandMailbox& mailbox = bridge.commands;
    FeatureMenuDraw draw{menu, mailbox, bridge};
    if (menu.collapsed || menu.home_screen || menu.page != MenuPage::Domestic) {
        menu.city_management.was_open = false;
        menu.city_management.technology_requested = false;
    }

    // Independently opened editors continue rendering when the workbench closes.
    draw_special_assignment_results(menu);
    if (!menu.pk_editors_unlocked) {
        menu.officer_list_open = false;
        menu.protagonist_detail = false;
        menu.specialty_tools.open = false;
        menu.officer_open_request.reset();
    }
    menu.city_management.manual_editing = menu.pk_editors_unlocked;
    menu.officer_tools.manual_editing = menu.pk_editors_unlocked;
    draw.draw_secondary_jewels();
    if (!menu.open) {
        return;
    }

    const bool popup_open = ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
    // Keep rendering a page while its modal is open. A queued hotkey takes effect
    // once it closes, so hiding the page cannot strand the modal or its draft.
    if (!popup_open) {
        if (menu.toggle_collapse) {
            menu.collapsed = !menu.collapsed;
            menu.toggle_collapse = false;
            menu.auto_load_collapse = false;
            if (menu.collapsed) {
                ImGui::SetWindowFocus(nullptr);
            }
        } else if (menu.auto_load_collapse) {
            menu.collapsed = true;
            menu.auto_load_collapse = false;
        }
    }
    if (menu.collapsed || menu.officer_list_open || menu.protagonist_detail || menu.secondary_jewel_window) {
        draw_menu_launcher(menu, popup_open, launcher);
        return;
    }

    const ImGuiViewport& viewport = *ImGui::GetMainViewport();
    const ImVec2 origin = viewport.WorkPos;
    const ImVec2 size = viewport.WorkSize;
    const float hud_scale = std::min(size.x / 1920.0f, size.y / 1080.0f);
    const float scale = menu.dpi_scale;
    const bool drawn_home = menu.home_screen;
    constexpr const char* window_name = "편집 도구###S8RPK.Workbench";

    ImGui::SetNextWindowPos(origin, ImGuiCond_Always);
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::SetNextWindowCollapsed(false, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus;
    if (ImGui::Begin(window_name, nullptr, flags)) {
        ImDrawList& background = *ImGui::GetWindowDrawList();

        background.AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y), IM_COL32(15, 12, 10, 140));
        draw_screen_heading(menu, menu.page == MenuPage::Settings && !drawn_home ? domain::native_ui::SETTINGS :
            domain::native_ui::EDIT, "편집", drawn_home ? "편집할 항목을 선택해 주십시오" :
            MENU_PAGES[static_cast<std::size_t>(menu.page)].description, hud_scale);

        const ImVec2 paper_min(origin.x + size.x * (drawn_home ? 0.23f : 0.06f),
            origin.y + size.y * (drawn_home ? 0.18f : 0.115f));
        const ImVec2 paper_max(origin.x + size.x * (drawn_home ? 0.77f : 0.94f),
            origin.y + size.y * (drawn_home ? 0.80f : 0.86f));

        menu.window_x = paper_min.x;
        menu.window_y = paper_min.y;
        menu_sprites::draw_panel(menu.ui_atlases, paper_min, paper_max, hud_scale);
        ImGui::SetCursorScreenPos(ImVec2(paper_min.x + 38 * hud_scale, paper_min.y + 26 * hud_scale));
        if (drawn_home) {
            menu_sprites::navigation_row(menu.ui_atlases, "전체 메뉴##home_tab", ImVec2(112 * scale, 34 * scale), true);
        } else {
            draw_category_tabs(menu, popup_open, scale, paper_max.x - paper_min.x - 76 * hud_scale);
        }

        const MenuPage drawn_page = menu.page;
        const ImVec2 body_min(paper_min.x + 28 * hud_scale, ImGui::GetCursorScreenPos().y);
        const ImVec2 body_max(paper_max.x - 28 * hud_scale, paper_max.y - 24 * hud_scale);
        const ImVec2 body_size(body_max.x - body_min.x, std::max(1.0f, body_max.y - body_min.y));

        draw_native_card(menu, body_min, body_max, hud_scale);
        ImGui::SetCursorScreenPos(body_min);
        ImGui::PushID(drawn_home ? -1 : static_cast<int>(menu.page));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24 * hud_scale, 20 * hud_scale));
        if (!drawn_home) {
            ImGui::SetNextWindowContentSize(ImVec2(std::max(440.0f * scale,
                body_size.x - ImGui::GetStyle().ScrollbarSize - 48 * hud_scale), 0));
        }
        if (ImGui::BeginChild("##workbench_page", body_size, ImGuiChildFlags_AlwaysUseWindowPadding,
                ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_HorizontalScrollbar)) {
            draw_previous_request_error(menu);
            if (menu.ready && menu.save_notice && menu.save_notice->status != services::ConfigSaveStatus::Completed) {
                ImGui::TextColored(ImVec4(0.68f, 0.18f, 0.10f, 1.0f), "설정 저장 확인 필요 (%d, %u)",
                    static_cast<int>(menu.save_notice->status), menu.save_notice->native_code);
                if ((menu.save_notice->status == services::ConfigSaveStatus::Failed ||
                     menu.save_notice->status == services::ConfigSaveStatus::Cancelled) && ImGui::SmallButton("저장 다시 시도")) {
                    menu.retry_save_requested = true;
                }
            } else if (!menu.ready && menu.save_notice) {
                ImGui::TextWrapped("설정 파일을 확인해 주세요. 형식 또는 파일 접근 문제가 해결되면 자동으로 다시 읽습니다.");
            }
            if (drawn_home) {
                draw_home_menu(menu, popup_open, scale);
            } else {
                ImGui::PushItemWidth(std::min(280.0f * scale, ImGui::GetContentRegionAvail().x * 0.48f));
                ImGui::PushTextWrapPos(0.0f);
                switch (drawn_page) {
                case MenuPage::Overview: draw.draw_overview(); break;
                case MenuPage::Officers: draw.draw_officer_edit(); break;
                case MenuPage::Traits: draw.draw_traits(); break;
                case MenuPage::Domestic: draw.draw_domestic(); break;
                case MenuPage::CouncilSocial:
                    if (ImGui::BeginTable("##council_cards", 2, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_BordersInnerV)) {
                        ImGui::TableNextColumn();
                        draw.draw_council();
                        ImGui::TableNextColumn();
                        draw.draw_social();
                        ImGui::EndTable();
                    }
                    break;
                case MenuPage::Battle:
                    if (ImGui::BeginTable("##battle_cards", 2, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_BordersInnerV)) {
                        ImGui::TableNextColumn();
                        draw.draw_war();
                        ImGui::TableNextColumn();
                        draw.draw_stratagem();
                        ImGui::EndTable();
                    }
                    break;
                case MenuPage::Scenario: draw.draw_scenario(); break;
                case MenuPage::Settings: draw.draw_footer(ImGui::GetContentRegionAvail().x); break;
                case MenuPage::Count: break;
                }
                ImGui::PopTextWrapPos();
                ImGui::PopItemWidth();
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopID();

        const float edge = 88 * hud_scale;

        ImGui::SetCursorScreenPos(ImVec2(origin.x + (size.x - edge) * 0.5f, origin.y + size.y * 0.93f - edge * 0.5f));
        ImGui::BeginDisabled(popup_open);
        if (menu_sprites::back_button(menu.ui_atlases, "뒤로##workbench_back", edge, hud_scale)) {
            if (drawn_home) {
                minimize_workbench(menu);
            } else {
                menu.home_screen = true;
                menu.ui_scale_draft.editing = false;
                menu.notification_speed_draft.editing = false;
                menu.mission_appointment_limit_editing = false;
            }
        }
        ImGui::SetCursorScreenPos(ImVec2(origin.x + size.x - 96 * hud_scale, origin.y + 20 * hud_scale));
        if (menu_sprites::sprite_button(menu.ui_atlases, "최소화##workbench_minimize", ImVec2(80 * hud_scale, 30 * hud_scale))) {
            minimize_workbench(menu);
        }
        ImGui::EndDisabled();
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
    // Keep the HUD control at its calibrated position even while a page is open.
    draw_menu_launcher(menu, popup_open, launcher);
}

void draw_officer_name_input(FeatureMenu& menu, services::UiBridge& bridge, std::uint16_t id) {
    ImGui::SetNextItemWidth(180);
    if (menu.officer_name_focus) {
        ImGui::SetKeyboardFocusHere();
        menu.officer_name_focus = false;
    }
    const bool enter = ImGui::InputText("이름##edit_officer", menu.officer_name_draft.data(),
                                        menu.officer_name_draft.size(), ImGuiInputTextFlags_EnterReturnsTrue);
    const bool edited = ImGui::IsItemDeactivatedAfterEdit();
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        menu.officer_name_edit_id = 0;
    } else if (enter || edited) {
        if (bridge.request_officer_name({id, menu.officer_name_draft.data()})) {
            menu.officer_name_save_pending = true;
            menu.officer_name_edit_id = 0;
        } else {
            bridge.publish_notice("무장 이름 저장 요청을 접수하지 못했습니다.");
        }
    }
}

void draw_officer_name(FeatureMenu& menu, services::UiBridge& bridge, std::uint16_t id, bool protagonist) {
    const auto name = menu.officer_names ? menu.officer_names->find(id) : domain::OfficerNames::const_iterator{};
    const auto display = menu.officer_names && name != menu.officer_names->end()
        ? name->second : "#" + std::to_string(id);
    ImGui::BeginDisabled(menu.officer_name_save_pending);
    if (menu.officer_name_edit_id == id && !menu.officer_name_edit_list &&
        menu.officer_name_edit_protagonist == protagonist) {
        draw_officer_name_input(menu, bridge, id);
    } else if (ImGui::Button(display.c_str())) {
        menu.officer_name_edit_id = id;
        menu.officer_name_edit_protagonist = protagonist;
        menu.officer_name_edit_list = false;
        menu.officer_name_focus = true;
        std::snprintf(menu.officer_name_draft.data(), menu.officer_name_draft.size(), "%s", display.c_str());
    }
    ImGui::EndDisabled();
}

void draw_officer_traits(FeatureMenu& menu, services::CommandMailbox& mailbox, std::uint16_t officer_id,
                         const domain::OfficerTraits& traits, bool protagonist) {
    if (ImGui::CollapsingHeader("기재")) {
        constexpr std::array trait_names{
            "", "대덕", "의협", "만인적", "일신시담", "금마초", "노당익장", "복룡", "봉추", "기린아", "초세지걸",
            "왕좌", "불요불굴", "낭고", "병귀신속", "료래료래", "금강불괴", "위서심공", "산도강습", "강동맹호", "소패왕",
            "용재", "화신", "냉염", "괄목", "방울감녕", "원모심려", "천하무쌍", "수화폐월", "명가위광", "악역무도",
            "구심", "황천", "전장의꽃", "호위", "기습병", "간파", "군규", "냉정", "기략", "진법",
            "궤계", "맹공", "견수", "불굴", "산전", "삼전", "강창", "조기통제", "북방마술", "질주궁",
            "수신", "상조교", "재녀", "교화", "호랑지심", "부가", "능리", "근면", "시재", "유심",
            "경성", "직정", "자만", "반감", "소심", "조급", "성걸", "이기적", "병약", "거만"};
        std::vector<std::uint16_t> available_traits;
        for (std::uint16_t id = 1; id <= 70; ++id) {
            available_traits.push_back(id);
        }
        available_traits.push_back(201);
        available_traits.push_back(202);
        if (menu.custom_traits) {
            for (const auto& entry : *menu.custom_traits) {
                available_traits.push_back(entry.first);
            }
        }
        std::sort(available_traits.begin(), available_traits.end());
        available_traits.erase(std::unique(available_traits.begin(), available_traits.end()), available_traits.end());
        const auto trait_label = [&](std::uint16_t id) {
            if (id <= 70 && id > 0) {
                return std::string(trait_names[id]) + " (ID " + std::to_string(id) + ")";
            }
            if (id == 201 || id == 202) {
                return std::string(id == 201 ? "괴물" : "잔병첩보") + " (ID " + std::to_string(id) + ")";
            }
            if (menu.custom_traits) {
                const auto found = menu.custom_traits->find(id);
                if (found != menu.custom_traits->end()) {
                    return found->second.name + " (ID " + std::to_string(id) + ")";
                }
            }
            return "사용자 기재 #" + std::to_string(id) + " (ID " + std::to_string(id) + ")";
        };
        for (std::size_t slot = 0; slot < traits.size(); ++slot) {
            const auto& trait = traits[slot];
            ImGui::PushID(static_cast<int>(slot));
            ImGui::BeginDisabled(menu.pending.has_value() || !menu.pending_officer_edits.empty() || !menu.ready);
            const auto preview = trait ? trait_label(trait->id) : std::string("비어있음");
            if (ImGui::BeginCombo("기재 변경", preview.c_str())) {
                const auto submit_trait = [&](std::uint16_t id) {
                    const domain::RequestId request{++menu.next_request};
                    auto result = mailbox.push({request, menu.epoch,
                        domain::SetRosterOfficerTrait{officer_id, static_cast<std::uint8_t>(slot), id, protagonist}});
                    if (result) {
                        menu.pending = request;
                    } else {
                        menu.error = result.error();
                    }
                };
                for (const auto id : available_traits) {
                    const auto label = trait_label(id);
                    const bool selected = trait && trait->id == id;
                    if (ImGui::Selectable(label.c_str(), selected)) {
                        submit_trait(id);
                    }
                    if (selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::EndDisabled();
            if (!trait) {
                ImGui::Text("기재 %zu: 없음", slot + 1);
                ImGui::PopID();
                continue;
            }
            const char* name = trait->id > 0 && trait->id < trait_names.size() ? trait_names[trait->id]
                : trait->id == 201 ? "괴물" : trait->id == 202 ? "잔병첩보" : nullptr;
            if (name) {
                ImGui::Text("기재 %zu: %s (ID %u)", slot + 1, name, static_cast<unsigned>(trait->id));
            } else {
                ImGui::Text("기재 %zu: #%u (ID %u)", slot + 1, static_cast<unsigned>(trait->id),
                            static_cast<unsigned>(trait->id));
            }
            for (const auto& effect : trait->effects) {
                if (!effect.effect_id) {
                    break;
                }
                const auto definition = menu.trait_effects ? menu.trait_effects->find(effect.effect_id)
                    : domain::TraitEffectDefinitions::const_iterator{};
                if (menu.trait_effects && definition != menu.trait_effects->end()) {
                    auto description = definition->second.text;
                    const auto replace_value = [&](const char* token, std::uint16_t value,
                                                   const auto& map) {
                        const auto position = description.find(token);
                        if (position != std::string::npos) {
                            const auto found = map.find(value);
                            description.replace(position, std::char_traits<char>::length(token),
                                found == map.end() ? std::to_string(value) : found->second);
                        }
                    };
                    replace_value("{value}", effect.value1, definition->second.values);
                    replace_value("{param}", effect.value2, definition->second.params);
                    ImGui::Text("  %s (ID:%u)", description.c_str(), static_cast<unsigned>(effect.effect_id));
                } else {
                    ImGui::Text("  효과 %u (값1: %u, 값2: %u)", static_cast<unsigned>(effect.effect_id),
                                static_cast<unsigned>(effect.value1), static_cast<unsigned>(effect.value2));
                }
            }
            ImGui::PopID();
        }
    }
}

namespace {

void open_officer_details(FeatureMenu& menu, std::uint16_t id) {
    menu.officer_list_open = true;
    menu.officer_open_request = id;
    menu.officer_open_refresh_requested = false;
    menu.officer_open_focus = true;
    menu.officer_filter = -1;
    menu.officer_search_pending = false;
    menu.officer_pending_search.clear();
    menu.officer_search.fill(0);
    menu.officer_search_result = -1;
    menu.selected_officer_ids.fill(false);
    menu.officer_next_selection.reset();
    menu.officer_reselect_pending = false;
    menu.officer_name_edit_id = 0;
    menu.officer_name_focus = false;
    menu.error.reset();
}

void draw_officer_inspection(FeatureMenu& menu, services::UiBridge& bridge,
    std::uint16_t id, bool protagonist) {
    const auto& color = protagonist ? menu.snapshot.protagonist_faction_color
        : menu.snapshot.selected_faction_color;
    auto& draft = protagonist ? menu.protagonist_color_draft : menu.selected_color_draft;
    const bool busy = !menu.ready || menu.pending.has_value() || !menu.pending_officer_edits.empty();

    ImGui::PushID(protagonist ? "hero_inspection" : "selected_inspection");
    if (menu.snapshot.selected_faction_color.recovery_pending ||
        menu.snapshot.protagonist_faction_color.recovery_pending) {
        ImGui::TextWrapped("세력 색상의 원래 값을 복구하는 중입니다. 복구가 끝날 때까지 색상 편집을 할 수 없습니다.");
    } else if (color.officer_id != id) {
        ImGui::TextDisabled("세력 색상 정보를 기다리는 중입니다.");
    } else if (!color.available) {
        ImGui::TextDisabled(color.no_faction ? "소속 세력이 없습니다." : "세력 색상을 읽을 수 없습니다.");
    } else {
        if (draft.officer_id != id || !draft.dirty) {
            draft = {id, color.token, color.color, color.color, false};
        }
        ImGui::BeginDisabled(busy);
        ImGui::SetNextItemWidth(100 * menu.dpi_scale);
        if (ImGui::InputInt("세력 색상 번호", &draft.value, 0, 0)) {
            draft.dirty = true;
        }
        const bool stale = draft.token != color.token || draft.expected_color != color.color;
        const bool valid = draft.value >= 0 && draft.value <= 255;
        ImGui::SameLine();
        ImGui::BeginDisabled(!draft.dirty || stale || !valid);
        if (ImGui::Button("색상 적용")) {
            const domain::RequestId request{++menu.next_request};
            const auto result = bridge.commands.push({request, menu.epoch,
                domain::SetOfficerFactionColor{id, draft.token, draft.expected_color,
                    static_cast<std::uint8_t>(draft.value), protagonist}});
            if (result) {
                menu.pending = request;
                menu.pending_notice = "세력 색상을 변경했습니다.";
                draft.dirty = false;
            } else {
                menu.error = result.error();
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("현재 값 사용")) {
            draft = {id, color.token, color.color, color.color, false};
        }
        ImGui::EndDisabled();
        if (stale) {
            ImGui::TextWrapped("소속 세력 또는 색상이 바뀌었습니다. 현재 값을 다시 확인한 뒤 편집하세요.");
        } else if (!valid) {
            ImGui::TextUnformatted("색상 번호는 0~255 범위로 입력하세요.");
        }
        ImGui::TextDisabled("소속 세력 전체에 적용됩니다. 현재 번호: %u", color.color);
    }

    if (!protagonist || ImGui::CollapsingHeader("관계 정보")) {
        const auto& relations = protagonist ? menu.snapshot.protagonist_relations
            : menu.snapshot.selected_officer_relations;
        ImGui::BeginDisabled(busy || (relations && relations->officer_id == id &&
            relations->epoch == menu.epoch && relations->status == domain::OfficerRelationsStatus::Loading));
        if (ImGui::Button("관계 갱신")) {
            const domain::RequestId request{++menu.next_request};
            const auto result = bridge.commands.push({request, menu.epoch,
                domain::RefreshOfficerRelations{id, protagonist}});
            if (result) {
                menu.pending = request;
                menu.error.reset();
            } else {
                menu.error = result.error();
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::TextDisabled("게임에서 관계가 바뀌면 갱신하세요.");
        if (!relations || relations->officer_id != id || relations->epoch != menu.epoch ||
            relations->status == domain::OfficerRelationsStatus::Loading) {
            ImGui::TextUnformatted("관계 정보를 불러오는 중입니다.");
        } else {
            switch (relations->status) {
            case domain::OfficerRelationsStatus::Partial:
                ImGui::TextWrapped("일부 관계 데이터만 확인했습니다. 조회하지 못한 종류는 별도로 표시합니다.");
                break;
            case domain::OfficerRelationsStatus::Unavailable:
                ImGui::TextWrapped("현재 게임의 관계 데이터 위치를 확인할 수 없습니다. 관계 갱신으로 다시 조회하세요.");
                break;
            case domain::OfficerRelationsStatus::ReadFailed:
                ImGui::TextWrapped("관계 데이터를 읽지 못했습니다. 관계 갱신으로 다시 조회하세요.");
                break;
            default: break;
            }
            constexpr std::array names{"의형제", "배우자", "상생", "상극", "원수", "호적수"};
            const float relation_card_height = std::max(ImGui::GetContentRegionAvail().y * 0.44f,
                ImGui::GetFrameHeight() * 6.0f);
            if (!protagonist && ImGui::BeginTable("##relationship_cards", 2, ImGuiTableFlags_SizingStretchSame)) {
                constexpr std::array<std::size_t, 4> order{1, 0, 2, 3};
                for (const std::size_t group_index : order) {
                    ImGui::TableNextColumn();
                    ImGui::PushID(static_cast<int>(group_index));
                    const domain::OfficerRelationGroup& group = relations->groups[group_index];
                    if (ImGui::BeginChild("##relation_card", ImVec2(0, relation_card_height), true)) {
                        ImGui::BeginDisabled(busy || !group.available);
                        if (menu_sprites::sprite_button(menu.ui_atlases, names[group_index],
                                ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetFrameHeight() * 1.4f))) {
                            menu.officer_relation_selection = domain::ReplaceOfficerRelation{
                                id, static_cast<domain::OfficerRelationKind>(group_index), 0, 0};
                            menu.officer_detail_open = false;
                            menu.officer_filter = -1;
                        }
                        ImGui::EndDisabled();
                        if (!group.available) {
                            ImGui::TextDisabled("조회 불가");
                        } else if (group.count == 0) {
                            ImGui::TextUnformatted("없음");
                        }
                        for (std::size_t index = 0; index < group.count; ++index) {
                            const domain::OfficerRelationMember& member = group.members[index];
                            const auto found = menu.officer_names ? menu.officer_names->find(member.id)
                                : domain::OfficerNames::const_iterator{};
                            const std::string label = menu.officer_names && found != menu.officer_names->end()
                                ? found->second : "무장 #" + std::to_string(member.id);
                            ImGui::PushID(static_cast<int>(member.id));
                            ImGui::BeginDisabled(busy);
                            if (ImGui::Selectable(label.c_str(), false, 0, ImVec2(0, ImGui::GetFrameHeight() * 1.3f))) {
                                menu.officer_relation_selection = domain::ReplaceOfficerRelation{
                                    id, static_cast<domain::OfficerRelationKind>(group_index), member.id, 0};
                                menu.officer_detail_open = false;
                                menu.officer_filter = -1;
                            }
                            ImGui::EndDisabled();
                            ImGui::PopID();
                        }
                    }
                    ImGui::EndChild();
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
            for (std::size_t group_index = 0; group_index < names.size(); ++group_index) {
                if (!protagonist && group_index < 4) continue;
                const auto& group = relations->groups[group_index];
                if (!group.available) {
                    ImGui::TextDisabled("%s: 조회 불가", names[group_index]);
                    continue;
                }
                ImGui::PushID(static_cast<int>(group_index));
                const std::string title = std::string(names[group_index]) + " (" +
                    std::to_string(group.count) + ")";
                if (!protagonist && group_index < 4) {
                    ImGui::BeginDisabled(busy);
                    if (menu_sprites::sprite_button(menu.ui_atlases, names[group_index],
                            ImVec2(ImGui::GetFontSize() * 6.0f, ImGui::GetFrameHeight()))) {
                        menu.officer_relation_selection = domain::ReplaceOfficerRelation{
                            id, static_cast<domain::OfficerRelationKind>(group_index), 0, 0};
                        menu.officer_detail_open = false;
                        menu.officer_filter = -1;
                    }
                    ImGui::EndDisabled();
                }
                if (group.count == 0) {
                    ImGui::TextUnformatted("없음");
                    ImGui::PopID();
                    continue;
                }
                if (ImGui::TreeNodeEx(title.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                    const float height = std::min(static_cast<float>(group.count + 1), 7.0f) *
                        ImGui::GetTextLineHeightWithSpacing() + 12 * menu.dpi_scale;
                    if (ImGui::BeginTable("##relations", 3, ImGuiTableFlags_RowBg |
                        ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_ScrollY, ImVec2(0, height))) {
                        ImGui::TableSetupColumn("무장");
                        ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 65 * menu.dpi_scale);
                        ImGui::TableSetupColumn("친밀도", ImGuiTableColumnFlags_WidthFixed, 70 * menu.dpi_scale);
                        ImGui::TableSetupScrollFreeze(0, 1);
                        ImGui::TableHeadersRow();
                        ImGuiListClipper clipper;
                        clipper.Begin(static_cast<int>(group.count));
                        while (clipper.Step()) {
                            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                                const auto& member = group.members[static_cast<std::size_t>(row)];
                                const auto found = menu.officer_names ? menu.officer_names->find(member.id)
                                    : domain::OfficerNames::const_iterator{};
                                const auto label = menu.officer_names && found != menu.officer_names->end()
                                    ? found->second : "무장 #" + std::to_string(member.id);
                                ImGui::PushID(static_cast<int>(member.id));
                                ImGui::TableNextRow();
                                ImGui::TableNextColumn();
                                ImGui::BeginDisabled(busy);
                                if (ImGui::Selectable(label.c_str())) {
                                    if (!protagonist && group_index < 4) {
                                        menu.officer_relation_selection = domain::ReplaceOfficerRelation{
                                            id, static_cast<domain::OfficerRelationKind>(group_index), member.id, 0};
                                        menu.officer_detail_open = false;
                                        menu.officer_filter = -1;
                                    } else {
                                        open_officer_details(menu, member.id);
                                    }
                                }
                                ImGui::EndDisabled();
                                ImGui::TableNextColumn();
                                ImGui::Text("%u", member.id);
                                ImGui::TableNextColumn();
                                if (member.affinity) {
                                    ImGui::Text("%u", *member.affinity);
                                } else {
                                    ImGui::TextUnformatted("-");
                                }
                                ImGui::PopID();
                            }
                        }
                        ImGui::EndTable();
                    }
                    ImGui::TreePop();
                }
                ImGui::PopID();
            }
        }
    }
    ImGui::PopID();
}

} // namespace

void draw_officer_roster(FeatureMenu& menu, services::UiBridge& bridge) {
    auto& mailbox = bridge.commands;
    if (!menu.officer_list_open) {
        return;
    }
    if (menu.officer_open_focus) {
        ImGui::SetNextWindowCollapsed(false);
        ImGui::SetNextWindowFocus();
        menu.officer_open_focus = false;
    }
    if (begin_editor_screen(menu, "모든 무장 편집 리스트###OfficerListWin", menu.officer_list_open, "편집할 무장을 선택해 주십시오")) {
        const int scan_cursor = bridge.officer_roster_scan_cursor.load(std::memory_order_acquire);
        const bool roster_available = menu.officer_roster->available && menu.officer_roster->epoch == menu.epoch;

        // Opening the window permits one automatic request. A failed read
        // remains idle until the user explicitly retries with the refresh button.
        if ((ImGui::IsWindowAppearing() || (menu.officer_open_request && !menu.officer_open_refresh_requested)) &&
            menu.ready && !menu.pending &&
            menu.pending_officer_edits.empty() && !roster_available && scan_cursor < 0) {
            const domain::RequestId request{++menu.next_request};
            const domain::Result<void> result = mailbox.push({request, menu.epoch, domain::RefreshOfficerRoster{}});

            if (result) {
                menu.pending = request;
                menu.officer_open_refresh_requested = menu.officer_open_request.has_value();
            } else {
                menu.error = result.error();
                menu.officer_open_refresh_requested = true;
            }
        }

        if (scan_cursor >= 0) {
            ImGui::Text("목록 로딩 중... (%d/%zu)", scan_cursor, domain::OFFICER_ROSTER_RECORD_COUNT);
        } else if (menu.officer_roster->loading && menu.officer_roster->epoch == menu.epoch) {
            ImGui::Text("목록 반영 중... (%zu명)", menu.officer_roster->entry_count);
        }
        if (menu.officer_open_request) {
            ImGui::Text("무장 #%u 상세를 준비하고 있습니다.", *menu.officer_open_request);
        }
        if (!menu.pending_officer_edits.empty()) {
            ImGui::Text("일괄 적용 중: %zu / %zu", menu.completed_officer_edits,
                        menu.pending_officer_edits.size());
        }
        draw_previous_request_error(menu);
        if (menu.officer_relation_selection) {
            ImGui::TextUnformatted("관계 대상으로 지정할 무장을 더블클릭하세요. 뒤로 버튼은 선택을 취소합니다.");
            ImGui::BeginDisabled(menu.pending.has_value() || !menu.ready || menu.officer_relation_selection->previous_id == 0);
            if (ImGui::Button("관계 해제")) {
                const domain::RequestId request{++menu.next_request};
                const domain::Result<void> result = mailbox.push({request, menu.epoch, *menu.officer_relation_selection});
                if (result) {
                    menu.pending = request;
                    menu.officer_relation_selection.reset();
                    menu.officer_detail_open = true;
                } else {
                    menu.error = result.error();
                }
            }
            ImGui::EndDisabled();
        }
        const auto select_officer = [&](std::uint16_t id) -> bool {
            if (menu.officer_relation_selection || menu.pending || !menu.pending_officer_edits.empty() || !menu.ready) {
                return false;
            }
            const domain::RequestId request{++menu.next_request};
            auto result = mailbox.push({request, menu.epoch, domain::SelectRosterOfficer{id}});
            if (result) {
                menu.pending = request;
            } else {
                menu.error = result.error();
            }
            return result.has_value();
        };
        const auto queue_officer_edits = [&](std::uint16_t primary, const auto& payload_for) {
            menu.pending_officer_edits.clear();
            menu.completed_officer_edits = 0;
            menu.failed_officer_edits = 0;
            menu.officer_edit_error.reset();
            menu.error.reset();
            menu.pending_officer_edits.push_back(payload_for(primary));
            for (std::size_t id = 1; id < menu.selected_officer_ids.size(); ++id) {
                if (menu.selected_officer_ids[id] && id != primary) {
                    menu.pending_officer_edits.push_back(payload_for(static_cast<std::uint16_t>(id)));
                }
            }
            menu.next_officer_edit = 0;
            menu.officer_edit_epoch = menu.epoch;
        };
        if (!menu.officer_detail_open) {
        ImGui::SetNextItemWidth(160);
        bool search = ImGui::InputTextWithHint("##officer_search", "이름 or ID", menu.officer_search.data(),
                                               menu.officer_search.size(), ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        search |= ImGui::Button("찾기");
        if (search && menu.officer_search[0]) {
            menu.officer_search_pending = true;
            menu.officer_pending_search = menu.officer_search.data();
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(menu.pending.has_value() || !menu.pending_officer_edits.empty() || !menu.ready);
        if (ImGui::Button("목록 갱신")) {
            const domain::RequestId request{++menu.next_request};
            auto result = mailbox.push({request, menu.epoch, domain::RefreshOfficerRoster{}});
            if (result) {
                menu.pending = request;
            } else {
                menu.error = result.error();
            }
        }
        ImGui::EndDisabled();
        if (!roster_available && scan_cursor < 0) {
            ImGui::TextDisabled("게임 준비 후 '목록 갱신'을 눌러 목록을 불러오세요.");
        }
        }
        if (menu.officer_roster->available && menu.officer_roster->epoch == menu.epoch) {
            struct Filter { const char* label; int status; };
            constexpr std::array filters{
                Filter{"군사", 0x18}, Filter{"일반", 0x28}, Filter{"두령", 0x38}, Filter{"동지", 0x48},
                Filter{"태수", 0xe8}, Filter{"도독", 0xd8}, Filter{"군주", 0xc8}, Filter{"재야", 0x58},
                Filter{"미발견", 0x68}, Filter{"사망", 0x88}, Filter{"NPC", 0x98}, Filter{"전부", -1},
            };
            ImGui::BeginDisabled(menu.pending.has_value() || !menu.pending_officer_edits.empty());
            for (std::size_t i = 0; !menu.officer_detail_open && i < filters.size(); ++i) {
                if (ImGui::Button(filters[i].label)) {
                    menu.officer_filter = filters[i].status;
                }
                if (i + 1 < filters.size()) {
                    ImGui::SameLine();
                }
            }
            ImGui::EndDisabled();
            const auto rebuild_filter = [&]() {
                menu.officer_sort_dirty = true;
                menu.officer_filtered_indices.clear();
                for (std::size_t i = 1; i < menu.officer_roster->entries.size(); ++i) {
                    const domain::OfficerRosterEntry& entry = menu.officer_roster->entries[i];
                    const std::uint8_t status = entry.status;

                    if (entry.id == 0) {
                        continue;
                    }
                    if (menu.officer_filter == -1 || status == menu.officer_filter ||
                        (menu.officer_filter == 0x68 && status == 0x78)) {
                        menu.officer_filtered_indices.push_back(i);
                    }
                }
                menu.officer_cached_filter = menu.officer_filter;
                menu.officer_filtered_generation = menu.officer_roster->generation;
                menu.officer_filtered_revision = menu.officer_roster->revision;
            };
            const bool filter_changed = menu.officer_cached_filter != menu.officer_filter ||
                menu.officer_filtered_generation != menu.officer_roster->generation ||
                menu.officer_filtered_revision != menu.officer_roster->revision;
            if (filter_changed) {
                rebuild_filter();
                menu.officer_reselect_pending = true;
            }
            const auto remember_next_officer = [&]() {
                menu.officer_next_selection = 0;
                if (!menu.snapshot.selected_officer) {
                    return;
                }
                const auto current = menu.snapshot.selected_officer->id;
                for (std::size_t i = 0; i < menu.officer_filtered_indices.size(); ++i) {
                    if (menu.officer_roster->entries[menu.officer_filtered_indices[i]].id == current) {
                        if (i + 1 < menu.officer_filtered_indices.size()) {
                            menu.officer_next_selection = menu.officer_roster->entries[menu.officer_filtered_indices[i + 1]].id;
                        } else if (i > 0) {
                            menu.officer_next_selection = menu.officer_roster->entries[menu.officer_filtered_indices[i - 1]].id;
                        }
                        break;
                    }
                }
            };
            if (menu.officer_filter == 0x68) {
                ImGui::BeginDisabled(menu.pending.has_value() || !menu.pending_officer_edits.empty() ||
                                     !menu.ready || menu.officer_roster->loading);
                const bool all_ronin = ImGui::Button("미발견 전부 재야");
                ImGui::SameLine();
                const bool selected_ronin = ImGui::Button("선택 무장 재야");
                if (all_ronin || selected_ronin) {
                    menu.pending_officer_edits.clear();
                    for (const auto& entry : menu.officer_roster->entries) {
                        if (entry.id == 0) {
                            continue;
                        }
                        if ((all_ronin && (entry.status == 0x68 || entry.status == 0x78)) ||
                            (selected_ronin && menu.selected_officer_ids[entry.id])) {
                            menu.pending_officer_edits.push_back(domain::SetRosterOfficerRonin{entry.id, all_ronin});
                        }
                    }
                    if (menu.pending_officer_edits.empty()) {
                        bridge.publish_notice("재야로 변경할 무장이 없습니다.");
                    } else {
                        menu.officer_next_selection = 0;
                        if (selected_ronin && menu.snapshot.selected_officer) {
                            const auto current = menu.snapshot.selected_officer->id;
                            for (std::size_t i = 0; i < menu.officer_filtered_indices.size(); ++i) {
                                if (menu.officer_roster->entries[menu.officer_filtered_indices[i]].id != current) {
                                    continue;
                                }
                                for (std::size_t j = i + 1; j < menu.officer_filtered_indices.size(); ++j) {
                                    const auto id = menu.officer_roster->entries[menu.officer_filtered_indices[j]].id;
                                    if (!menu.selected_officer_ids[id]) {
                                        menu.officer_next_selection = id;
                                        break;
                                    }
                                }
                                if (*menu.officer_next_selection == 0) {
                                    for (std::size_t j = i; j > 0; --j) {
                                        const auto id = menu.officer_roster->entries[menu.officer_filtered_indices[j - 1]].id;
                                        if (!menu.selected_officer_ids[id]) {
                                            menu.officer_next_selection = id;
                                            break;
                                        }
                                    }
                                }
                                break;
                            }
                        }
                        menu.completed_officer_edits = 0;
                        menu.failed_officer_edits = 0;
                        menu.officer_edit_error.reset();
                        menu.error.reset();
                        menu.next_officer_edit = 0;
                        menu.officer_edit_epoch = menu.epoch;
                        if (selected_ronin) {
                            menu.selected_officer_ids.fill(false);
                        }
                    }
                }
                ImGui::EndDisabled();
            }
            const auto selected_count = std::count(menu.selected_officer_ids.begin(), menu.selected_officer_ids.end(), true);
            if (!menu.officer_detail_open) {
            ImGui::SameLine();
            if (ImGui::Button(selected_count ? "선택 해제" : "전체 선택")) {
                if (selected_count) {
                    menu.selected_officer_ids.fill(false);
                } else {
                    for (const auto index : menu.officer_filtered_indices) {
                        menu.selected_officer_ids[menu.officer_roster->entries[index].id] = true;
                    }
                }
            }
            ImGui::SameLine();
            ImGui::Text("(%zu명)", static_cast<std::size_t>(selected_count));
            }
            if (menu.officer_open_request && !menu.pending && menu.pending_officer_edits.empty() &&
                menu.ready && !menu.officer_roster->loading && scan_cursor < 0) {
                const auto target = *menu.officer_open_request;
                menu.officer_filter = -1;
                rebuild_filter();
                const auto found = std::find_if(menu.officer_filtered_indices.begin(),
                    menu.officer_filtered_indices.end(), [&](std::size_t index) {
                        return menu.officer_roster->entries[index].id == target;
                    });
                if (found == menu.officer_filtered_indices.end()) {
                    bridge.publish_notice("해당 무장을 현재 목록에서 찾지 못했습니다. 목록 갱신 후 다시 선택하세요.");
                    menu.officer_open_request.reset();
                } else if (select_officer(target)) {
                    menu.officer_detail_open = true;
                    menu.officer_detail_tab = 0;
                    menu.officer_search_result = static_cast<int>(found - menu.officer_filtered_indices.begin());
                    menu.officer_open_request.reset();
                    ImGui::SetScrollY(0);
                }
                menu.officer_reselect_pending = false;
                menu.officer_next_selection.reset();
            }
            const bool run_search = !menu.officer_open_request && menu.officer_search_pending && !menu.pending &&
                                    menu.pending_officer_edits.empty() && menu.ready &&
                                    !menu.officer_roster->loading;
            if (run_search) {
                menu.officer_search_result = -1;
                const std::string_view query(menu.officer_pending_search);
                for (std::size_t i = 0; i < menu.officer_filtered_indices.size(); ++i) {
                    const auto id = menu.officer_roster->entries[menu.officer_filtered_indices[i]].id;
                    const auto name = menu.officer_names ? menu.officer_names->find(id) : domain::OfficerNames::const_iterator{};
                    if (std::to_string(id) == query || (menu.officer_names && name != menu.officer_names->end() &&
                        name->second.find(query) != std::string::npos)) {
                        menu.officer_search_result = static_cast<int>(i);
                        select_officer(id);
                        break;
                    }
                }
                if (menu.officer_search_result < 0 && menu.officer_filter != -1) {
                    for (const auto& entry : menu.officer_roster->entries) {
                        if (entry.id == 0) {
                            continue;
                        }
                        const auto name = menu.officer_names ? menu.officer_names->find(entry.id) : domain::OfficerNames::const_iterator{};
                        if (std::to_string(entry.id) == query || (menu.officer_names && name != menu.officer_names->end() &&
                            name->second.find(query) != std::string::npos)) {
                            menu.officer_filter = -1;
                            rebuild_filter();
                            for (std::size_t i = 0; i < menu.officer_filtered_indices.size(); ++i) {
                                if (menu.officer_roster->entries[menu.officer_filtered_indices[i]].id == entry.id) {
                                    menu.officer_search_result = static_cast<int>(i);
                                    select_officer(entry.id);
                                    break;
                                }
                            }
                            break;
                        }
                    }
                }
                menu.officer_search_pending = false;
                if (menu.officer_search_result >= 0) {
                    menu.officer_reselect_pending = false;
                }
            }
            const bool selected_visible = !menu.officer_open_request && menu.snapshot.selected_officer &&
                std::any_of(menu.officer_filtered_indices.begin(), menu.officer_filtered_indices.end(), [&](std::size_t index) {
                    return menu.officer_roster->entries[index].id == menu.snapshot.selected_officer->id;
                });
            if (menu.officer_reselect_pending && !menu.officer_open_request && !menu.officer_search_pending &&
                !menu.pending && menu.pending_officer_edits.empty() && menu.ready &&
                !menu.officer_roster->loading && scan_cursor < 0) {
                if ((!selected_visible || menu.officer_next_selection) && !menu.officer_filtered_indices.empty()) {
                    auto next = menu.officer_roster->entries[menu.officer_filtered_indices.front()].id;
                    if (menu.officer_next_selection) {
                        for (const auto index : menu.officer_filtered_indices) {
                            if (menu.officer_roster->entries[index].id == *menu.officer_next_selection) {
                                next = *menu.officer_next_selection;
                                break;
                            }
                        }
                    }
                    select_officer(next);
                }
                menu.officer_next_selection.reset();
                menu.officer_reselect_pending = false;
            }
            if (!menu.officer_detail_open) ImGui::Text("무장 %zu명", menu.officer_filtered_indices.size());
            if (selected_visible && menu.officer_detail_open) {
                const auto& officer = *menu.snapshot.selected_officer;
                bridge.officer_detail_visible.store(true);
                constexpr std::array detail_tabs{"기본", "기능", "관계", "명성"};
                for (int tab = 0; tab < 4; ++tab) {
                    if (tab != 0) ImGui::SameLine();
                    if (menu_sprites::navigation_row(menu.ui_atlases, detail_tabs[tab],
                        ImVec2(ImGui::GetFontSize() * 4.0f, ImGui::GetFrameHeight() * 1.4f), menu.officer_detail_tab == tab)) {
                        menu.officer_detail_tab = tab;
                    }
                }
                ImGui::Separator();
                if (menu.officer_detail_tab == 2) {
                    draw_officer_inspection(menu, bridge, officer.id, false);
                }
                if (menu.officer_detail_tab == 0) {
                ImGui::BeginChild("##officer_profile", ImVec2(ImGui::GetContentRegionAvail().x * 0.48f, 0), true);
                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.65f);
                ImGui::Text("선택: #%u", static_cast<unsigned>(officer.id));
                const char* status_name = nullptr;
                switch (officer.status) {
                case 0x18: status_name = "군사"; break;
                case 0x28: status_name = "일반"; break;
                case 0x38: status_name = "두령"; break;
                case 0x48: status_name = "동지"; break;
                case 0x58: status_name = "재야"; break;
                case 0x68: case 0x78: status_name = "미발견"; break;
                case 0x88: status_name = "사망"; break;
                case 0x98: status_name = "NPC"; break;
                case 0xc8: status_name = "군주"; break;
                case 0xd8: status_name = "도독"; break;
                case 0xe8: status_name = "태수"; break;
                }
                if (status_name) {
                    ImGui::Text("무장 상태: %s", status_name);
                } else {
                    ImGui::Text("무장 상태: 기타 (0x%02X)", static_cast<unsigned>(officer.status));
                }
                draw_officer_name(menu, bridge, officer.id, false);
                const auto family_name = [&](std::uint16_t id) -> const char* {
                    if (!id) {
                        return "없음";
                    }
                    if (menu.officer_names) {
                        const auto found = menu.officer_names->find(id);
                        if (found != menu.officer_names->end()) {
                            return found->second.c_str();
                        }
                    }
                    return "알수없음";
                };
                ImGui::Text("가문: %s | 부: %s | 모: %s", family_name(officer.family[0]),
                            family_name(officer.family[1]), family_name(officer.family[2]));
                constexpr std::array city_names{
                    "양평", "북평", "계", "남피", "업", "평원", "북해", "제남", "진양", "상당",
                    "하비", "소패", "복양", "진류", "허창", "초", "여남", "낙양", "홍농", "장안",
                    "천수", "무위", "광릉", "서평", "한중", "무도", "성도", "영안", "자동", "강주",
                    "영창", "완", "신야", "양양", "강하", "상용", "강릉", "장사", "영릉", "무릉",
                    "계양", "수춘", "여강", "말릉/건업", "오", "회계", "시상", "파양", "건녕", "운남", "교지"};
                ImGui::Text("거주 도시: %s", officer.city ? city_names[*officer.city] : "정보 없음");
                if (officer.status == 0x58) {
                    ImGui::BeginDisabled(menu.pending.has_value() || !menu.pending_officer_edits.empty() || !menu.ready);
                    ImGui::SetNextItemWidth(150);
                    ImGui::Combo("이동 도시", &menu.officer_target_city, city_names.data(), static_cast<int>(city_names.size()));
                    ImGui::SameLine();
                    if (ImGui::Button("선택도시로 이동")) {
                        const domain::RequestId request{++menu.next_request};
                        auto result = mailbox.push({request, menu.epoch,
                            domain::MoveRosterOfficer{officer.id, static_cast<std::uint8_t>(menu.officer_target_city)}});
                        if (result) {
                            menu.pending = request;
                            menu.pending_notice = "선택한 도시로 이동했습니다.";
                        } else {
                            menu.error = result.error();
                        }
                    }
                    ImGui::EndDisabled();
                }
                if (officer.status == 0x88) {
                    ImGui::BeginDisabled(menu.pending.has_value() || !menu.pending_officer_edits.empty() || !menu.ready);
                    const bool revive_here = ImGui::Button("주인공 도시에서 부활");
                    ImGui::SetNextItemWidth(150);
                    ImGui::Combo("부활 도시", &menu.officer_target_city, city_names.data(), static_cast<int>(city_names.size()));
                    ImGui::SameLine();
                    const bool revive_selected = ImGui::Button("선택 도시에서 부활");
                    if (revive_here || revive_selected) {
                        const domain::RequestId request{++menu.next_request};
                        const auto city = revive_selected ? std::optional<std::uint8_t>{static_cast<std::uint8_t>(menu.officer_target_city)}
                                                          : std::nullopt;
                        auto result = mailbox.push({request, menu.epoch, domain::ReviveRosterOfficer{officer.id, city}});
                        if (result) {
                            menu.pending = request;
                            remember_next_officer();
                            menu.pending_notice = "무장을 부활했습니다. 저장 후 불러오면 게임에 반영됩니다.";
                        } else {
                            menu.error = result.error();
                        }
                    }
                    ImGui::EndDisabled();
                }
                if (officer.corps_number) {
                    std::string governor = "없음";
                    if (officer.corps_governor) {
                        governor = "무장 ID " + std::to_string(officer.corps_governor);
                        if (menu.officer_names) {
                            const auto found = menu.officer_names->find(officer.corps_governor);
                            if (found != menu.officer_names->end()) {
                                governor = found->second;
                            }
                        }
                    }
                    ImGui::Text("소속 군단: %llu군단 · 도독 %s",
                                static_cast<unsigned long long>(*officer.corps_number), governor.c_str());
                } else {
                    ImGui::TextUnformatted("소속 군단: 직할 / 군단 없음");
                }
                constexpr std::array tendencies{"?", "소극", "보통", "호전", "적극", "사욕"};
                constexpr std::array personalities{"?", "대담", "저돌", "온화", "침착", "나약"};
                const auto tendency = officer.strategic_tendency < tendencies.size() ? officer.strategic_tendency : 0;
                const auto personality = officer.personality < personalities.size() ? officer.personality : 0;
                int gender = officer.gender == 1 ? 1 : 0;
                constexpr std::array gender_names{"남성", "여성"};
                ImGui::BeginDisabled(menu.pending.has_value() || !menu.pending_officer_edits.empty() || !menu.ready);
                if ((officer.status == 0x68 || officer.status == 0x78) && ImGui::Button("재야로 변경")) {
                    const domain::RequestId request{++menu.next_request};
                    auto result = mailbox.push({request, menu.epoch, domain::SetRosterOfficerRonin{officer.id}});
                    if (result) {
                        menu.pending = request;
                        remember_next_officer();
                        menu.pending_notice = "미발견 무장을 재야로 변경했습니다. 저장 후 불러오면 게임에 반영됩니다.";
                    } else {
                        menu.error = result.error();
                    }
                }
                if (ImGui::Combo("성별", &gender, gender_names.data(), static_cast<int>(gender_names.size()))) {
                    const domain::RequestId request{++menu.next_request};
                    auto result = mailbox.push({request, menu.epoch,
                                                domain::SetRosterOfficerGender{officer.id, static_cast<std::uint8_t>(gender)}});
                    if (result) {
                        menu.pending = request;
                    } else {
                        menu.error = result.error();
                    }
                }
                constexpr std::array voice_names{
                    "없음", "난폭남", "탐욕남", "호걸남", "정열남", "노장남", "용장남", "과묵남",
                    "성숙남", "청년", "중년", "소심남", "정중남", "우유부단남", "책사남", "거만남",
                    "교활남", "호방녀", "용감녀", "보통녀", "현명녀", "요염녀", "거만녀"};
                int voice = officer.voice < voice_names.size() ? officer.voice : 0;
                if (ImGui::Combo("목소리", &voice, voice_names.data(), static_cast<int>(voice_names.size()))) {
                    const domain::RequestId request{++menu.next_request};
                    auto result = mailbox.push({request, menu.epoch,
                                                domain::SetRosterOfficerVoice{officer.id, static_cast<std::uint8_t>(voice)}});
                    if (result) {
                        menu.pending = request;
                    } else {
                        menu.error = result.error();
                    }
                }
                ImGui::EndDisabled();
                ImGui::Text("전략: %s  성격: %s", tendencies[tendency], personalities[personality]);
                constexpr std::array interest_names{"술", "무구", "서적", "보물"};
                std::string interests;
                for (std::size_t i = 0; i < interest_names.size(); ++i) {
                    if (officer.interests & (1u << i)) {
                        if (!interests.empty()) {
                            interests += " / ";
                        }
                        interests += interest_names[i];
                    }
                }
                ImGui::Text("흥미: %s", interests.empty() ? "없음" : interests.c_str());
                constexpr std::array priority_names{"알 수 없음", "무명", "문명", "문무불문", "악명", "무관심", "고명"};
                constexpr std::array greed_names{"알 수 없음", "무욕", "보통", "탐욕"};
                if (officer.priority > 0 && officer.priority < priority_names.size()) {
                    ImGui::Text("중시: %s", priority_names[officer.priority]);
                } else {
                    ImGui::Text("중시: 알 수 없음 (%u)", static_cast<unsigned>(officer.priority));
                }
                if (officer.greed > 0 && officer.greed < greed_names.size()) {
                    ImGui::Text("물욕: %s", greed_names[officer.greed]);
                } else {
                    ImGui::Text("물욕: 알 수 없음 (%u)", static_cast<unsigned>(officer.greed));
                }
                draw_officer_traits(menu, mailbox, officer.id, officer.traits, false);
                if (menu.snapshot.ai_growth_preview && menu.snapshot.ai_growth_preview->officer_id == officer.id) {
                    draw_ai_growth_preview(*menu.snapshot.ai_growth_preview);
                }
                ImGui::PopItemWidth();
                ImGui::EndChild();
                ImGui::SameLine();
                }
                constexpr std::array labels{"통솔", "무력", "지력", "정치", "매력", "문명", "무명", "악명",
                                            "공적", "봉록", "충성", "전략포인트", "행동력",
                                            "얼굴 번호", "모델 번호", "모델 색상", "등장년도", "생년", "몰년(수명)"};
                if (menu.selected_basic_id != officer.id) {
                    menu.selected_basic_id = officer.id;
                    menu.selected_basic_editing.fill(false);
                    menu.selected_aptitude_editing.fill(false);
                }
                if (menu.officer_detail_tab == 0 || menu.officer_detail_tab == 3) {
                    ImGui::BeginDisabled(menu.pending.has_value() || !menu.pending_officer_edits.empty() || !menu.ready);
                    if (menu.officer_detail_tab == 3) {
                        constexpr std::array priorities{"무명", "문명", "문무불문", "악명", "무관심", "고명"};
                        int priority = static_cast<int>(officer.priority) - 1;
                        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8.0f);
                        if (ImGui::Combo("중시 명성", &priority, priorities.data(), static_cast<int>(priorities.size()))) {
                            queue_officer_edits(officer.id, [&](std::uint16_t id) -> domain::CommandPayload {
                                return domain::SetRosterOfficerPriority{id, static_cast<std::uint8_t>(priority + 1)};
                            });
                        }
                    }
                    if (ImGui::BeginChild("##selected_officer_basic", ImVec2(0, 0), true)) {
                        for (std::size_t i = 0; i < labels.size(); ++i) {
                            const bool fame = i >= 5 && i <= 7;
                            if (fame != (menu.officer_detail_tab == 3)) continue;
                            auto& draft = menu.selected_basic_draft[i];
                            if (!menu.selected_basic_editing[i]) {
                                draft = static_cast<int>(officer.basic[i]);
                            }
                            ImGui::PushID(static_cast<int>(i));
                            ImGui::TextUnformatted(labels[i]);
                            ImGui::SameLine();
                            bool changed = false;
                            if (ImGui::Button("-")) {
                                --draft;
                                changed = true;
                            }
                            ImGui::SameLine();
                            ImGui::SetNextItemWidth(100);
                            ImGui::InputInt("##value", &draft, 0, 0, ImGuiInputTextFlags_CharsDecimal);
                            menu.selected_basic_editing[i] = ImGui::IsItemActive();
                            if (ImGui::IsItemDeactivatedAfterEdit()) {
                                changed = true;
                            }
                            ImGui::SameLine();
                            if (ImGui::Button("+")) {
                                ++draft;
                                changed = true;
                            }
                            if (changed) {
                                queue_officer_edits(officer.id, [&](std::uint16_t id) -> domain::CommandPayload {
                                    return domain::SetRosterOfficerBasic{id, static_cast<std::uint8_t>(i),
                                                                         static_cast<std::uint32_t>(draft)};
                                });
                            }
                            ImGui::PopID();
                        }
                    }
                    ImGui::EndChild();
                    ImGui::EndDisabled();
                }
                if (menu.officer_detail_tab == 1) {
                    ImGui::BeginDisabled(menu.pending.has_value() || !menu.pending_officer_edits.empty() || !menu.ready);
                    if (ImGui::BeginChild("##selected_officer_training", ImVec2(0, 0), true)) {
                        const auto submit_training = [&](domain::OfficerTrainingKind kind, std::uint8_t index, std::uint8_t value) {
                            queue_officer_edits(officer.id, [&](std::uint16_t id) -> domain::CommandPayload {
                                return domain::SetRosterOfficerTraining{id, kind, index, value};
                            });
                        };
                        const auto submit_count = [&](std::uintptr_t offset, int count) {
                            queue_officer_edits(officer.id, [&](std::uint16_t id) -> domain::CommandPayload {
                                return domain::SetSkillCount{id, offset, count};
                            });
                        };
                        constexpr std::array tactic_names{
                            "강격", "난격", "교란", "맹돌", "창금", "연격", "돌격", "급습", "기사", "차현",
                            "제사", "난사", "화시", "원사", "시람", "화전", "난전", "연환", "돌관", "폭선",
                            "열화", "격류", "낙석", "요격", "동토", "분기", "고무", "매성", "치료", "천계",
                            "풍변", "천변", "요술", "환술", "낙뢰", "충차", "정란", "투석", "운제", "상병"};
                        ImGui::SeparatorText("전법 · 버튼을 누르면 Lv +1 / 사용 횟수는 Enter로 적용");
                        constexpr std::array tactic_groups{"보병", "기병", "궁병", "함선", "군략", "보조", "둔갑", "병기"};
                        if (ImGui::BeginTable("##tactic_grid", 6, ImGuiTableFlags_SizingStretchSame)) {
                            ImGui::TableSetupColumn("계통", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 2.5f);
                            for (std::size_t i = 0; i < tactic_names.size(); ++i) {
                                if (i % 5 == 0) {
                                    ImGui::TableNextRow();
                                    ImGui::TableSetColumnIndex(0);
                                    ImGui::TextUnformatted(tactic_groups[i / 5]);
                                }
                                ImGui::TableSetColumnIndex(static_cast<int>(i % 5 + 1));
                                ImGui::PushID(static_cast<int>(i));
                                const float count_width = ImGui::CalcTextSize("99").x + ImGui::GetStyle().FramePadding.x * 2.0f;
                                if (i < officer.tactic_levels.size()) {
                                    const std::uint8_t level = officer.tactic_levels[i];
                                    if (menu_sprites::sprite_button(menu.ui_atlases, tactic_names[i],
                                            ImVec2(ImGui::GetContentRegionAvail().x - count_width - ImGui::GetStyle().ItemSpacing.x, ImGui::GetFrameHeight()))) {
                                        submit_training(domain::OfficerTrainingKind::Tactic, static_cast<std::uint8_t>(i),
                                                        static_cast<std::uint8_t>((level + 1) % 4));
                                    }
                                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Lv %u / 3", static_cast<unsigned>(level));
                                    ImGui::SameLine();
                                } else {
                                    ImGui::TextUnformatted(tactic_names[i]);
                                    ImGui::SameLine();
                                }
                                int count = 0;
                                if (menu.skill_counts) {
                                    const auto found = menu.skill_counts->find(officer.id);
                                    if (found != menu.skill_counts->end()) {
                                        const auto skill = found->second.find(0x139 + i);
                                        if (skill != found->second.end() && skill->second > 0) {
                                            count = skill->second;
                                        }
                                    }
                                }
                                ImGui::BeginDisabled(!menu.skill_counts);
                                ImGui::SetNextItemWidth(ImGui::CalcTextSize("99").x + ImGui::GetStyle().FramePadding.x * 2.0f);
                                if (ImGui::InputInt("##count", &count, 0, 0, ImGuiInputTextFlags_EnterReturnsTrue)) {
                                    submit_count(0x139 + i, std::clamp(count, 0, 99));
                                }
                                if (ImGui::IsItemHovered()) {
                                    ImGui::SetTooltip("사용 횟수 1~99, 0은 지정 해제. Enter로 적용합니다.");
                                }
                                ImGui::EndDisabled();
                                ImGui::PopID();
                            }
                            ImGui::EndTable();
                        }
                        constexpr std::array skill_names{
                            "경작", "상재", "축성", "경비", "발명", "천성", "교섭", "허보", "공작", "화술", "열변", "귀모",
                            "보장", "기장", "궁장", "수군", "조기", "신산", "원호", "파성", "행군", "여력", "과감", "위풍"};
                        ImGui::SeparatorText("특기 · 버튼을 누르면 Lv +1");
                        constexpr std::array skill_groups{"임무", "지모", "병과", "군사"};
                        if (ImGui::BeginTable("##skill_grid", 7, ImGuiTableFlags_SizingStretchSame)) {
                            ImGui::TableSetupColumn("계통", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 2.5f);
                            for (std::size_t i = 0; i < skill_names.size(); ++i) {
                                if (i % 6 == 0) {
                                    ImGui::TableNextRow();
                                    ImGui::TableSetColumnIndex(0);
                                    ImGui::TextUnformatted(skill_groups[i / 6]);
                                }
                                ImGui::TableSetColumnIndex(static_cast<int>(i % 6 + 1));
                                ImGui::PushID(static_cast<int>(i));
                                const std::uint8_t level = officer.skill_levels[i];
                                if (menu_sprites::sprite_button(menu.ui_atlases, skill_names[i],
                                        ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetFrameHeight()))) {
                                    submit_training(domain::OfficerTrainingKind::Skill, static_cast<std::uint8_t>(i),
                                                    static_cast<std::uint8_t>((level + 1) % 4));
                                }
                                ImGui::Text("Lv%u", static_cast<unsigned>(level));
                                ImGui::PopID();
                            }
                            ImGui::EndTable();
                        }
                        constexpr std::array aptitude_names{"보병", "기병", "궁병", "함선", "군략", "보조", "둔갑", "임무", "지모", "병과", "군사"};
                        if (ImGui::TreeNode("소양")) {
                            for (std::size_t i = 0; i < aptitude_names.size(); ++i) {
                                auto& draft = menu.selected_aptitude_draft[i];
                                if (!menu.selected_aptitude_editing[i]) {
                                    draft = officer.aptitudes[i];
                                }
                                ImGui::PushID(static_cast<int>(i));
                                ImGui::TextUnformatted(aptitude_names[i]);
                                ImGui::SameLine();
                                bool changed = false;
                                if (ImGui::Button("-")) {
                                    --draft;
                                    changed = true;
                                }
                                ImGui::SameLine();
                                ImGui::SetNextItemWidth(70);
                                ImGui::InputInt("##value", &draft, 0, 0, ImGuiInputTextFlags_CharsDecimal);
                                menu.selected_aptitude_editing[i] = ImGui::IsItemActive();
                                if (ImGui::IsItemDeactivatedAfterEdit()) {
                                    changed = true;
                                }
                                ImGui::SameLine();
                                if (ImGui::Button("+")) {
                                    ++draft;
                                    changed = true;
                                }
                                if (changed) {
                                    submit_training(domain::OfficerTrainingKind::Aptitude,
                                        static_cast<std::uint8_t>(i), static_cast<std::uint8_t>(draft));
                                }
                                ImGui::PopID();
                            }
                            ImGui::TreePop();
                        }
                        constexpr std::array ability_names{"군악대", "무쌍 보병", "불꽃 기병", "원격 궁병", "등갑군",
                            "총사령관", "기습부대", "대군사", "무신", "함선 병기화"};
                        if (ImGui::TreeNode("특수 능력")) {
                            ImGui::BeginDisabled(!menu.skill_counts);
                            for (std::size_t i = 0; i < ability_names.size(); ++i) {
                                bool enabled = false;
                                if (menu.skill_counts) {
                                    const auto found = menu.skill_counts->find(officer.id);
                                    if (found != menu.skill_counts->end()) {
                                        const auto skill = found->second.find(0x1000 + i);
                                        enabled = skill != found->second.end() && skill->second > 0;
                                    }
                                }
                                if (ImGui::Checkbox(ability_names[i], &enabled)) {
                                    submit_count(0x1000 + i, enabled ? 1 : 0);
                                }
                            }
                            ImGui::EndDisabled();
                            ImGui::TreePop();
                        }
                    }
                    ImGui::EndChild();
                    ImGui::EndDisabled();
                }
            }
            if (!menu.officer_detail_open) {
            ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, IM_COL32(139, 112, 51, 255));
            ImGui::PushStyleColor(ImGuiCol_Header, IM_COL32(244, 207, 87, 210));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(255, 226, 111, 230));
            ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(10 * menu.dpi_scale, 5 * menu.dpi_scale));
            if (ImGui::BeginTable("##officer_roster", 4,
                    ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_Sortable)) {
                ImGui::TableSetupColumn("선택", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort, 48 * menu.dpi_scale);
                ImGui::TableSetupColumn("번호", ImGuiTableColumnFlags_WidthFixed, 70 * menu.dpi_scale);
                ImGui::TableSetupColumn("무장", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("상태", ImGuiTableColumnFlags_WidthFixed, 110 * menu.dpi_scale);
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 245, 211, 255));
                ImGui::TableHeadersRow();
                ImGui::PopStyleColor();

                ImGuiTableSortSpecs* sort = ImGui::TableGetSortSpecs();
                if (sort && sort->SpecsCount > 0 && (sort->SpecsDirty || menu.officer_sort_dirty)) {
                    const int column = sort->Specs[0].ColumnIndex;
                    const bool descending = sort->Specs[0].SortDirection == ImGuiSortDirection_Descending;
                    std::sort(menu.officer_filtered_indices.ids.begin(),
                        menu.officer_filtered_indices.ids.begin() + menu.officer_filtered_indices.count,
                        [&](std::size_t left, std::size_t right) {
                            const domain::OfficerRosterEntry& a = menu.officer_roster->entries[left];
                            const domain::OfficerRosterEntry& b = menu.officer_roster->entries[right];
                            int order = static_cast<int>(a.id) - static_cast<int>(b.id);
                            if (column == 3 && a.status != b.status) {
                                order = static_cast<int>(a.status) - static_cast<int>(b.status);
                            } else if (column == 2 && menu.officer_names) {
                                const auto an = menu.officer_names->find(a.id);
                                const auto bn = menu.officer_names->find(b.id);
                                if (an != menu.officer_names->end() && bn != menu.officer_names->end()) {
                                    const int name_order = an->second.compare(bn->second);
                                    if (name_order != 0) order = name_order;
                                }
                            }
                            return descending ? order > 0 : order < 0;
                        });
                    sort->SpecsDirty = false;
                    menu.officer_sort_dirty = false;
                }

                const float row_height = ImGui::GetFrameHeight() + ImGui::GetStyle().CellPadding.y * 2;
                if (run_search && menu.officer_search_result >= 0) {
                    ImGui::SetScrollY(menu.officer_search_result * row_height);
                }
                ImGuiListClipper clipper;
                clipper.Begin(static_cast<int>(menu.officer_filtered_indices.size()), row_height);
                while (clipper.Step()) {
                    for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                        const auto& officer = menu.officer_roster->entries[menu.officer_filtered_indices[i]];
                        ImGui::TableNextRow(ImGuiTableRowFlags_None, row_height);
                        ImGui::TableSetColumnIndex(0);
                        ImGui::PushID(static_cast<int>(officer.id));
                        ImGui::Checkbox("##multi", &menu.selected_officer_ids[officer.id]);
                        ImGui::TableSetColumnIndex(1);
                        ImGui::Text("%u", static_cast<unsigned>(officer.id));
                        ImGui::TableSetColumnIndex(2);
                        const auto name = menu.officer_names ? menu.officer_names->find(officer.id)
                                                             : domain::OfficerNames::const_iterator{};
                        const char* display = menu.officer_names && name != menu.officer_names->end()
                            ? name->second.c_str() : "";
                        const bool protagonist = menu.snapshot.protagonist_id == officer.id;
                        const std::string label = (protagonist ? std::string("★ ") : std::string{}) +
                            display + (protagonist ? " (주인공)" : "");
                        const bool selected = menu.snapshot.selected_officer &&
                            menu.snapshot.selected_officer->id == officer.id;
                        ImGui::BeginDisabled(!menu.pending_officer_edits.empty() || !menu.ready);
                        if (menu.officer_name_edit_id == officer.id && menu.officer_name_edit_list) {
                            draw_officer_name_input(menu, bridge, officer.id);
                        } else {
                            const bool clicked = ImGui::Selectable(label.c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick, ImVec2(0, ImGui::GetFrameHeight()));
                            if (clicked && ImGui::IsMouseDoubleClicked(0) && !menu.officer_name_save_pending) {
                                if (menu.officer_relation_selection && !menu.pending) {
                                    domain::ReplaceOfficerRelation command = *menu.officer_relation_selection;
                                    command.replacement_id = officer.id;
                                    const domain::RequestId request{++menu.next_request};
                                    const domain::Result<void> result = mailbox.push({request, menu.epoch, command});
                                    if (result) {
                                        menu.pending = request;
                                        menu.officer_relation_selection.reset();
                                        menu.officer_detail_open = true;
                                    } else {
                                        menu.error = result.error();
                                    }
                                } else if (!menu.officer_relation_selection && (selected || select_officer(officer.id))) {
                                    menu.officer_detail_open = true;
                                    menu.officer_detail_tab = 0;
                                }
                            } else if (!menu.officer_relation_selection && (clicked || (ImGui::IsItemFocused() && !selected))) {
                                select_officer(officer.id);
                            }
                        }
                        ImGui::EndDisabled();
                        if (selected) {
                            ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(255, 218, 91, 180));
                        }
                        ImGui::TableSetColumnIndex(3);
                        switch (officer.status) {
                        case 0x18: ImGui::TextUnformatted("군사"); break;
                        case 0x28: ImGui::TextUnformatted("일반"); break;
                        case 0x38: ImGui::TextUnformatted("두령"); break;
                        case 0x48: ImGui::TextUnformatted("동지"); break;
                        case 0x58: ImGui::TextUnformatted("재야"); break;
                        case 0x68: case 0x78: ImGui::TextUnformatted("미발견"); break;
                        case 0x88: ImGui::TextUnformatted("사망"); break;
                        case 0x98: ImGui::TextUnformatted("NPC"); break;
                        case 0xc8: ImGui::TextUnformatted("군주"); break;
                        case 0xd8: ImGui::TextUnformatted("도독"); break;
                        case 0xe8: ImGui::TextUnformatted("태수"); break;
                        default: ImGui::Text("0x%02X", static_cast<unsigned>(officer.status)); break;
                        }
                        ImGui::PopID();
                    }
                }
                ImGui::EndTable();
            }
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(3);
            }
        } else {
            ImGui::TextUnformatted("목록 갱신을 눌러 무장을 읽어오세요.");
        }
    }
    end_editor_screen();
}

void draw_protagonist_abilities(FeatureMenu& menu, services::UiBridge& bridge) {
    auto& mailbox = bridge.commands;
    if (!menu.protagonist_detail) {
        return;
    }
    if (begin_editor_screen(menu, "주인공 무장 상세 편집###OffDetailWin", menu.protagonist_detail, "무장 데이터를 설정해 주십시오", true)) {
        if (menu.snapshot.protagonist_id) {
            bridge.protagonist_detail_visible.store(true);
            draw_officer_name(menu, bridge, *menu.snapshot.protagonist_id, true);
            draw_officer_inspection(menu, bridge, *menu.snapshot.protagonist_id, true);
            if (menu.snapshot.protagonist_traits) {
                draw_officer_traits(menu, mailbox, *menu.snapshot.protagonist_id, *menu.snapshot.protagonist_traits, true);
            }
        }
        if (!menu.pending_officer_edits.empty()) {
            ImGui::Text("일괄 적용 중: %zu / %zu", menu.completed_officer_edits, menu.pending_officer_edits.size());
        }
        draw_previous_request_error(menu);
        const auto queue_hero_edits = [&](domain::CommandPayload primary, const auto& payload_for) {
            menu.pending_officer_edits.clear();
            menu.completed_officer_edits = 0;
            menu.failed_officer_edits = 0;
            menu.officer_edit_error.reset();
            menu.error.reset();
            menu.pending_officer_edits.push_back(std::move(primary));
            for (std::size_t id = 1; id < menu.selected_officer_ids.size(); ++id) {
                if (menu.selected_officer_ids[id] && id != *menu.snapshot.protagonist_id) {
                    menu.pending_officer_edits.push_back(payload_for(static_cast<std::uint16_t>(id)));
                }
            }
            menu.next_officer_edit = 0;
            menu.officer_edit_epoch = menu.epoch;
        };
        ImGui::TextUnformatted("특수 능력 설정");
        const bool disabled = !menu.ready || !menu.snapshot.protagonist_id || menu.pending.has_value() ||
                              !menu.pending_officer_edits.empty();
        if (disabled) {
            ImGui::BeginDisabled();
        }
        ImGui::BeginDisabled(!menu.snapshot.protagonist_gender);
        int gender = menu.snapshot.protagonist_gender == 1 ? 1 : 0;
        constexpr std::array gender_names{"남성", "여성"};
        if (ImGui::Combo("성별##protagonist", &gender, gender_names.data(), static_cast<int>(gender_names.size()))) {
            const domain::RequestId request{++menu.next_request};
            auto result = mailbox.push({request, menu.epoch,
                                        domain::SetProtagonistGender{*menu.snapshot.protagonist_id, static_cast<std::uint8_t>(gender)}});
            if (result) {
                menu.pending = request;
            } else {
                menu.error = result.error();
            }
        }
        ImGui::EndDisabled();
        if (ImGui::CollapsingHeader("능력/상태")) {
            constexpr std::array names{"통솔", "무력", "지력", "정치", "매력", "문명", "무명", "악명",
                                       "공적", "봉록", "충성", "전략포인트", "행동력",
                                       "얼굴 번호", "모델 번호", "모델 색상", "등장년도", "생년", "몰년(수명)"};
            ImGui::BeginDisabled(!menu.snapshot.protagonist_basic);
            for (std::size_t i = 0; i < names.size(); ++i) {
                auto& draft = menu.protagonist_basic_draft[i];
                if (!menu.protagonist_basic_editing[i] && menu.snapshot.protagonist_basic) {
                    draft = static_cast<int>((*menu.snapshot.protagonist_basic)[i]);
                }
                ImGui::PushID(static_cast<int>(0x500 + i));
                ImGui::TextUnformatted(names[i]);
                ImGui::SameLine();
                bool changed = false;
                if (ImGui::Button("-")) {
                    --draft;
                    changed = true;
                }
                ImGui::SameLine();
                ImGui::SetNextItemWidth(100);
                ImGui::InputInt("##basic", &draft, 0, 0, ImGuiInputTextFlags_CharsDecimal);
                menu.protagonist_basic_editing[i] = ImGui::IsItemActive();
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    changed = true;
                }
                ImGui::SameLine();
                if (ImGui::Button("+")) {
                    ++draft;
                    changed = true;
                }
                if (changed && !disabled && menu.snapshot.protagonist_basic) {
                    const auto index = static_cast<std::uint8_t>(i);
                    const auto value = static_cast<std::uint32_t>(draft);
                    queue_hero_edits(domain::SetProtagonistBasic{*menu.snapshot.protagonist_id, index, value},
                        [&](std::uint16_t id) -> domain::CommandPayload {
                            return domain::SetRosterOfficerBasic{id, index, value};
                        });
                }
                ImGui::PopID();
            }
            ImGui::EndDisabled();
        }
        if (ImGui::CollapsingHeader("전법 사용 횟수")) {
            struct TacticGroup {
                const char* label;
                std::uintptr_t first_offset;
                std::array<const char*, 5> names;
            };
            constexpr std::array groups{
                TacticGroup{"보병", 0x139, {"강격", "난격", "교란", "맹돌", "창금"}},
                TacticGroup{"기병", 0x13e, {"연격", "돌격", "급습", "기사", "차현"}},
                TacticGroup{"궁병", 0x143, {"제사", "난사", "화시", "원사", "시람"}},
                TacticGroup{"함선", 0x148, {"화전", "난전", "연환", "돌관", "폭선"}},
                TacticGroup{"군략", 0x14d, {"열화", "격류", "낙석", "요격", "동토"}},
                TacticGroup{"보조", 0x152, {"분기", "고무", "매성", "치료", "천계"}},
                TacticGroup{"둔갑", 0x157, {"풍변", "천변", "요술", "환술", "낙뢰"}},
                TacticGroup{"병기", 0x15c, {"충차", "정란", "투석", "운제", "상병"}},
            };
            for (const auto& group : groups) {
                ImGui::TextUnformatted(group.label);
                for (std::size_t i = 0; i < group.names.size(); ++i) {
                    const auto offset = group.first_offset + i;
                    int count = 0;
                    if (menu.snapshot.protagonist_id && menu.skill_counts) {
                        const auto officer = menu.skill_counts->find(*menu.snapshot.protagonist_id);
                        if (officer != menu.skill_counts->end()) {
                            const auto skill = officer->second.find(offset);
                            if (skill != officer->second.end() && skill->second > 0) {
                                count = skill->second;
                            }
                        }
                    }
                    ImGui::PushID(static_cast<int>(offset));
                    ImGui::TextUnformatted(group.names[i]);
                    ImGui::SameLine();
                    if (offset < 0x15c) {
                        const auto index = static_cast<std::uint8_t>(offset - 0x139);
                        const auto level = menu.snapshot.protagonist_tactic_levels
                            ? (*menu.snapshot.protagonist_tactic_levels)[index] : 0;
                        ImGui::BeginDisabled(!menu.snapshot.protagonist_tactic_levels);
                        const auto level_label = std::to_string(level) + "##level";
                        if (ImGui::Button(level_label.c_str())) {
                            const auto value = static_cast<std::uint8_t>((level + 1) % 4);
                            queue_hero_edits(domain::SetProtagonistTacticLevel{*menu.snapshot.protagonist_id, index, value},
                                [&](std::uint16_t id) -> domain::CommandPayload {
                                    return domain::SetRosterOfficerTraining{id, domain::OfficerTrainingKind::Tactic, index, value};
                                });
                        }
                        ImGui::EndDisabled();
                        ImGui::SameLine();
                    }
                    const auto label = count == 0 ? std::string("-") : std::to_string(count);
                    ImGui::BeginDisabled(!menu.skill_counts);
                    if (ImGui::Button(label.c_str())) {
                        const auto value = (count + 1) % 10;
                        queue_hero_edits(domain::SetSkillCount{*menu.snapshot.protagonist_id, offset, value},
                            [&](std::uint16_t id) -> domain::CommandPayload {
                                return domain::SetSkillCount{id, offset, value};
                            });
                    }
                    ImGui::EndDisabled();
                    ImGui::PopID();
                    if (i + 1 < group.names.size()) {
                        ImGui::SameLine();
                    }
                }
            }
        }
        if (ImGui::CollapsingHeader("특기 레벨")) {
            struct SkillGroup {
                const char* label;
                std::array<const char*, 6> names;
            };
            constexpr std::array groups{
                SkillGroup{"임무", {"경작", "상재", "축성", "경비", "발명", "천성"}},
                SkillGroup{"지모", {"교섭", "허보", "공작", "화술", "열변", "귀모"}},
                SkillGroup{"병과", {"보장", "기장", "궁장", "수군", "조기", "신산"}},
                SkillGroup{"군사", {"원호", "파성", "행군", "여력", "과감", "위풍"}},
            };
            ImGui::BeginDisabled(!menu.snapshot.protagonist_skill_levels);
            for (std::size_t group = 0; group < groups.size(); ++group) {
                ImGui::TextUnformatted(groups[group].label);
                for (std::size_t i = 0; i < groups[group].names.size(); ++i) {
                    const auto index = static_cast<std::uint8_t>(group * 6 + i);
                    const auto level = menu.snapshot.protagonist_skill_levels
                        ? (*menu.snapshot.protagonist_skill_levels)[index] : 0;
                    ImGui::PushID(0x1d0 + index);
                    ImGui::TextUnformatted(groups[group].names[i]);
                    ImGui::SameLine();
                    const auto label = std::to_string(level);
                    if (ImGui::Button(label.c_str())) {
                        const auto value = static_cast<std::uint8_t>((level + 1) % 4);
                        queue_hero_edits(domain::SetProtagonistTraining{*menu.snapshot.protagonist_id, false, index, value},
                            [&](std::uint16_t id) -> domain::CommandPayload {
                                return domain::SetRosterOfficerTraining{id, domain::OfficerTrainingKind::Skill, index, value};
                            });
                    }
                    ImGui::PopID();
                    if (i + 1 < groups[group].names.size()) {
                        ImGui::SameLine();
                    }
                }
            }
            ImGui::EndDisabled();
        }
        if (ImGui::CollapsingHeader("소양(EXP)")) {
            constexpr std::array names{"보병소양", "기병소양", "궁병소양", "함선소양", "군략소양", "보조소양",
                                       "둔갑소양", "임무소양", "지모소양", "병과소양", "군사소양"};
            ImGui::BeginDisabled(!menu.snapshot.protagonist_aptitudes);
            for (std::size_t i = 0; i < names.size(); ++i) {
                auto& draft = menu.protagonist_aptitude_draft[i];
                if (!menu.protagonist_aptitude_editing[i] && menu.snapshot.protagonist_aptitudes) {
                    draft = (*menu.snapshot.protagonist_aptitudes)[i];
                }
                const auto submit_aptitude = [&]() {
                    const auto index = static_cast<std::uint8_t>(i);
                    const auto value = static_cast<std::uint8_t>(draft);
                    queue_hero_edits(domain::SetProtagonistTraining{*menu.snapshot.protagonist_id, true, index, value},
                        [&](std::uint16_t id) -> domain::CommandPayload {
                            return domain::SetRosterOfficerTraining{id, domain::OfficerTrainingKind::Aptitude, index, value};
                        });
                };
                ImGui::PushID(static_cast<int>(i));
                ImGui::TextUnformatted(names[i]);
                ImGui::SameLine();
                bool changed = false;
                if (ImGui::Button("-")) {
                    --draft;
                    changed = true;
                }
                ImGui::SameLine();
                ImGui::SetNextItemWidth(70);
                ImGui::InputInt("##aptitude", &draft, 0, 0, ImGuiInputTextFlags_CharsDecimal);
                menu.protagonist_aptitude_editing[i] = ImGui::IsItemActive();
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    changed = true;
                }
                ImGui::SameLine();
                if (ImGui::Button("+")) {
                    ++draft;
                    changed = true;
                }
                if (changed && !disabled && menu.snapshot.protagonist_aptitudes) {
                    submit_aptitude();
                }
                ImGui::PopID();
            }
            ImGui::EndDisabled();
        }
        struct Ability {
            const char* label;
            std::uintptr_t offset;
        };
        constexpr std::array abilities{
            Ability{"군악대", 0x1000}, Ability{"무쌍 보병", 0x1001},
            Ability{"불꽃 기병", 0x1002}, Ability{"원격 궁병", 0x1003},
            Ability{"등갑군", 0x1004}, Ability{"총사령관", 0x1005},
            Ability{"기습부대", 0x1006}, Ability{"대군사", 0x1007},
            Ability{"만능의 군세! 무신(병종 제약 무시)", 0x1008},
            Ability{"함선 병기화 (지상에서 함선전법 활성화)", 0x1009},
        };
        ImGui::BeginDisabled(!menu.skill_counts);
        for (const auto& ability : abilities) {
            bool enabled = false;
            if (menu.snapshot.protagonist_id && menu.skill_counts) {
                const auto officer = menu.skill_counts->find(*menu.snapshot.protagonist_id);
                if (officer != menu.skill_counts->end()) {
                    const auto skill = officer->second.find(ability.offset);
                    enabled = skill != officer->second.end() && skill->second > 0;
                }
            }
            if (ImGui::Checkbox(ability.label, &enabled)) {
                const int value = enabled ? 1 : 0;
                queue_hero_edits(domain::SetSkillCount{*menu.snapshot.protagonist_id, ability.offset, value},
                    [&](std::uint16_t id) -> domain::CommandPayload {
                        return domain::SetSkillCount{id, ability.offset, value};
                    });
            }
        }
        ImGui::EndDisabled();
        if (disabled) {
            ImGui::EndDisabled();
        }
    }
    end_editor_screen();
}

} // namespace s8::overlay
