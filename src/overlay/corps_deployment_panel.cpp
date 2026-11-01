#include "corps_deployment_panel.h"

#include "../../third_party/imgui_legacy/imgui.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <span>

namespace s8::overlay {
namespace {

const char* status_text(domain::CorpsStatus status) {
    switch (status) {
    case domain::CorpsStatus::Ready: return "준비됨";
    case domain::CorpsStatus::Reading: return "군단·무장·관계 정보를 읽는 중";
    case domain::CorpsStatus::Planning: return "추천 배치를 계산하는 중";
    case domain::CorpsStatus::ApplyingMoves: return "1단계: 일반 무장·군사 이동";
    case domain::CorpsStatus::ApplyingGovernors: return "2단계: 태수 배치";
    case domain::CorpsStatus::Restoring: return "이번 단계의 변경을 복구하는 중";
    case domain::CorpsStatus::Complete: return "완료";
    case domain::CorpsStatus::Cancelled: return "중지됨";
    case domain::CorpsStatus::SessionChanged: return "게임이 바뀌어 군단 정보를 다시 읽습니다.";
    case domain::CorpsStatus::Busy: return "진행 중인 작업이 끝난 뒤 다시 요청하세요.";
    case domain::CorpsStatus::InvalidRequest: return "군단과 무장 선택을 확인하세요.";
    case domain::CorpsStatus::ReadFailed: return "군단 정보를 읽지 못했습니다. 새로고침해 주세요.";
    case domain::CorpsStatus::OwnershipChanged: return "소속·신분·평정 상태가 바뀌어 작업을 중지했습니다.";
    case domain::CorpsStatus::WriteFailed: return "쓰기에 실패하여 이번 단계의 변경을 되돌렸습니다.";
    case domain::CorpsStatus::RollbackFailed: return "복구를 재시도하고 있습니다. 새 작업을 잠시 기다려 주세요.";
    case domain::CorpsStatus::MissingGovernor: return "충성 100인 태수 후보를 배치하지 못한 도시가 있습니다.";
    case domain::CorpsStatus::MissingCycleBuffer: return "태수 순환을 풀 일반 무장 후보가 없어 이번 단계를 되돌렸습니다.";
    case domain::CorpsStatus::RelationshipUnavailable: return "일부 관계 정보를 읽지 못해 확인된 관계만 추천에 반영했습니다.";
    case domain::CorpsStatus::CapacityExceeded: return "현재 데이터의 관계 수 또는 변경 수를 처리하지 못했습니다.";
    }
    return "";
}

const char* rank_name(std::uint8_t status) {
    switch (status) {
    case 0xc8: return "군주";
    case 0xd8: return "도독";
    case 0xe8: return "태수";
    case 0x18: return "군사";
    case 0x38: return "두령";
    case 0x48: return "동지";
    default: return "일반";
    }
}

const char* city_name(std::uint8_t city) {
    return city < domain::CITY_COUNT ? domain::CITY_NAMES[city].data() : "—";
}

void officer_label(std::uint16_t id, const domain::OfficerNames* names, std::span<char> output) {
    if (id == domain::NO_CITY_OFFICER) { std::snprintf(output.data(), output.size(), "없음"); return; }
    if (names) {
        const domain::OfficerNames::const_iterator found = names->find(id);

        if (found != names->end()) { std::snprintf(output.data(), output.size(), "%s (%u)", found->second.c_str(), unsigned(id)); return; }
    }
    std::snprintf(output.data(), output.size(), "무장 %u", unsigned(id));
}

bool submit(CorpsDeploymentPanel& panel, services::CommandMailbox& commands, std::uint64_t& next_request,
    const domain::CorpsRequest& payload) {
    if (panel.pending) { return false; }
    const domain::RequestId id{++next_request};
    const domain::Result<void> accepted = commands.push({id, panel.epoch, payload});

    if (!accepted) { panel.error = "요청을 접수하지 못했습니다. 잠시 후 다시 시도하세요."; return false; }
    panel.pending = id;
    panel.error = nullptr;
    return true;
}

void action(CorpsDeploymentPanel& panel, services::CommandMailbox& commands, std::uint64_t& next_request,
    domain::CorpsAction kind) {
    domain::CorpsRequest request;

    request.action = kind;
    submit(panel, commands, next_request, request);
}

bool city_combo(const char* label, std::uint8_t& city, const domain::CorpsSnapshot& snapshot) {
    bool changed = false;

    if (ImGui::BeginCombo(label, city_name(city))) {
        for (std::size_t index = 0; index < domain::CITY_COUNT; ++index) {
            if (!snapshot.cities[index].player_owned) { continue; }
            if (ImGui::Selectable(domain::CITY_NAMES[index].data(), city == index)) {
                city = static_cast<std::uint8_t>(index);
                changed = true;
            }
        }
        ImGui::EndCombo();
    }
    return changed;
}

void draw_plan(const domain::CorpsSnapshot& snapshot, const domain::OfficerNames* names) {
    if (!ImGui::BeginTable("##corps_cities", 7, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
            ImGuiTableFlags_ScrollY, ImVec2(0, 200))) { return; }
    constexpr std::array<const char*, 7> COLUMNS{"도시", "위치", "현재", "목표", "현재 책임자", "추천 책임자", "상태"};

    for (const char* label : COLUMNS) { ImGui::TableSetupColumn(label); }
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableHeadersRow();
    for (std::size_t city = 0; city < domain::CITY_COUNT; ++city) {
        const domain::CorpsCity& row = snapshot.cities[city];
        std::array<char, 160> label{};

        if (!row.selected) { continue; }
        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::TextUnformatted(domain::CITY_NAMES[city].data());
        ImGui::TableNextColumn(); ImGui::TextUnformatted(row.frontline ? "전방" : "후방");
        ImGui::TableNextColumn(); ImGui::Text("%u", unsigned(row.current_count));
        ImGui::TableNextColumn(); ImGui::Text("%u", unsigned(row.target_count));
        officer_label(row.current_governor, names, label);
        ImGui::TableNextColumn(); ImGui::TextUnformatted(label.data());
        officer_label(row.recommended_governor, names, label);
        ImGui::TableNextColumn(); ImGui::TextUnformatted(label.data());
        ImGui::TableNextColumn(); ImGui::TextUnformatted(row.deferred ? "태수 교체 보류" : "");
    }
    ImGui::EndTable();
}

void draw_roster(CorpsDeploymentPanel& panel, const domain::CorpsSnapshot& snapshot,
    services::CommandMailbox& commands, std::uint64_t& next_request, const domain::OfficerNames* names) {
    if (ImGui::Button("이전 페이지") && snapshot.page > 0) {
        domain::CorpsRequest request;

        request.action = domain::CorpsAction::SetPage;
        request.page = static_cast<std::uint16_t>(snapshot.page - 1);
        submit(panel, commands, next_request, request);
    }
    ImGui::SameLine();
    ImGui::Text("%u / %u · %u명", unsigned(snapshot.page_count == 0 ? 0 : snapshot.page + 1),
        unsigned(snapshot.page_count), unsigned(snapshot.officer_count));
    ImGui::SameLine();
    if (ImGui::Button("다음 페이지") && snapshot.page + 1 < snapshot.page_count) {
        domain::CorpsRequest request;

        request.action = domain::CorpsAction::SetPage;
        request.page = static_cast<std::uint16_t>(snapshot.page + 1);
        submit(panel, commands, next_request, request);
    }
    if (!ImGui::BeginTable("##corps_officers", 8, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
            ImGuiTableFlags_ScrollY, ImVec2(0, 260))) { return; }
    constexpr std::array<const char*, 8> COLUMNS{"무장", "신분", "충성", "통/무/지/정/매", "현재 도시", "추천 도시", "추천 이유", "선택"};

    for (const char* label : COLUMNS) { ImGui::TableSetupColumn(label); }
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableHeadersRow();
    for (std::size_t index = 0; index < snapshot.visible_count; ++index) {
        const domain::CorpsOfficer& row = snapshot.officers[index];
        std::array<char, 160> label{};

        officer_label(row.id, names, label);
        ImGui::PushID(static_cast<int>(row.id));
        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::TextUnformatted(label.data());
        ImGui::TableNextColumn(); ImGui::Text("%s%s", rank_name(row.status), row.recommended_governor ? " → 태수" : "");
        ImGui::TableNextColumn(); ImGui::Text("%u", unsigned(row.loyalty));
        ImGui::TableNextColumn(); ImGui::Text("%u/%u/%u/%u/%u", unsigned(row.attributes[0]), unsigned(row.attributes[1]),
            unsigned(row.attributes[2]), unsigned(row.attributes[3]), unsigned(row.attributes[4]));
        ImGui::TableNextColumn(); ImGui::TextUnformatted(city_name(row.current_city));
        ImGui::TableNextColumn(); ImGui::TextUnformatted(city_name(row.recommended_city));
        ImGui::TableNextColumn(); ImGui::TextUnformatted(domain::CORPS_REASON_NAMES[static_cast<std::size_t>(row.reason)].data());
        ImGui::TableNextColumn();
        if (ImGui::SmallButton("이동/교체")) { panel.officer = row.id; }
        ImGui::SameLine();
        if (ImGui::SmallButton("후보")) { panel.candidate = row.id; }
        ImGui::PopID();
    }
    ImGui::EndTable();
}

void draw_manual(CorpsDeploymentPanel& panel, const domain::CorpsSnapshot& snapshot,
    services::CommandMailbox& commands, std::uint64_t& next_request, const domain::OfficerNames* names) {
    ImGui::SeparatorText("개별 이동 · 태수 · 도독 교체");
    ImGui::SetNextItemWidth(120);
    ImGui::InputInt("무장 ID", &panel.officer);
    panel.officer = std::clamp(panel.officer, 1, static_cast<int>(domain::CORPS_OFFICER_CAPACITY));
    std::array<char, 160> label{};

    officer_label(static_cast<std::uint16_t>(panel.officer), names, label);
    ImGui::SameLine(); ImGui::TextUnformatted(label.data());
    city_combo("이동할 도시", panel.target_city, snapshot);
    if (ImGui::Button("선택 무장 이동")) {
        domain::CorpsRequest request;

        request.action = domain::CorpsAction::MoveOfficer;
        request.officer = static_cast<std::uint16_t>(panel.officer);
        request.target_city = panel.target_city;
        submit(panel, commands, next_request, request);
    }
    ImGui::TextDisabled("도독·태수 이동은 교체 기능을 사용하세요. 군단 간 이동은 일반 무장만 허용합니다.");
    ImGui::SetNextItemWidth(120);
    ImGui::InputInt("교체 후보 ID", &panel.candidate);
    panel.candidate = std::clamp(panel.candidate, 1, static_cast<int>(domain::CORPS_OFFICER_CAPACITY));
    officer_label(static_cast<std::uint16_t>(panel.candidate), names, label);
    ImGui::SameLine(); ImGui::TextUnformatted(label.data());
    if (ImGui::Button("선택 도시 태수 교체")) {
        domain::CorpsRequest request;

        request.action = domain::CorpsAction::SwapGovernor;
        request.city = snapshot.selected_city;
        request.officer = snapshot.selected_city < domain::CITY_COUNT ? snapshot.cities[snapshot.selected_city].current_governor : domain::NO_CITY_OFFICER;
        request.other_officer = static_cast<std::uint16_t>(panel.candidate);
        submit(panel, commands, next_request, request);
    }
    ImGui::SameLine();
    if (ImGui::Button("선택 군단 도독 교체")) {
        domain::CorpsRequest request;

        request.action = domain::CorpsAction::SwapGeneral;
        request.officer = snapshot.selected_city < domain::CITY_COUNT ? snapshot.cities[snapshot.selected_city].general : domain::NO_CITY_OFFICER;
        request.other_officer = static_cast<std::uint16_t>(panel.candidate);
        submit(panel, commands, next_request, request);
    }
    ImGui::TextDisabled("태수 후보: 같은 도시의 일반 무장. 도독 후보: 같은 군단의 태수.");
}

} // namespace

bool on_corps_deployment_completion(CorpsDeploymentPanel& panel, const services::CommandCompletion& completion) {
    if (!panel.pending || *panel.pending != completion.request) { return false; }
    panel.pending.reset();
    if (!completion.result) { panel.error = "요청을 적용하지 못했습니다. 군단 작업 상태를 확인하세요."; }
    return true;
}

void draw_corps_deployment(CorpsDeploymentPanel& panel, services::CorpsBridge& bridge,
    services::CommandMailbox& commands, domain::GameEpoch epoch, std::uint64_t& next_request,
    const domain::OfficerNames* names) {
    const domain::CorpsSnapshot& snapshot = bridge.read();

    if (panel.epoch != epoch) {
        panel.epoch = epoch;
        panel.pending.reset();
        panel.target_city = domain::NO_CITY;
        panel.error = nullptr;
        panel.was_open = false;
    }
    if (!panel.open) { panel.was_open = false; return; }
    if (!panel.was_open && !snapshot.busy && !panel.pending) { action(panel, commands, next_request, domain::CorpsAction::Refresh); }
    panel.was_open = true;
    ImGui::SetNextWindowSize(ImVec2(1100, 860), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("군단 배치 · 도시 무장 이동", &panel.open, ImGuiWindowFlags_NoSavedSettings)) { ImGui::End(); return; }
    ImGui::TextWrapped("%s", status_text(snapshot.status));
    if (snapshot.busy && !snapshot.work_stage.empty()) {
        if (snapshot.total) {
            ImGui::Text("%.*s · %zu / %zu", static_cast<int>(snapshot.work_stage.size()), snapshot.work_stage.data(),
                snapshot.completed, snapshot.total);
            ImGui::ProgressBar(static_cast<float>(snapshot.completed) / static_cast<float>(snapshot.total), ImVec2(-1, 0));
        } else {
            ImGui::Text("%.*s · 처리 위치 %zu", static_cast<int>(snapshot.work_stage.size()), snapshot.work_stage.data(), snapshot.completed);
        }
    }
    ImGui::Text("이동 %zu명 · 태수/도독 교체 %zu회 · 최초 임명 %zu회 · 보류 %zu도시",
        snapshot.moved, snapshot.swapped, snapshot.appointed, snapshot.deferred);
    if (panel.error) { ImGui::TextWrapped("%s", panel.error); }
    ImGui::BeginDisabled(panel.pending.has_value());
    if (ImGui::Button("진행 중인 작업 중지")) { action(panel, commands, next_request, domain::CorpsAction::Cancel); }
    ImGui::EndDisabled();
    if (!snapshot.available || snapshot.epoch != epoch) { ImGui::TextUnformatted("플레이어 세력의 도시 정보를 읽을 수 없습니다."); ImGui::End(); return; }
    ImGui::BeginDisabled(panel.pending.has_value() || snapshot.busy);
    std::uint8_t selected = snapshot.selected_city;

    if (city_combo("군단 기준 도시", selected, snapshot)) {
        domain::CorpsRequest request;

        request.action = domain::CorpsAction::SelectCity;
        request.city = selected;
        submit(panel, commands, next_request, request);
    }
    if (ImGui::Button("새로고침")) { action(panel, commands, next_request, domain::CorpsAction::Refresh); }
    domain::CorpsOptions options = snapshot.options;
    bool changed = ImGui::Checkbox("평정마다 등록 군단 자동 배치", &options.each_council);

    changed = ImGui::Checkbox("의형제 묶기", &options.group_sworn) || changed;
    ImGui::SameLine(); changed = ImGui::Checkbox("배우자 묶기", &options.group_spouses) || changed;
    ImGui::SameLine(); changed = ImGui::Checkbox("상생 묶기", &options.group_synergetic) || changed;
    if (changed) {
        domain::CorpsRequest request;

        request.action = domain::CorpsAction::SetOptions;
        request.options = options;
        submit(panel, commands, next_request, request);
    }
    const bool has_corps = selected < domain::CITY_COUNT &&
        (snapshot.cities[selected].number != 0 || snapshot.cities[selected].general != domain::NO_CITY_OFFICER);
    const bool registered = selected < domain::CITY_COUNT && snapshot.cities[selected].auto_target;

    ImGui::BeginDisabled(!has_corps);
    if (ImGui::Button(registered ? "자동 배치 군단에서 제외" : "이 군단을 자동 배치에 등록")) {
        action(panel, commands, next_request, domain::CorpsAction::ToggleAutoTarget);
    }
    ImGui::SameLine();
    if (ImGui::Button("추천 계산")) { action(panel, commands, next_request, domain::CorpsAction::BuildPlan); }
    ImGui::SameLine();
    if (ImGui::Button("목표 인원 초기화·재계산")) { action(panel, commands, next_request, domain::CorpsAction::ResetTargets); }
    ImGui::BeginDisabled(!snapshot.plan_valid);
    if (ImGui::Button("1단계 이동 적용")) { action(panel, commands, next_request, domain::CorpsAction::ApplyMoves); }
    ImGui::SameLine();
    if (ImGui::Button("2단계 태수 배치")) { action(panel, commands, next_request, domain::CorpsAction::ApplyGovernors); }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("추천·이동·태수 배치 실행")) { action(panel, commands, next_request, domain::CorpsAction::ApplyAll); }
    ImGui::EndDisabled();
    ImGui::TextDisabled("자동 배치 등록은 현재 게임에서만 유지됩니다. 완료된 1단계 이동은 2단계 실패 시에도 유지됩니다.");
    draw_plan(snapshot, names);
    draw_roster(panel, snapshot, commands, next_request, names);
    draw_manual(panel, snapshot, commands, next_request, names);
    ImGui::EndDisabled();
    ImGui::End();
}

} // namespace s8::overlay
