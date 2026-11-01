#include "officer_tools_panel.h"

#include "../../third_party/imgui_legacy/imgui.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <string>
#include <utility>

namespace s8::overlay {
namespace {

constexpr std::array<const char*, 35> TACTIC_NAMES{
    "강격", "난격", "교란", "맹돌", "창금", "연격", "돌격", "급습", "기사", "차현",
    "제사", "난사", "화시", "원사", "시람", "화전", "난전", "연환", "돌관", "폭선",
    "열화", "격류", "낙석", "요격", "동토", "분기", "고무", "매성", "치료", "천계",
    "풍변", "천변", "요술", "환술", "낙뢰"};
constexpr std::array<const char*, 24> SKILL_NAMES{
    "경작", "상재", "축성", "경비", "발명", "천성", "교섭", "허보", "공작", "화술", "열변", "귀모",
    "보장", "기장", "궁장", "수군", "조기", "신산", "원호", "파성", "행군", "여력", "과감", "위풍"};

const char* status_label(domain::OfficerToolStatus status) {
    switch (status) {
    case domain::OfficerToolStatus::Idle: return "대기";
    case domain::OfficerToolStatus::Preparing: return "준비 중";
    case domain::OfficerToolStatus::Applying: return "적용 중";
    case domain::OfficerToolStatus::Completed: return "완료";
    case domain::OfficerToolStatus::Cancelled: return "중단됨 · 완료된 변경은 유지";
    case domain::OfficerToolStatus::SessionChanged: return "게임 변경으로 중단됨";
    case domain::OfficerToolStatus::InvalidRequest: return "입력값을 확인하세요";
    case domain::OfficerToolStatus::Unavailable: return "현재 게임 데이터를 확인할 수 없습니다";
    case domain::OfficerToolStatus::ReadFailed: return "데이터 읽기 실패";
    case domain::OfficerToolStatus::WriteFailed: return "일부 변경을 적용하지 못했습니다";
    case domain::OfficerToolStatus::RollbackFailed: return "적용 및 원래 값 복구 실패";
    case domain::OfficerToolStatus::Conflict: return "현재 상태가 바뀌었습니다 · 새로고침 후 확인하세요";
    }
    return "알 수 없는 상태";
}

const char* officer_name(const domain::OfficerNames* names, std::uint16_t id) {
    if (names != nullptr) {
        const domain::OfficerNames::const_iterator found = names->find(id);

        if (found != names->end() && !found->second.empty()) {
            return found->second.c_str();
        }
    }
    return "무장";
}

void issue(std::optional<domain::OfficerToolsRequest>& output, domain::GameEpoch epoch,
    domain::OfficerToolAction action) {
    if (!output) {
        output = domain::OfficerToolsRequest{epoch, std::move(action)};
    }
}

bool active_job(const domain::OfficerToolProgress& progress) {
    return progress.status == domain::OfficerToolStatus::Preparing ||
        progress.status == domain::OfficerToolStatus::Applying;
}

template <std::size_t N>
void draw_training_choices(const char* title, const std::array<const char*, N>& names,
    std::array<domain::TrainingChoice, N>& choices, float scale) {
    if (!ImGui::CollapsingHeader(title)) {
        return;
    }
    ImGui::PushID(title);
    if (ImGui::Button("모두 선택")) {
        for (domain::TrainingChoice& choice : choices) {
            choice.enabled = true;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("선택 해제")) {
        for (domain::TrainingChoice& choice : choices) {
            choice.enabled = false;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("선택 항목 레벨 3")) {
        for (domain::TrainingChoice& choice : choices) {
            if (choice.enabled) {
                choice.level = 3;
            }
        }
    }
    if (ImGui::BeginTable("##training", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH)) {
        ImGui::TableSetupColumn("적용", ImGuiTableColumnFlags_WidthFixed, 40 * scale);
        ImGui::TableSetupColumn("항목");
        ImGui::TableSetupColumn("숙련도", ImGuiTableColumnFlags_WidthFixed, 100 * scale);
        ImGui::TableHeadersRow();
        for (std::size_t i = 0; i < N; ++i) {
            domain::TrainingChoice& choice = choices[i];
            int level = choice.level;

            ImGui::PushID(static_cast<int>(i));
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Checkbox("##enabled", &choice.enabled);
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(names[i]);
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-1);
            if (ImGui::Combo("##level", &level, "0\0I\0II\0III\0")) {
                choice.level = static_cast<std::uint8_t>(level);
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::PopID();
}

void draw_progress(const domain::OfficerToolProgress& progress) {
    if (progress.job == domain::OfficerToolJob::None) {
        return;
    }
    ImGui::Separator();
    ImGui::Text("%s: %s", progress.job == domain::OfficerToolJob::Training ? "일괄 편집" : "랜덤 기재",
        status_label(progress.status));
    ImGui::Text("검색 %u / 대상 %u / 처리 %u / 변경 %u / 실패 %u",
        progress.scanned, progress.eligible, progress.processed, progress.changed, progress.failed);
    if (progress.job == domain::OfficerToolJob::RandomTraits) {
        ImGui::Text("기재 준비 %u / %u · 부여 %u칸 · 이미 세 칸 보유 %u명",
            progress.trait_objects, progress.trait_candidates, progress.filled_slots, progress.already_full);
    }
}

void draw_children(OfficerToolsPanel& panel, const domain::OfficerToolsView& view,
    const domain::OfficerNames* names, float scale, std::optional<domain::OfficerToolsRequest>& output) {
    const domain::FamilyToolsView& family = view.family;

    if (family.child_spouse != 0) {
        ImGui::Text("배우자 %s (%u)의 자녀", officer_name(names, family.child_spouse), family.child_spouse);
        ImGui::SameLine();
        if (ImGui::Button("모든 자녀")) {
            issue(output, view.epoch, domain::SetFamilyPage{0, family.spouse_page, 0});
        }
    }
    if (ImGui::BeginTable("##children", 7, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH)) {
        ImGui::TableSetupColumn("적용", ImGuiTableColumnFlags_WidthFixed, 40 * scale);
        ImGui::TableSetupColumn("자녀");
        ImGui::TableSetupColumn("출생", ImGuiTableColumnFlags_WidthFixed, 50 * scale);
        ImGui::TableSetupColumn("등장", ImGuiTableColumnFlags_WidthFixed, 50 * scale);
        ImGui::TableSetupColumn("사망", ImGuiTableColumnFlags_WidthFixed, 50 * scale);
        ImGui::TableSetupColumn("몇 년 후", ImGuiTableColumnFlags_WidthFixed, 70 * scale);
        ImGui::TableSetupColumn("임관 예정");
        ImGui::TableHeadersRow();
        for (std::size_t i = 0; i < family.child_count; ++i) {
            const domain::ChildToolRow& child = family.children[i];
            const bool commissioned = view.date_ready && view.year >= child.appearance;
            bool selected = child.selected;

            if (panel.child_years[child.id] == 0) {
                panel.child_years[child.id] = child.years_later;
            }
            int years = panel.child_years[child.id];

            ImGui::PushID(static_cast<int>(child.id));
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::BeginDisabled(family.loading || commissioned || !view.date_ready);
            if (ImGui::Checkbox("##selected", &selected)) {
                issue(output, view.epoch, domain::SetChildSchedule{
                    child.id, static_cast<std::uint8_t>(years), selected});
            }
            ImGui::EndDisabled();
            ImGui::TableNextColumn();
            ImGui::Text("%s (%u)", officer_name(names, child.id), child.id);
            ImGui::TableNextColumn();
            ImGui::Text("%u", child.birth);
            ImGui::TableNextColumn();
            ImGui::Text("%u", child.appearance);
            ImGui::TableNextColumn();
            ImGui::Text("%u", child.death);
            ImGui::TableNextColumn();
            ImGui::BeginDisabled(family.loading || commissioned || !view.date_ready);
            ImGui::SetNextItemWidth(-1);
            if (ImGui::InputInt("##years", &years, 0, 0)) {
                years = std::clamp(years, 1, 10);
                panel.child_years[child.id] = static_cast<std::uint8_t>(years);
                issue(output, view.epoch, domain::SetChildSchedule{
                    child.id, static_cast<std::uint8_t>(years), child.selected});
            }
            ImGui::EndDisabled();
            ImGui::TableNextColumn();
            if (commissioned) {
                ImGui::TextUnformatted("임관 완료");
            } else if (child.target_year != 0) {
                ImGui::Text("%u년", child.target_year);
            } else {
                ImGui::TextUnformatted("-");
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    const std::size_t pages = std::max(std::size_t{1},
        (family.child_total + domain::FAMILY_CHILD_PAGE - 1) / domain::FAMILY_CHILD_PAGE);

    ImGui::BeginDisabled(family.loading || family.child_page == 0);
    if (ImGui::Button("이전 자녀")) {
        issue(output, view.epoch, domain::SetFamilyPage{family.child_page - 1, family.spouse_page, family.child_spouse});
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::Text("%zu명 · %zu / %zu", family.child_total, family.child_page + 1, pages);
    ImGui::SameLine();
    ImGui::BeginDisabled(family.loading || family.child_page + 1 >= pages);
    if (ImGui::Button("다음 자녀")) {
        issue(output, view.epoch, domain::SetFamilyPage{family.child_page + 1, family.spouse_page, family.child_spouse});
    }
    ImGui::EndDisabled();
    ImGui::TextWrapped("임관 전 적용을 해제하면 원래 출생·등장 연도로 복구합니다. 예정 연도에 도달한 뒤에는 변경된 연도를 유지합니다.");
}

bool spouse_in_slots(const domain::FamilyToolsView& family, std::uint16_t id) {
    for (const domain::PregnancyToolSlot& slot : family.pregnancy) {
        if (slot.spouse == id) {
            return true;
        }
    }
    return false;
}

void draw_spouses(OfficerToolsPanel& panel, const domain::OfficerToolsView& view,
    const domain::OfficerNames* names, float scale, std::optional<domain::OfficerToolsRequest>& output) {
    const domain::FamilyToolsView& family = view.family;

    ImGui::Text("현재 배우자 %zu명", family.spouse_total);
    ImGui::TextDisabled("배우자 이름을 누르면 무장 상세 편집을 엽니다.");
    if (family.spouse_order_recovery_pending) {
        ImGui::TextWrapped("배우자 순서의 원래 값을 복구하는 중입니다. 복구가 끝날 때까지 순서 교환을 할 수 없습니다.");
    }
    if (ImGui::BeginTable("##spouses", 7, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH)) {
        ImGui::TableSetupColumn("교환", ImGuiTableColumnFlags_WidthFixed, 42 * scale);
        ImGui::TableSetupColumn("순서", ImGuiTableColumnFlags_WidthFixed, 42 * scale);
        ImGui::TableSetupColumn("배우자");
        ImGui::TableSetupColumn("나이", ImGuiTableColumnFlags_WidthFixed, 50 * scale);
        ImGui::TableSetupColumn("자녀", ImGuiTableColumnFlags_WidthFixed, 50 * scale);
        ImGui::TableSetupColumn("자녀 목록");
        ImGui::TableSetupColumn("슬롯 교체 대상");
        ImGui::TableHeadersRow();
        for (std::size_t i = 0; i < family.spouse_count; ++i) {
            const domain::SpouseToolRow& spouse = family.spouses[i];

            ImGui::PushID(static_cast<int>(spouse.id));
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            const auto selected = std::find(panel.spouse_order_selection.begin(),
                panel.spouse_order_selection.end(), spouse.id);
            bool checked = selected != panel.spouse_order_selection.end();
            ImGui::BeginDisabled(family.loading || !family.relationships_ready ||
                family.spouse_order_recovery_pending || !spouse.order_swappable ||
                (!checked && panel.spouse_order_selection[0] && panel.spouse_order_selection[1]));
            if (ImGui::Checkbox("##swap", &checked)) {
                const auto slot = checked ? std::find(panel.spouse_order_selection.begin(),
                    panel.spouse_order_selection.end(), std::uint16_t{0}) : selected;
                if (slot != panel.spouse_order_selection.end()) {
                    const auto index = static_cast<std::size_t>(slot - panel.spouse_order_selection.begin());
                    *slot = checked ? spouse.id : 0;
                    panel.spouse_order_positions[index] = checked ? spouse.order : 0;
                }
            }
            ImGui::EndDisabled();
            ImGui::TableNextColumn();
            ImGui::Text("%u", spouse.order);
            ImGui::TableNextColumn();
            const std::string label = std::string(officer_name(names, spouse.id)) + " (" +
                std::to_string(spouse.id) + ")##details";
            if (ImGui::Selectable(label.c_str())) {
                panel.open_officer = spouse.id;
            }
            ImGui::TableNextColumn();
            if (view.date_ready && spouse.birth != 0 && view.year >= spouse.birth) {
                ImGui::Text("%u", view.year - spouse.birth);
            } else {
                ImGui::TextUnformatted("-");
            }
            ImGui::TableNextColumn();
            ImGui::Text("%u", spouse.child_count);
            ImGui::TableNextColumn();
            if (ImGui::Button("자녀 보기")) {
                issue(output, view.epoch, domain::SetFamilyPage{0, family.spouse_page, spouse.id});
            }
            ImGui::TableNextColumn();
            ImGui::BeginDisabled(family.loading || spouse_in_slots(family, spouse.id));
            if (ImGui::Selectable(panel.pregnancy_spouse == spouse.id ? "선택됨" : "선택",
                panel.pregnancy_spouse == spouse.id)) {
                panel.pregnancy_spouse = spouse.id;
            }
            ImGui::EndDisabled();
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    for (const auto id : panel.spouse_order_selection) {
        if (id) {
            ImGui::Text("교환 선택: %s (%u)", officer_name(names, id), id);
        }
    }
    ImGui::BeginDisabled(family.loading || !family.relationships_ready ||
        family.spouse_order_recovery_pending || !panel.spouse_order_selection[0] ||
        !panel.spouse_order_selection[1]);
    if (ImGui::Button("선택한 배우자 순서 교환")) {
        issue(output, view.epoch, domain::SwapSpouseOrder{panel.spouse_order_selection[0],
            panel.spouse_order_selection[1], panel.spouse_order_positions[0],
            panel.spouse_order_positions[1], panel.spouse_order_generation});
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("교환 선택 해제")) {
        panel.spouse_order_selection.fill(0);
        panel.spouse_order_positions.fill(0);
    }
    ImGui::TextWrapped("두 배우자의 관계 목록 순서를 교환합니다. 임신 슬롯 교체 대상은 '슬롯 교체 대상' 열에서 별도로 지정합니다.");
    const std::size_t pages = std::max(std::size_t{1},
        (family.spouse_total + domain::FAMILY_SPOUSE_PAGE - 1) / domain::FAMILY_SPOUSE_PAGE);

    ImGui::BeginDisabled(family.loading || family.spouse_page == 0);
    if (ImGui::Button("이전 배우자")) {
        issue(output, view.epoch, domain::SetFamilyPage{family.child_page, family.spouse_page - 1, family.child_spouse});
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::Text("%zu / %zu", family.spouse_page + 1, pages);
    ImGui::SameLine();
    ImGui::BeginDisabled(family.loading || family.spouse_page + 1 >= pages);
    if (ImGui::Button("다음 배우자")) {
        issue(output, view.epoch, domain::SetFamilyPage{family.child_page, family.spouse_page + 1, family.child_spouse});
    }
    ImGui::EndDisabled();
}

void draw_pregnancy(OfficerToolsPanel& panel, const domain::OfficerToolsView& view,
    const domain::OfficerNames* names, float scale, std::optional<domain::OfficerToolsRequest>& output) {
    const domain::FamilyToolsView& family = view.family;

    if (!family.pregnancy_ready) {
        ImGui::TextWrapped("임신 슬롯을 확인할 수 없습니다. 배우자 목록을 새로고침하세요.");
        return;
    }
    if (ImGui::BeginTable("##pregnancy", 6, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH)) {
        ImGui::TableSetupColumn("슬롯", ImGuiTableColumnFlags_WidthFixed, 60 * scale);
        ImGui::TableSetupColumn("배우자");
        ImGui::TableSetupColumn("상태");
        ImGui::TableSetupColumn("가능성");
        ImGui::TableSetupColumn("남은 개월");
        ImGui::TableSetupColumn("자녀");
        ImGui::TableHeadersRow();
        for (std::size_t i = 0; i < family.pregnancy.size(); ++i) {
            const domain::PregnancyToolSlot& slot = family.pregnancy[i];
            std::array<char, 24> label{};

            std::snprintf(label.data(), label.size(), "%zu##slot", i + 1);
            ImGui::PushID(static_cast<int>(i));
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::RadioButton(label.data(), &panel.pregnancy_slot, static_cast<int>(i));
            ImGui::TableNextColumn();
            if (slot.spouse != 0) {
                ImGui::Text("%s (%u)", officer_name(names, slot.spouse), slot.spouse);
            } else {
                ImGui::TextUnformatted("없음");
            }
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(slot.pregnant != 0 ? "임신 중" : slot.child != 0 ? "육아 중" : "비임신");
            ImGui::TableNextColumn();
            ImGui::Text("%u%%", 100u - slot.cooldown);
            ImGui::TableNextColumn();
            ImGui::Text("%u", slot.remaining_months);
            ImGui::TableNextColumn();
            if (slot.child != 0) {
                ImGui::Text("%s (%u)", officer_name(names, slot.child), slot.child);
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("자녀 능력 값: %u / %u / %u / %u / %u", slot.child_caps[0],
                        slot.child_caps[1], slot.child_caps[2], slot.child_caps[3], slot.child_caps[4]);
                }
            } else {
                ImGui::TextUnformatted("없음");
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    const domain::PregnancyToolSlot& selected = family.pregnancy[panel.pregnancy_slot];

    if (panel.pregnancy_spouse != 0) {
        ImGui::Text("교체 대상: %s (%u)", officer_name(names, panel.pregnancy_spouse), panel.pregnancy_spouse);
    } else {
        ImGui::TextUnformatted("아래 배우자 목록에서 교체 대상을 선택하세요.");
    }
    ImGui::BeginDisabled(family.loading || !family.relationships_ready || !selected.safe_to_swap ||
        panel.pregnancy_spouse == 0 || spouse_in_slots(family, panel.pregnancy_spouse));
    if (ImGui::Button("선택 슬롯의 배우자 교체")) {
        issue(output, view.epoch, domain::SetPregnancySpouse{static_cast<std::uint8_t>(panel.pregnancy_slot),
            selected.spouse, panel.pregnancy_spouse});
    }
    ImGui::EndDisabled();
    ImGui::TextWrapped("임신 중이 아니고 남은 개월과 자녀가 모두 없는 슬롯만 교체할 수 있습니다. 교체 직후 가능성은 0%로 초기화됩니다.");
}

void draw_scenario(OfficerToolsPanel& panel, const domain::OfficerToolsView& view,
    const domain::OfficerNames* names, float scale, std::optional<domain::OfficerToolsRequest>& output) {
    ImGui::TextUnformatted("시나리오 날짜 · 입력 종료 시 적용");
    ImGui::BeginDisabled(!view.date_ready);
    ImGui::SetNextItemWidth(110 * scale);
    ImGui::InputInt("년", &panel.scenario_year, 0, 0, ImGuiInputTextFlags_CharsDecimal);
    const bool year_changed = ImGui::IsItemDeactivatedAfterEdit();
    const bool year_active = ImGui::IsItemActive();

    ImGui::SameLine();
    ImGui::SetNextItemWidth(70 * scale);
    ImGui::InputInt("월", &panel.scenario_month, 0, 0, ImGuiInputTextFlags_CharsDecimal);
    const bool month_changed = ImGui::IsItemDeactivatedAfterEdit();
    const bool month_active = ImGui::IsItemActive();

    ImGui::EndDisabled();
    if (year_changed || month_changed) {
        panel.date_input_invalid = panel.scenario_year < 0 || panel.scenario_year > 65535 ||
            panel.scenario_month < 1 || panel.scenario_month > 12;
        if (!panel.date_input_invalid) {
            issue(output, view.epoch, domain::SetScenarioDate{static_cast<std::uint16_t>(panel.scenario_year),
                static_cast<std::uint8_t>(panel.scenario_month), year_changed, month_changed});
        }
        panel.last_date_sync = ImGui::GetTime();
    } else if (!year_active && !month_active && view.date_ready &&
        ImGui::GetTime() - panel.last_date_sync >= 0.25) {
        panel.scenario_year = view.year;
        panel.scenario_month = view.month;
        panel.last_date_sync = ImGui::GetTime();
    }
    if (panel.date_input_invalid) {
        ImGui::TextUnformatted("연도는 0~65535, 월은 1~12를 입력하세요.");
    }
    ImGui::Separator();
    if (view.demo_officer != 0) {
        ImGui::Text("복원할 주인공: %s (%u)", officer_name(names, view.demo_officer), view.demo_officer);
    }
    ImGui::BeginDisabled(!view.demo_ready);
    if (ImGui::Button("데모플레이 중지")) {
        issue(output, view.epoch, domain::StopDemoPlay{view.demo_officer});
    }
    ImGui::EndDisabled();
    ImGui::TextWrapped("데모를 중지한 뒤 수동으로 저장하고 다시 불러오세요.");
}

} // namespace

std::optional<domain::OfficerToolsRequest> draw_officer_tools_panel(OfficerToolsPanel& panel,
    const domain::OfficerToolsView& view, const domain::OfficerNames* names, float scale, bool can_submit) {
    std::optional<domain::OfficerToolsRequest> output;
    panel.open_officer.reset();

    if (!panel.open) {
        return output;
    }
    if (panel.epoch != view.epoch) {
        panel.epoch = view.epoch;
        panel.child_years.fill(0);
        panel.pregnancy_spouse = 0;
        panel.pregnancy_slot = 0;
        panel.last_date_sync = 0;
        panel.date_input_invalid = false;
        panel.spouse_order_generation = 0;
        panel.spouse_order_selection.fill(0);
        panel.spouse_order_positions.fill(0);
    }
    if (panel.spouse_order_generation != view.family.relationship_generation) {
        panel.spouse_order_generation = view.family.relationship_generation;
        panel.spouse_order_selection.fill(0);
        panel.spouse_order_positions.fill(0);
    }
    ImGui::SetNextWindowSize(ImVec2(760 * scale, 650 * scale), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("무장·가족·시나리오 도구###OfficerTools", &panel.open)) {
        ImGui::End();
        return output;
    }
    if (view.request_status != domain::OfficerToolStatus::Idle) {
        ImGui::Text("최근 요청: %s", view.request_status == domain::OfficerToolStatus::Preparing
            ? "접수됨" : status_label(view.request_status));
    }
    ImGui::BeginDisabled(!can_submit);
    if (ImGui::BeginTabBar("##officer_tools_tabs")) {
        if (ImGui::BeginTabItem("전법·특기 일괄 편집")) {
            ImGui::TextWrapped("선택한 전법과 특기의 숙련도를 전체 유효 무장에 적용합니다.");
            draw_training_choices("전법 35종", TACTIC_NAMES, panel.training.tactics, scale);
            draw_training_choices("특기 24종", SKILL_NAMES, panel.training.skills, scale);
            ImGui::BeginDisabled(active_job(view.progress));
            if (ImGui::Button("전체 무장에 적용")) {
                issue(output, view.epoch, panel.training);
            }
            ImGui::EndDisabled();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("랜덤 기재")) {
            ImGui::TextWrapped("기존 기재를 유지하고 빈 슬롯만 중복 없이 채웁니다.");
            ImGui::Checkbox("황금 · 1등급", &panel.random.grades[0]);
            ImGui::SameLine();
            ImGui::Checkbox("청록 · 2등급", &panel.random.grades[1]);
            ImGui::SameLine();
            ImGui::Checkbox("적색 · 3등급", &panel.random.grades[2]);
            ImGui::SetNextItemWidth(150 * scale);
            ImGui::InputInt("무장 ID · 0은 전체", &panel.random_officer, 0, 0);
            panel.random_officer = std::clamp(panel.random_officer, 0, static_cast<int>(domain::OFFICER_TOOL_LIMIT));
            ImGui::BeginDisabled(active_job(view.progress) ||
                (!panel.random.grades[0] && !panel.random.grades[1] && !panel.random.grades[2]));
            if (ImGui::Button("빈 기재 슬롯에 랜덤 배정")) {
                panel.random.officer_id = static_cast<std::uint16_t>(panel.random_officer);
                issue(output, view.epoch, panel.random);
            }
            ImGui::EndDisabled();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("자녀·배우자")) {
            if (ImGui::Button("가족 목록 새로고침")) {
                issue(output, view.epoch, domain::RefreshFamilyTools{});
            }
            ImGui::SameLine();
            ImGui::TextUnformatted(view.family.loading ? "가족 정보를 읽는 중" : "가족 정보");
            if (ImGui::CollapsingHeader("자녀 조기 임관", ImGuiTreeNodeFlags_DefaultOpen)) {
                draw_children(panel, view, names, scale, output);
            }
            if (ImGui::CollapsingHeader("임신 슬롯·배우자 교체", ImGuiTreeNodeFlags_DefaultOpen)) {
                draw_pregnancy(panel, view, names, scale, output);
                draw_spouses(panel, view, names, scale, output);
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("시나리오")) {
            draw_scenario(panel, view, names, scale, output);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    draw_progress(view.progress);
    if (active_job(view.progress) && ImGui::Button("남은 일괄 작업 중단")) {
        issue(output, view.epoch, domain::CancelOfficerToolJob{});
    }
    ImGui::EndDisabled();
    ImGui::End();
    return output;
}

} // namespace s8::overlay
