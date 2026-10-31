#include "feature_menu.h"

#include "../../third_party/imgui_legacy/imgui.h"
#include "../domain/secondary_jewel_definitions.h"

#include <array>
#include <algorithm>
#include <string>
#include <utility>
#include <cstdio>

namespace s8::overlay {
namespace {

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
        ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.3f, 1.0f), "적용 실패 · 재시도 대기");
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
    menu.officer_roster = bridge.officer_roster_view();
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
        }
    }
    if (const std::optional<domain::OfficerToolsView> tools = bridge.officer_tools.take()) {
        menu.officer_tools_view = *tools;
    }
    for (std::size_t index = 0; index < 4; ++index) {
        const std::optional<domain::OfficerNoticesSnapshot> notices = bridge.officer_notices.take();

        if (!notices) {
            break;
        }
        update_officer_notice_panel(menu.officer_notices, *notices, menu.notifications,
            menu.officer_names ? &*menu.officer_names : nullptr);
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

void draw_feature_menu(FeatureMenu& menu, services::CommandMailbox& mailbox) {
    draw_special_assignment_results(menu);
    draw_trait_text_editor(menu, mailbox);

    if (!menu.open) {
        return;
    }
    const auto range_button = [&](int range, bool selected) {
        if (selected) {
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1, 1, 0, 1));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 3.0f * menu.dpi_scale);
        }
        const auto texture = menu.range_textures[range - 1];
        ImGui::BeginGroup();
        const bool pressed = texture
            ? ImGui::ImageButton("##range", texture, ImVec2(64 * menu.dpi_scale, 64 * menu.dpi_scale))
            : ImGui::Button(std::to_string(range).c_str(), ImVec2(64 * menu.dpi_scale, 64 * menu.dpi_scale));
        ImGui::Text(" %d", range);
        ImGui::EndGroup();
        if (selected) {
            ImGui::PopStyleVar();
            ImGui::PopStyleColor();
        }
        return pressed;
    };
    if (menu.toggle_collapse) {
        ImGui::SetNextWindowCollapsed(!menu.collapsed, ImGuiCond_Always);
        menu.toggle_collapse = false;
        menu.auto_load_collapse = false;
    } else if (menu.auto_load_collapse) {
        ImGui::SetNextWindowCollapsed(true, ImGuiCond_Appearing);
        menu.auto_load_collapse = false;
    }
    if (!ImGui::Begin("S8RPK", &menu.open)) {
        menu.window_x = ImGui::GetWindowPos().x;
        menu.window_y = ImGui::GetWindowPos().y;
        menu.collapsed = ImGui::IsWindowCollapsed();
        ImGui::End();
        return;
    }
    menu.collapsed = ImGui::IsWindowCollapsed();
    menu.window_x = ImGui::GetWindowPos().x;
    menu.window_y = ImGui::GetWindowPos().y;
    if (menu.error) {
        ImGui::Text("Request failed (%d)", static_cast<int>(menu.error->code));
    }
    if (menu.save_notice && menu.save_notice->status != services::ConfigSaveStatus::Completed) {
        ImGui::Text("설정 저장 실패 (%d, %u)", static_cast<int>(menu.save_notice->status),
                    menu.save_notice->native_code);
        if ((menu.save_notice->status == services::ConfigSaveStatus::Failed ||
             menu.save_notice->status == services::ConfigSaveStatus::Cancelled) && ImGui::Button("저장 다시 시도")) {
            menu.retry_save_requested = true;
        }
    }
    struct FeatureItem {
        const char* label;
        domain::FeatureId id;
    };
    constexpr std::array FEATURES{
        FeatureItem{"행동력 무제한", domain::FeatureId::InfiniteAp},
        FeatureItem{"보주 교체 무제한", domain::FeatureId::FastJewel},
        FeatureItem{"연회 무제한", domain::FeatureId::InfiniteBanquet},
        FeatureItem{"중재 무제한", domain::FeatureId::InfiniteMediation},
        FeatureItem{"대도시", domain::FeatureId::BigCity},
        FeatureItem{"방목 도시", domain::FeatureId::BangmokCity},
        FeatureItem{"농경 도시", domain::FeatureId::NonggyeongCity},
        FeatureItem{"상업 도시", domain::FeatureId::SangeopCity},
        FeatureItem{"견문 시 민심 최대", domain::FeatureId::AttitudeHack},
        FeatureItem{"즉시 경애 맺기", domain::FeatureId::FastRelationship},
        FeatureItem{"AI 친밀도 가속", domain::FeatureId::AutoAffinityGrowth},
        FeatureItem{"결혼 무제한", domain::FeatureId::MarriageApplied},
        FeatureItem{"세력 기술 초기화", domain::FeatureId::TechZero},
        FeatureItem{"시나리오 초기 설정 보정", domain::FeatureId::StartSetting},
        FeatureItem{"게임 배속", domain::FeatureId::SpeedHack},
        FeatureItem{"매 평정 새로운 전기", domain::FeatureId::InfTengi},
        FeatureItem{"AI 전쟁 개선", domain::FeatureId::AiWarImprove},
        FeatureItem{"포로 포획 관리", domain::FeatureId::PrisonerCaptureManagement},
        FeatureItem{"태수 포로 처분", domain::FeatureId::GovernorPrisonerDisposal},
        FeatureItem{"태수 포로 처분 시 특권 소모", domain::FeatureId::GovernorPrisonerConsumePrivilege},
        FeatureItem{"공격측 전법 게이지 최대", domain::FeatureId::MaxAttackStratagemGauge},
        FeatureItem{"수비측 전법 게이지 최대", domain::FeatureId::MaxDefenseStratagemGauge},
        FeatureItem{"단절 영토 무장 이동 제한", domain::FeatureId::IsolatedTerritoryMovement},
        FeatureItem{"수비측 증원 배치 개선", domain::FeatureId::ReinforcementDefenderPlacement},
        FeatureItem{"이동 후 평정 계속", domain::FeatureId::CouncilContinueAfterMove},
        FeatureItem{"CPU 임무에서 주인공 제외", domain::FeatureId::MissionCpuHeroExclusion},
        FeatureItem{"세력 도시 재야 무장 처단", domain::FeatureId::CouncilExecuteFreeOfficers},
        FeatureItem{"상성 수치 표시", domain::FeatureId::AffinityDisplay},
        FeatureItem{"부하의 군주 이동 제안", domain::FeatureId::RulerTransferProposal},
        FeatureItem{"기재 3칸 표시", domain::FeatureId::TraitViewer},
        FeatureItem{"치료", domain::FeatureId::SelfHeal},
        FeatureItem{"동토", domain::FeatureId::Dongto},
        FeatureItem{"천계", domain::FeatureId::Celestial},
        FeatureItem{"투석", domain::FeatureId::Catapult},
        FeatureItem{"격류/낙석", domain::FeatureId::TerrainIgnore},
        FeatureItem{"방어 건물 강화", domain::FeatureId::DefBuilding},
        FeatureItem{"건녕 ↔ 교지 진로 방해 무시", domain::FeatureId::RoadBlock},
        FeatureItem{"교지 ↔ 회계 진로 방해 무시", domain::FeatureId::RoadBlock2},
        FeatureItem{"만병 습득 조건 해제", domain::FeatureId::SkillCondition},
        FeatureItem{"유목기병 습득 조건 해제", domain::FeatureId::YumokCondition},
        FeatureItem{"상병 습득 조건 해제", domain::FeatureId::SangbyeongCondition},
        FeatureItem{"전투 상시 15일", domain::FeatureId::DateAlways15},
        FeatureItem{"전투 일자 유동 조절", domain::FeatureId::DateDynamic},
        FeatureItem{"전투 시작 경과일 스킵", domain::FeatureId::SkipDaysEnabled},
        FeatureItem{"날씨 간단 변경", domain::FeatureId::WeatherSkillSimple},
        FeatureItem{"날씨 대폭 변경", domain::FeatureId::WeatherSkillComplex},
        FeatureItem{"지형 능력 공방만", domain::FeatureId::TerrainAbilityAtkDef},
        FeatureItem{"지형 능력 전체", domain::FeatureId::TerrainAbilityAll},
        FeatureItem{"능력치 한계 돌파", domain::FeatureId::AutoStatUp99},
        FeatureItem{"모든 무장 성향 적극", domain::FeatureId::AllAggressive},
        FeatureItem{"악명 0 유지", domain::FeatureId::ZeroInfamy},
        FeatureItem{"공성전 성 주변 1칸", domain::FeatureId::SiegeWarfare},
        FeatureItem{"공성전 성 주변 2칸", domain::FeatureId::SiegeWarfare2},
        FeatureItem{"주점 청부 무한 유지", domain::FeatureId::InfiniteTavernRequests},
        FeatureItem{"평정 후 명품 자동 배분", domain::FeatureId::AutoFillSpecialties},
        FeatureItem{"평정 전투맵 셔플", domain::FeatureId::BattleMapShuffle},
        FeatureItem{"세력 군주 보너스", domain::FeatureId::FactionLordBonus},
        FeatureItem{"미발견 무장을 재야로", domain::FeatureId::UndiscoveredToRonin},
        FeatureItem{"새로 등장한 재야 무장 알림", domain::FeatureId::MonitorRonin},
        FeatureItem{"무장 영입·사망 알림", domain::FeatureId::OfficerChangeNotify},
        FeatureItem{"무장 충성도 100", domain::FeatureId::Loyalty},
        FeatureItem{"혐오/상극 무시 경애", domain::FeatureId::HateCave},
        FeatureItem{"무조건 공명 발생", domain::FeatureId::Resonance},
        FeatureItem{"내정 배율 적용", domain::FeatureId::Domestics},
        FeatureItem{"시작 시 치트창 표시", domain::FeatureId::AutoLoadMenu},
        FeatureItem{"위젯: 알림확인", domain::FeatureId::ShowWidgetNotif},
        FeatureItem{"위젯: 주인공", domain::FeatureId::ShowWidgetHero},
        FeatureItem{"위젯: 모든 무장", domain::FeatureId::ShowWidgetAllOfficers},
        FeatureItem{"알림 파일 로그", domain::FeatureId::FileLog},
        FeatureItem{"선물 무제한", domain::FeatureId::InfiniteGift},
        FeatureItem{"담화 무제한", domain::FeatureId::InfiniteTalk},
        FeatureItem{"대련 무제한", domain::FeatureId::InfiniteDuel},
        FeatureItem{"설전 무제한", domain::FeatureId::InfiniteDebate},
    };
    const bool disabled = menu.pending.has_value() || !menu.ready;
    if (disabled) {
        ImGui::BeginDisabled();
    }
    const auto submit = [&](domain::CommandPayload payload) {
        const domain::RequestId request{++menu.next_request};
        auto result = mailbox.push({request, menu.epoch, std::move(payload)});
        if (result) {
            menu.pending = request;
            return true;
        } else {
            menu.error = result.error();
            return false;
        }
    };
    for (const auto& item : FEATURES) {
        bool enabled = menu.snapshot.features.get(item.id);

        ImGui::BeginDisabled(item.id == domain::FeatureId::GovernorPrisonerConsumePrivilege &&
            !menu.snapshot.features.get(domain::FeatureId::GovernorPrisonerDisposal));

        if (ImGui::Checkbox(item.label, &enabled)) {
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
                ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.3f, 1.0f),
                    "파일 로그 저장 실패: 옵션을 껐다 켜면 다시 시도합니다.");
            }

            if (menu.file_log_dropped != 0) {
                ImGui::TextDisabled("로그 대기열에서 누락된 알림: %llu",
                    static_cast<unsigned long long>(menu.file_log_dropped));
            }
        }

        if (item.id == domain::FeatureId::TraitViewer) {
            ImGui::SameLine();
            ImGui::TextDisabled("%s", domain::trait_apply_status_text(menu.snapshot.traits.viewer));

            if (menu.snapshot.traits.viewer != domain::TraitApplyStatus::Applied &&
                menu.snapshot.traits.viewer != domain::TraitApplyStatus::Disabled &&
                menu.snapshot.traits.viewer != domain::TraitApplyStatus::Pending &&
                menu.snapshot.traits.viewer != domain::TraitApplyStatus::CallbackBusy) {
                ImGui::SameLine();

                if (ImGui::SmallButton("기재 표시 다시 시도")) {
                    submit(domain::SetFeature{domain::FeatureId::TraitViewer, enabled});
                }
            }
        }

        const std::optional<domain::NativeFeatureGroup> native_group = domain::native_feature_group(item.id);

        if (native_group) {

            const domain::NativeFeatureStatus& status = menu.snapshot.native_features[static_cast<std::size_t>(*native_group)];

            ImGui::SameLine();

            if (status.phase == domain::NativeFeaturePhase::Pending) {
                ImGui::TextDisabled("적용 대기 (현재 그룹 %s)", status.applied ? "활성" : "해제");
            } else if (status.phase == domain::NativeFeaturePhase::Applied) {
                ImGui::TextDisabled("그룹 적용됨");
            } else if (status.phase == domain::NativeFeaturePhase::Disabled) {
                ImGui::TextDisabled("해제됨");
            } else {
                ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.3f, 1.0f), "%s: %s",
                    status.phase == domain::NativeFeaturePhase::Uncertain ? "복구 확인 필요" : "적용 실패",
                    domain::native_feature_error_text(status.error));
                ImGui::SameLine();
                ImGui::PushID(static_cast<int>(item.id));

                if (ImGui::SmallButton("다시 시도")) {
                    submit(domain::SetFeature{item.id, enabled});
                }

                ImGui::PopID();
            }
        }

        const std::optional<domain::LegacyFeatureGroup> legacy_group = domain::legacy_feature_group(item.id);

        if (legacy_group) {
            draw_legacy_status(menu.snapshot, *legacy_group);
        }
    }
    ImGui::Separator();

    if (ImGui::Button("기재 이름·설명 편집")) {
        menu.trait_text_editor.open = true;
    }

    ImGui::SameLine();

    if (ImGui::Button("메모장 / 메모리 도구")) {
        menu.debug_tools.open = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("명품 관리")) {
        menu.specialty_tools.open = true;
    }

    if (ImGui::Button("일괄 전법·특기 / 랜덤 기재 / 가족·시나리오 도구")) {
        menu.officer_tools.open = true;
    }
    if (ImGui::Button("도시 관리 / 후방 지원 / 세력 기술")) {
        menu.city_management.open = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("군단 배치 / 도시 무장 이동")) {
        menu.corps_deployment.open = true;
    }

    ImGui::TextDisabled("확장 기재: %s", domain::trait_apply_status_text(menu.snapshot.traits.catalog));
    ImGui::Separator();

    if (ImGui::CollapsingHeader("주인공 자원")) {
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
                ImGui::SetNextItemWidth(150 * menu.dpi_scale);
                ImGui::InputScalar(domain::PLAYER_RESOURCE_LABELS[index], ImGuiDataType_U32, &menu.resource_draft[index]);
                ImGui::SameLine();
                ImGui::BeginDisabled(menu.pending.has_value() ||
                    menu.resource_draft[index] > domain::player_resource_limit(resource));
                if (ImGui::Button("적용")) {
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

    bool growth_enabled = menu.snapshot.ai_growth_settings.enabled;

    if (ImGui::Checkbox("AI 무장 자동성장", &growth_enabled)) {
        submit(domain::SetAiGrowthEnabled{growth_enabled});
    }

    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("12월에서 1월로 넘어간 뒤 평정에서 누적 EXP로 소양과 전법을 성장시킵니다.\n"
                         "게임의 능력 성장 속도도 함께 적용하며, 원래 양쪽 모두 '없음'이었다면 끌 때 복귀합니다.");
    }

    ImGui::BeginDisabled(!growth_enabled);

    constexpr std::array<const char*, 3> GROWTH_SPEEDS{"느림", "보통", "빠름"};
    int growth_speed = domain::normalize_ai_growth_speed(menu.snapshot.ai_growth_settings.speed) - 1;

    if (ImGui::Combo("AI 성장 속도", &growth_speed, GROWTH_SPEEDS.data(), static_cast<int>(GROWTH_SPEEDS.size()))) {
        submit(domain::SetAiGrowthSpeed{growth_speed + 1});
    }

    bool annual_special = menu.snapshot.ai_growth_settings.annual_special_abilities;

    if (ImGui::Checkbox("성장 후 특수능력 자동 부여", &annual_special)) {
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

    if (ImGui::Button("모든 무장 특수능력 자동 판정")) {

        if (submit(domain::StartSpecialAbilityAssignment{})) {
            menu.special_assignment_results_open = true;
        }
    }

    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(assignment.phase != domain::SpecialAssignmentPhase::Scanning);

    if (ImGui::Button("판정 취소")) {
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

    ImGui::TextDisabled("기존 수동 능력은 유지합니다. 등갑군은 자동 부여에서 제외합니다.");
    std::array<char, 32> slider_epoch_id{};

    std::snprintf(slider_epoch_id.data(), slider_epoch_id.size(), "scalar-%llu",
        static_cast<unsigned long long>(menu.epoch.value));
    // A new session gets new widget IDs, so a drag from the old session cannot
    // submit its discarded draft when the mouse is released afterwards.
    ImGui::PushID(slider_epoch_id.data());
    ImGui::SliderFloat("UI 배율", &menu.ui_scale_draft.value, 0.5f, 3.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp);
    menu.ui_scale_draft.editing = ImGui::IsItemActive();
    menu.ui_scale_draft.commit_requested = ImGui::IsItemDeactivatedAfterEdit() || menu.ui_scale_draft.commit_requested;
    if (menu.ui_scale_draft.commit_requested && !menu.ui_scale_draft.editing && !menu.pending) {
        if (submit(domain::SetUiScale{menu.ui_scale_draft.value})) {
            menu.ui_scale_draft.commit_requested = false;
            menu.ui_scale_draft.request = menu.pending;
            menu.ui_scale_draft.submitted = menu.ui_scale_draft.value;
        } else {
            menu.ui_scale_draft = MenuFloatDraft{menu.snapshot.ui_scale};
        }
    }
    ImGui::SliderFloat("알림 속도", &menu.notification_speed_draft.value, 20.0f, 500.0f, "%.0f px/s");
    menu.notification_speed_draft.editing = ImGui::IsItemActive();
    menu.notification_speed_draft.commit_requested = ImGui::IsItemDeactivatedAfterEdit() || menu.notification_speed_draft.commit_requested;
    if (menu.notification_speed_draft.commit_requested && !menu.notification_speed_draft.editing && !menu.pending) {
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
    int reward_mode = static_cast<int>(menu.snapshot.domestic_reward_mode);
    constexpr const char* reward_modes[]{"OFF", "실적 200% 이상 + 3위 이내", "실적 200% 이상 + 등수 무관"};
    if (ImGui::Combo("내정 포상 조건", &reward_mode, reward_modes, 3)) {
        const auto requested = static_cast<domain::DomesticRewardMode>(reward_mode);
        if (submit(domain::SetDomesticRewardMode{requested})) {
            menu.pending_notice = requested == domain::DomesticRewardMode::Off
                ? "내정 포상 조건 완화 OFF" : "내정 포상 조건 완화 적용 완료";
            menu.pending_failure_notice = "내정 포상 조건 완화 변경 실패";
        }
    }
    if (menu.snapshot.features.get(domain::FeatureId::SpeedHack)) {
        constexpr std::array<const char*, 9> SPEED_LABELS{
            "1.0x", "1.5x", "2.0x", "2.5x", "3.0x", "3.5x", "4.0x", "4.5x", "5.0x"};
        int selected_speed = static_cast<int>((menu.snapshot.speed_multiplier - 1.0f) * 2.0f);

        if (ImGui::Combo("배속 배율", &selected_speed, SPEED_LABELS.data(), static_cast<int>(SPEED_LABELS.size()))) {
            submit(domain::SetSpeedMultiplier{1.0f + static_cast<float>(selected_speed) * 0.5f});
        }
    }
    if (ImGui::CollapsingHeader("전기 발생 · 기간 · 재발생 간격")) {
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
    }
    if (menu.snapshot.features.get(domain::FeatureId::Domestics)) {
        auto multipliers = menu.snapshot.domestics;
        const bool player_changed = ImGui::SliderFloat("플레이어 배율", &multipliers.player, 1.0f, 10.0f, "%.1fx");
        const bool force_changed = ImGui::SliderFloat("세력 배율", &multipliers.force, 1.0f, 10.0f, "%.1fx");
        if (player_changed || force_changed) {
            submit(domain::SetDomesticsMultipliers{multipliers});
        }
    }
    if (menu.snapshot.mission_appointment_limit && !menu.mission_appointment_limit_editing) {
        menu.mission_appointment_limit_draft = *menu.snapshot.mission_appointment_limit;
    }
    if (!menu.snapshot.mission_appointment_limit) {
        ImGui::BeginDisabled();
    }
    ImGui::InputInt("임무 총 임명 한도", &menu.mission_appointment_limit_draft, 0, 0);
    menu.mission_appointment_limit_editing = ImGui::IsItemActive();
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        if (submit(domain::SetMissionAppointmentLimit{menu.mission_appointment_limit_draft})) {
            menu.pending_notice = "임무 총 임명 한도 적용 완료";
        }
    }
    if (!menu.snapshot.mission_appointment_limit) {
        ImGui::EndDisabled();
    }
    ImGui::SameLine();
    if (!menu.snapshot.mission_appointment_limit_managed) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("원본 복구##mission_appointment_limit")) {
        if (submit(domain::RestoreMissionAppointmentLimit{})) {
            menu.pending_notice = "임무 총 임명 한도 원본 복구 완료";
        }
    }
    if (!menu.snapshot.mission_appointment_limit_managed) {
        ImGui::EndDisabled();
    }
    bool all_jewels_open = menu.snapshot.all_jewels_open;
    if (ImGui::Checkbox("보주 전체 개방", &all_jewels_open)) {
        if (submit(domain::SetAllJewelsOpen{all_jewels_open})) {
            menu.pending_notice = all_jewels_open ? "보주 전체 개방 ON" : "보주 전체 개방 OFF";
        }
    }
    if (ImGui::Button("보조 보주 설정")) {
        menu.secondary_jewel_window = true;
    }
    draw_legacy_status(menu.snapshot, domain::LegacyFeatureGroup::SecondaryJewels);
    if (menu.secondary_jewel_window) {
        ImGui::SetNextWindowSize(ImVec2(720, 680), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("보조 보주 강제 사용 설정", &menu.secondary_jewel_window)) {
            auto mask = menu.snapshot.secondary_jewels.value_or(domain::SecondaryJewelMask{});
            std::size_t selected_count = 0;
            for (const auto id : domain::SECONDARY_JEWEL_IDS) {
                selected_count += domain::secondary_jewel_selected(mask, id);
            }
            ImGui::Text("선택: %zu / %zu", selected_count, domain::SECONDARY_JEWEL_IDS.size());
            ImGui::SameLine();
            if (ImGui::Button("전체 선택##secondary")) {
                submit(domain::SetSecondaryJewels{domain::all_secondary_jewels()});
            }
            ImGui::SameLine();
            if (ImGui::Button("전체 해제##secondary")) {
                submit(domain::SetSecondaryJewels{});
            }
            ImGui::InputText("이름 검색", menu.secondary_jewel_filter.data(), menu.secondary_jewel_filter.size());
            ImGui::TextDisabled("선택한 보주는 실제 개방된 뒤부터 적용됩니다.");
            constexpr std::array<const char*, 11> CATEGORIES{"공통", "재야", "두령", "일반", "군사", "태수",
                                                              "도독", "군주", "악한", "무인", "문인"};
            for (std::size_t category = 0; category < CATEGORIES.size(); ++category) {
                const auto kind = static_cast<domain::JewelCategory>(category);
                std::size_t total = 0, selected = 0;
                for (const auto& definition : domain::SECONDARY_JEWEL_DEFINITIONS) {
                    if (definition.category == kind) {
                        ++total;
                        selected += domain::secondary_jewel_selected(mask, definition.id);
                    }
                }
                const auto header = std::string(CATEGORIES[category]) + "  " + std::to_string(selected) + " / " + std::to_string(total);
                ImGui::PushID(static_cast<int>(category));
                if (ImGui::CollapsingHeader(header.c_str(), category == 0 ? ImGuiTreeNodeFlags_DefaultOpen : ImGuiTreeNodeFlags_None)) {
                    if (ImGui::SmallButton("계통 전체 선택")) {
                        for (const auto& definition : domain::SECONDARY_JEWEL_DEFINITIONS) {
                            if (definition.category == kind) {
                                mask.words[definition.id >> 6] |= std::uint64_t{1} << (definition.id & 63);
                            }
                        }
                        submit(domain::SetSecondaryJewels{mask});
                    }
                    ImGui::SameLine();
                    if (ImGui::SmallButton("계통 전체 해제")) {
                        for (const auto& definition : domain::SECONDARY_JEWEL_DEFINITIONS) {
                            if (definition.category == kind) {
                                mask.words[definition.id >> 6] &= ~(std::uint64_t{1} << (definition.id & 63));
                            }
                        }
                        submit(domain::SetSecondaryJewels{mask});
                    }
                    for (const auto& definition : domain::SECONDARY_JEWEL_DEFINITIONS) {
                        if (definition.category != kind ||
                            std::string_view(definition.name).find(menu.secondary_jewel_filter.data()) == std::string_view::npos) {
                            continue;
                        }
                        bool enabled = domain::secondary_jewel_selected(mask, definition.id);
                        ImGui::PushID(static_cast<int>(definition.id));
                        if (menu.snapshot.jewel_bitmap) {
                            const auto byte_index = static_cast<std::size_t>(definition.raw_id >> 3);
                            const bool open = ((*menu.snapshot.jewel_bitmap)[byte_index] & (1u << (definition.raw_id & 7))) != 0;
                            ImGui::TextUnformatted(open ? "개방됨" : "안됨");
                        } else {
                            ImGui::TextDisabled("확인 불가");
                        }
                        ImGui::SameLine();
                        if (ImGui::Checkbox(definition.name, &enabled)) {
                            const auto bit = std::uint64_t{1} << (definition.id & 63);
                            if (enabled) {
                                mask.words[definition.id >> 6] |= bit;
                            } else {
                                mask.words[definition.id >> 6] &= ~bit;
                            }
                            submit(domain::SetSecondaryJewels{mask});
                        }
                        ImGui::PopID();
                    }
                }
                ImGui::PopID();
            }
        }
        ImGui::End();
    }
    if (ImGui::InputInt("시작 경과일", &menu.skip_days_draft, 0, 0)) {
        menu.skip_days_draft = std::max(menu.skip_days_draft, 0);
        submit(domain::SetSkipDays{menu.skip_days_draft});
    }
    if (ImGui::InputInt("수비 부대 회복 비율 (%)", &menu.siege_heal_rate_draft, 1, 5)) {
        menu.siege_heal_rate_draft = std::clamp(menu.siege_heal_rate_draft, 0, 100);
        submit(domain::SetSiegeHealRate{menu.siege_heal_rate_draft});
    }
    if (ImGui::Button("지형 보너스 설정")) {
        menu.terrain_bonus_draft = menu.snapshot.terrain_bonus;
        ImGui::OpenPopup("지형 보너스 설정##editor");
    }
    if (ImGui::BeginPopupModal("지형 보너스 설정##editor", nullptr, ImGuiWindowFlags_NoSavedSettings)) {
        constexpr std::array<const char*, 12> TROOPS{"경보병", "중보병", "정예보병", "만병", "경기병", "중기병",
                                                       "정예기병", "유목기병", "궁병", "노병", "정예궁병", "연노병"};
        constexpr std::array<const char*, 11> TERRAINS{"가도", "평지", "황무지", "초원", "삼림", "습지",
                                                        "산지", "여울", "다리", "도하", "암석"};
        for (std::size_t troop = 0; troop < TROOPS.size(); ++troop) {
            ImGui::PushID(static_cast<int>(troop));
            if (ImGui::TreeNode(TROOPS[troop])) {
                for (std::size_t column = 0; column < TERRAINS.size(); ++column) {
                    auto& entry = menu.terrain_bonus_draft.values[troop + 1][domain::TERRAIN_BONUS_COLUMNS[column]];
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
        if (ImGui::Button("저장 후 닫기")) {
            if (submit(domain::SetTerrainBonus{menu.terrain_bonus_draft})) {
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("취소")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    if (ImGui::Button("치료 설정")) {
        menu.self_heal_draft = menu.snapshot.self_heal_settings;
        ImGui::OpenPopup("치료 설정##editor");
    }
    if (ImGui::BeginPopupModal("치료 설정##editor", nullptr, ImGuiWindowFlags_NoSavedSettings)) {
        auto& draft = menu.self_heal_draft;
        for (int i = 0; i < 3; ++i) {
            ImGui::PushID(i);
            ImGui::Text("Level %d", i + 1);
            ImGui::Checkbox("자기치료 활성화", &draft.self[i]);
            if (ImGui::InputInt("치료량", &draft.amount[i])) {
                draft.amount[i] = std::clamp(draft.amount[i], 0, 32767);
            }
            if (i > 0) {
                ImGui::TextUnformatted("치료 범위 선택:");
                for (int range = 1; range <= 11; ++range) {
                    ImGui::PushID(range);
                    if (range_button(range, draft.range[i - 1] == range)) {
                        draft.range[i - 1] = range;
                    }
                    ImGui::PopID();
                    if (range != 11) {
                        ImGui::SameLine();
                    }
                }
            }
            ImGui::PopID();
        }
        if (ImGui::Button("설정 적용")) {
            if (submit(domain::SetSelfHealSettings{draft})) {
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("취소")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    if (ImGui::Button("동토 설정")) {
        menu.dongto_draft = menu.snapshot.dongto_settings;
        ImGui::OpenPopup("동토 설정##editor");
    }
    if (ImGui::BeginPopupModal("동토 설정##editor", nullptr, ImGuiWindowFlags_NoSavedSettings)) {
        auto& draft = menu.dongto_draft;
        for (int i = 0; i < 3; ++i) {
            ImGui::PushID(i);
            ImGui::Text("Level %d", i + 1);
            ImGui::SliderInt("성공 확률 (%)", &draft.probability[i], 0, 100);
            ImGui::InputInt("직접 입력##확률", &draft.probability[i], 0);
            ImGui::SliderInt("상태이상 확률 (%)", &draft.status_probability[i], 0, 100);
            ImGui::InputInt("직접 입력##상태", &draft.status_probability[i], 0);
            ImGui::PopID();
        }
        if (ImGui::Button("설정 적용")) {
            if (submit(domain::SetDongtoSettings{draft})) {
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("취소")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    if (ImGui::Button("천계 설정")) {
        menu.celestial_draft = menu.snapshot.celestial_settings;
        ImGui::OpenPopup("천계 설정##editor");
    }
    if (ImGui::BeginPopupModal("천계 설정##editor", nullptr, ImGuiWindowFlags_NoSavedSettings)) {
        auto& draft = menu.celestial_draft;
        for (int i = 0; i < 3; ++i) {
            ImGui::PushID(i);
            ImGui::Text("Level %d", i + 1);
            ImGui::SliderInt("성공 확률 (%)", &draft.probability[i], 0, 100);
            ImGui::InputInt("직접 입력##확률", &draft.probability[i], 0);
            if (ImGui::InputInt("회복량", &draft.amount[i], 0)) {
                draft.amount[i] = std::max(draft.amount[i], 0);
            }
            for (int range = 1; range <= 11; ++range) {
                ImGui::PushID(range);
                if (range_button(range, draft.range[i] == range)) {
                    draft.range[i] = range;
                }
                ImGui::PopID();
                if (range != 11) {
                    ImGui::SameLine();
                }
            }
            ImGui::PopID();
        }
        if (ImGui::Button("설정 적용")) {
            if (submit(domain::SetCelestialSettings{draft})) {
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("취소")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    if (ImGui::Button("투석 설정")) {
        menu.catapult_draft = menu.snapshot.catapult_settings;
        ImGui::OpenPopup("투석 설정##editor");
    }
    if (ImGui::BeginPopupModal("투석 설정##editor", nullptr, ImGuiWindowFlags_NoSavedSettings)) {
        auto& draft = menu.catapult_draft;
        ImGui::SliderInt("최소 사거리", &draft.min_range, 1, 20);
        ImGui::InputInt("직접 입력##최소", &draft.min_range, 0);
        ImGui::SliderInt("최대 사거리", &draft.max_range, 1, 20);
        ImGui::InputInt("직접 입력##최대", &draft.max_range, 0);
        if (ImGui::InputInt("부대 위력", &draft.amount, 0)) {
            draft.amount = std::max(draft.amount, 0);
        }
        ImGui::TextUnformatted("범위 선택:");
        for (int range = 1; range <= 11; ++range) {
            ImGui::PushID(range);
            if (range_button(range, draft.range == range)) {
                draft.range = range;
            }
            ImGui::PopID();
            if (range != 11) {
                ImGui::SameLine();
            }
        }
        if (ImGui::Button("설정 적용")) {
            if (submit(domain::SetCatapultSettings{draft})) {
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("취소")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    if (ImGui::Button("격류/낙석 설정")) {
        menu.terrain_tactics_draft = menu.snapshot.terrain_tactics_settings;
        ImGui::OpenPopup("격류/낙석 설정##editor");
    }
    if (ImGui::BeginPopupModal("격류/낙석 설정##editor", nullptr, ImGuiWindowFlags_NoSavedSettings)) {
        auto& draft = menu.terrain_tactics_draft;
        for (int kind = 0; kind < 2; ++kind) {
            ImGui::PushID(kind);
            ImGui::TextUnformatted(kind == 0 ? "격류" : "낙석");
            auto& amounts = kind == 0 ? draft.water_amount : draft.stone_amount;
            auto& ranges = kind == 0 ? draft.water_range : draft.stone_range;
            for (int level = 0; level < 3; ++level) {
                ImGui::PushID(level);
                ImGui::Text("Level %d", level + 1);
                ImGui::InputInt("위력", &amounts[level], 0);
                for (int range = 1; range <= 11; ++range) {
                    ImGui::PushID(range);
                    if (range_button(range, ranges[level] == range)) {
                        ranges[level] = range;
                    }
                    ImGui::PopID();
                    if (range != 11) {
                        ImGui::SameLine();
                    }
                }
                ImGui::PopID();
            }
            ImGui::PopID();
        }
        if (ImGui::Button("설정 적용")) {
            if (submit(domain::SetTerrainTacticsSettings{draft})) {
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("취소")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    if (ImGui::Button("방어 건물 설정")) {
        menu.def_building_draft = menu.snapshot.def_building_settings;
        ImGui::OpenPopup("방어 건물 설정##editor");
    }
    if (ImGui::BeginPopupModal("방어 건물 설정##editor", nullptr, ImGuiWindowFlags_NoSavedSettings)) {
        constexpr std::array<const char*, 5> BUILDINGS{"도시", "관문", "망루", "성벽 투석대", "봉화대"};
        constexpr std::array<const char*, 4> LABELS{"내구도", "사거리", "공격력", "시야"};
        constexpr std::array<const char*, 3> SIGNAL_LABELS{"내구도", "전의", "시야"};
        auto& values = menu.def_building_draft.values;
        for (int building = 0; building < 5; ++building) {
            ImGui::PushID(building);
            ImGui::TextUnformatted(BUILDINGS[building]);
            const int count = building == 4 ? 3 : 4;
            for (int field = 0; field < count; ++field) {
                ImGui::InputInt(building == 4 ? SIGNAL_LABELS[field] : LABELS[field],
                                &values[building * 4 + field], 0);
            }
            ImGui::PopID();
        }
        if (ImGui::Button("설정 적용")) {
            if (submit(domain::SetDefBuildingSettings{menu.def_building_draft})) {
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("취소")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    ImGui::Separator();
    bool troop = menu.snapshot.troop_count_combat_enabled;
    if (ImGui::Checkbox("병력수 공방 반영", &troop)) {
        submit(domain::SetTroopCountCombat{troop});
    }
    draw_legacy_status(menu.snapshot, domain::LegacyFeatureGroup::TroopCountCombat);
    bool reinforcement = menu.snapshot.reinforcement_arrival_enabled;
    if (ImGui::Checkbox("원군 도착 턴 즉시 행동", &reinforcement)) {
        submit(domain::SetReinforcementArrival{reinforcement});
    }
    draw_legacy_status(menu.snapshot, domain::LegacyFeatureGroup::ReinforcementArrival);
    bool city_editor = menu.snapshot.tech_city_visibility_enabled;
    if (ImGui::Checkbox("게임 내 도시 편집기에 기술도시 표시", &city_editor)) {
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
    draw_legacy_status(menu.snapshot, domain::LegacyFeatureGroup::TechCityVisibility);
    bool cooldown = menu.snapshot.short_battle_cooldown_enabled;
    if (ImGui::Checkbox("단기접전 대기일수 변경", &cooldown)) {
        submit(domain::SetShortBattleCooldown{cooldown, menu.cooldown_days_draft});
    }
    draw_legacy_status(menu.snapshot, domain::LegacyFeatureGroup::ShortBattleCooldown);
    if (ImGui::InputInt("대기일수", &menu.cooldown_days_draft, 0, 0,
                        ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_EnterReturnsTrue)) {
        submit(domain::SetShortBattleCooldown{cooldown, menu.cooldown_days_draft});
    }
    bool stratagem = menu.snapshot.stratagem5_enabled;
    if (ImGui::Checkbox("5번 책략 활성화", &stratagem)) {
        submit(domain::SetStratagem5Enabled{stratagem});
    }
    ImGui::SameLine();
    if (ImGui::Button("5번 책략 설정")) {
        menu.stratagem5_draft = menu.snapshot.stratagem5_settings;
        ImGui::OpenPopup("5번 책략 설정##editor");
    }
    if (ImGui::BeginPopupModal("5번 책략 설정##editor", nullptr, ImGuiWindowFlags_NoSavedSettings)) {
        auto& draft = menu.stratagem5_draft;
        ImGui::TextDisabled("책략 ID 5/5/5와 사용횟수 1회는 고정합니다.");
        const char* targets[]{"아군", "적군", "피아불문"};
        int target = draft.target - 1;
        if (ImGui::Combo("대상", &target, targets, IM_ARRAYSIZE(targets))) {
            draft.target = target + 1;
        }
        constexpr int EFFECT_CODES[]{0, 3, 10, 11, 12};
        const char* effects[]{"없음", "상태이상", "사기 증감", "직접 데미지", "화계"};
        const auto draw_effect = [&](const char* title, int& effect, int& power, int& duration) {
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
                const char* statuses[]{"저지", "혼란", "공황"};
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
        };
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
        if (ImGui::Button("적용")) {
            if (submit(domain::SetStratagem5Settings{draft})) {
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("취소")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    if (disabled) {
        ImGui::EndDisabled();
    }
    ImGui::End();
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

void draw_officer_roster(FeatureMenu& menu, services::UiBridge& bridge) {
    auto& mailbox = bridge.commands;
    if (!menu.officer_list_open) {
        return;
    }
    if (menu.ready && !menu.pending && menu.pending_officer_edits.empty() &&
        (!menu.officer_roster || menu.officer_roster->epoch != menu.epoch)) {
        const domain::RequestId request{++menu.next_request};
        auto result = mailbox.push({request, menu.epoch, domain::RefreshOfficerRoster{}});
        if (result) {
            menu.pending = request;
        } else {
            menu.error = result.error();
        }
    }
    ImGui::SetNextWindowSize(ImVec2(1000, 700), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("모든 무장 편집 리스트###OfficerListWin", &menu.officer_list_open)) {
        const auto scan_cursor = bridge.officer_roster_scan_cursor.load();
        if (scan_cursor >= 0) {
            ImGui::Text("목록 로딩 중... (%d/5102)", scan_cursor);
        }
        if (!menu.pending_officer_edits.empty()) {
            ImGui::Text("일괄 적용 중: %zu / %zu", menu.completed_officer_edits,
                        menu.pending_officer_edits.size());
        }
        if (menu.error) {
            ImGui::Text("요청 실패 (%d)", static_cast<int>(menu.error->code));
        }
        const auto select_officer = [&](std::uint16_t id) {
            if (menu.pending || !menu.pending_officer_edits.empty() || !menu.ready) {
                return;
            }
            const domain::RequestId request{++menu.next_request};
            auto result = mailbox.push({request, menu.epoch, domain::SelectRosterOfficer{id}});
            if (result) {
                menu.pending = request;
            } else {
                menu.error = result.error();
            }
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
        if (menu.officer_roster && menu.officer_roster->epoch == menu.epoch) {
            struct Filter { const char* label; int status; };
            constexpr std::array filters{
                Filter{"군사", 0x18}, Filter{"일반", 0x28}, Filter{"두령", 0x38}, Filter{"동지", 0x48},
                Filter{"태수", 0xe8}, Filter{"도독", 0xd8}, Filter{"군주", 0xc8}, Filter{"재야", 0x58},
                Filter{"미발견", 0x68}, Filter{"사망", 0x88}, Filter{"NPC", 0x98}, Filter{"전부", -1},
            };
            ImGui::BeginDisabled(menu.pending.has_value() || !menu.pending_officer_edits.empty());
            for (std::size_t i = 0; i < filters.size(); ++i) {
                if (ImGui::Button(filters[i].label)) {
                    menu.officer_filter = filters[i].status;
                }
                if (i + 1 < filters.size()) {
                    ImGui::SameLine();
                }
            }
            ImGui::EndDisabled();
            const auto rebuild_filter = [&]() {
                menu.officer_filtered_indices.clear();
                for (std::size_t i = 0; i < menu.officer_roster->entries.size(); ++i) {
                    const auto status = menu.officer_roster->entries[i].status;
                    if (menu.officer_filter == -1 || status == menu.officer_filter ||
                        (menu.officer_filter == 0x68 && status == 0x78)) {
                        menu.officer_filtered_indices.push_back(i);
                    }
                }
                menu.officer_cached_filter = menu.officer_filter;
                menu.officer_filtered_source = menu.officer_roster;
            };
            const bool filter_changed = menu.officer_cached_filter != menu.officer_filter ||
                                        own::identity(menu.officer_filtered_source) != own::identity(menu.officer_roster);
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
            const bool run_search = menu.officer_search_pending && !menu.pending &&
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
            const bool selected_visible = menu.snapshot.selected_officer &&
                std::any_of(menu.officer_filtered_indices.begin(), menu.officer_filtered_indices.end(), [&](std::size_t index) {
                    return menu.officer_roster->entries[index].id == menu.snapshot.selected_officer->id;
                });
            if (menu.officer_reselect_pending && !menu.pending && menu.pending_officer_edits.empty() && menu.ready) {
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
            ImGui::Text("무장 %zu명", menu.officer_filtered_indices.size());
            if (selected_visible) {
                const auto& officer = *menu.snapshot.selected_officer;
                bridge.officer_detail_visible.store(true);
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
                constexpr std::array labels{"통솔", "무력", "지력", "정치", "매력", "문명", "무명", "악명",
                                            "공적", "봉록", "충성", "전략포인트", "행동력",
                                            "얼굴 번호", "모델 번호", "모델 색상", "등장년도", "생년", "몰년(수명)"};
                if (menu.selected_basic_id != officer.id) {
                    menu.selected_basic_id = officer.id;
                    menu.selected_basic_editing.fill(false);
                    menu.selected_aptitude_editing.fill(false);
                }
                if (ImGui::CollapsingHeader("선택 무장 능력/상태")) {
                    ImGui::BeginDisabled(menu.pending.has_value() || !menu.pending_officer_edits.empty() || !menu.ready);
                    if (ImGui::BeginChild("##selected_officer_basic", ImVec2(0, 220), true)) {
                        for (std::size_t i = 0; i < labels.size(); ++i) {
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
                if (ImGui::CollapsingHeader("선택 무장 전법/특기/소양")) {
                    ImGui::BeginDisabled(menu.pending.has_value() || !menu.pending_officer_edits.empty() || !menu.ready);
                    if (ImGui::BeginChild("##selected_officer_training", ImVec2(0, 220), true)) {
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
                        if (ImGui::TreeNode("전법")) {
                            for (std::size_t i = 0; i < tactic_names.size(); ++i) {
                                ImGui::PushID(static_cast<int>(i));
                                ImGui::TextUnformatted(tactic_names[i]);
                                ImGui::SameLine();
                                if (i < officer.tactic_levels.size()) {
                                    const auto level = officer.tactic_levels[i];
                                    const auto label = std::to_string(level) + "##level";
                                    if (ImGui::Button(label.c_str())) {
                                        submit_training(domain::OfficerTrainingKind::Tactic, static_cast<std::uint8_t>(i),
                                                        static_cast<std::uint8_t>((level + 1) % 4));
                                    }
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
                                const auto label = (count ? std::to_string(count) : std::string("-")) + "##count";
                                if (ImGui::Button(label.c_str())) {
                                    submit_count(0x139 + i, (count + 1) % 10);
                                }
                                ImGui::EndDisabled();
                                ImGui::PopID();
                            }
                            ImGui::TreePop();
                        }
                        constexpr std::array skill_names{
                            "경작", "상재", "축성", "경비", "발명", "천성", "교섭", "허보", "공작", "화술", "열변", "귀모",
                            "보장", "기장", "궁장", "수군", "조기", "신산", "원호", "파성", "행군", "여력", "과감", "위풍"};
                        if (ImGui::TreeNode("특기")) {
                            for (std::size_t i = 0; i < skill_names.size(); ++i) {
                                ImGui::PushID(static_cast<int>(i));
                                ImGui::TextUnformatted(skill_names[i]);
                                ImGui::SameLine();
                                const auto level = officer.skill_levels[i];
                                const auto label = std::to_string(level);
                                if (ImGui::Button(label.c_str())) {
                                    submit_training(domain::OfficerTrainingKind::Skill, static_cast<std::uint8_t>(i),
                                                    static_cast<std::uint8_t>((level + 1) % 4));
                                }
                                ImGui::PopID();
                            }
                            ImGui::TreePop();
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
            if (ImGui::BeginChild("##officer_roster", ImVec2(0, 0), true)) {
                const auto row_height = ImGui::GetFrameHeightWithSpacing();
                if (run_search && menu.officer_search_result >= 0) {
                    ImGui::SetScrollY(menu.officer_search_result * row_height);
                }
                ImGuiListClipper clipper;
                clipper.Begin(static_cast<int>(menu.officer_filtered_indices.size()), row_height);
                while (clipper.Step()) {
                    for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                        const auto& officer = menu.officer_roster->entries[menu.officer_filtered_indices[i]];
                        ImGui::PushID(static_cast<int>(officer.id));
                        ImGui::Checkbox("##multi", &menu.selected_officer_ids[officer.id]);
                        ImGui::SameLine();
                        const auto name = menu.officer_names ? menu.officer_names->find(officer.id)
                                                             : domain::OfficerNames::const_iterator{};
                        const char* display = menu.officer_names && name != menu.officer_names->end()
                            ? name->second.c_str() : "";
                        const bool protagonist = menu.snapshot.protagonist_id == officer.id;
                        const auto label = (protagonist ? std::string("★ ") : std::string{}) +
                            "#" + std::to_string(officer.id) + "  " + display +
                            (protagonist ? " (주인공)" : "") + "  상태 " + std::to_string(officer.status);
                        const bool selected = menu.snapshot.selected_officer &&
                            menu.snapshot.selected_officer->id == officer.id;
                        ImGui::BeginDisabled(!menu.pending_officer_edits.empty() || !menu.ready);
                        if (menu.officer_name_edit_id == officer.id && menu.officer_name_edit_list) {
                            draw_officer_name_input(menu, bridge, officer.id);
                        } else {
                            const bool clicked = ImGui::Selectable(label.c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick);
                            if (clicked && ImGui::IsMouseDoubleClicked(0) && !menu.officer_name_save_pending) {
                                menu.officer_name_edit_id = officer.id;
                                menu.officer_name_edit_list = true;
                                menu.officer_name_focus = true;
                                std::snprintf(menu.officer_name_draft.data(), menu.officer_name_draft.size(), "%s", display);
                            } else if (clicked || (ImGui::IsItemFocused() && !selected)) {
                                select_officer(officer.id);
                            }
                        }
                        ImGui::EndDisabled();
                        ImGui::PopID();
                    }
                }
            }
            ImGui::EndChild();
        } else {
            ImGui::TextUnformatted("목록 갱신을 눌러 무장을 읽어오세요.");
        }
    }
    ImGui::End();
}

void draw_protagonist_abilities(FeatureMenu& menu, services::UiBridge& bridge) {
    auto& mailbox = bridge.commands;
    if (!menu.protagonist_detail) {
        return;
    }
    ImGui::SetNextWindowSize(ImVec2(800, 700), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("주인공 무장 상세 편집###OffDetailWin", &menu.protagonist_detail)) {
        if (menu.snapshot.protagonist_id) {
            draw_officer_name(menu, bridge, *menu.snapshot.protagonist_id, true);
            if (menu.snapshot.protagonist_traits) {
                draw_officer_traits(menu, mailbox, *menu.snapshot.protagonist_id, *menu.snapshot.protagonist_traits, true);
            }
        }
        if (!menu.pending_officer_edits.empty()) {
            ImGui::Text("일괄 적용 중: %zu / %zu", menu.completed_officer_edits, menu.pending_officer_edits.size());
        }
        if (menu.error) {
            ImGui::Text("요청 실패 (%d)", static_cast<int>(menu.error->code));
        }
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
                if (changed) {
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
                if (changed) {
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
    ImGui::End();
}

void draw_officer_tools(FeatureMenu& menu, services::UiBridge& bridge) {
    if (!menu.officer_tools.open) {
        return;
    }
    if (!menu.officer_tools_view || menu.officer_tools_view->epoch != menu.epoch) {
        if (ImGui::Begin("무장·가족·시나리오 도구###OfficerTools", &menu.officer_tools.open)) {
            ImGui::TextUnformatted("현재 게임의 무장 정보를 기다리는 중입니다.");
        }
        ImGui::End();
        return;
    }
    const std::optional<domain::OfficerToolsRequest> command = draw_officer_tools_panel(menu.officer_tools,
        *menu.officer_tools_view, menu.officer_names ? &*menu.officer_names : nullptr, menu.dpi_scale,
        !menu.pending.has_value() && menu.ready);

    if (!command) {
        return;
    }
    const domain::RequestId request{++menu.next_request};
    const domain::Result<void> accepted = bridge.commands.push({request, menu.epoch, *command});

    if (accepted) {
        menu.pending = request;
        menu.error.reset();
    } else {
        menu.error = accepted.error();
    }
}

} // namespace s8::overlay
