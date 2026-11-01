#include "city_management_panel.h"

#include "../services/ui_bridge.h"

#include "../../third_party/imgui_legacy/imgui.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <span>
#include <utility>

namespace s8::overlay {
namespace {

const char* status_text(domain::CityWorkStatus status) {
    switch (status) {
    case domain::CityWorkStatus::Ready: return "준비됨";
    case domain::CityWorkStatus::Working: return "처리 중";
    case domain::CityWorkStatus::Complete: return "완료";
    case domain::CityWorkStatus::Cancelled: return "중지됨";
    case domain::CityWorkStatus::SessionChanged: return "게임이 바뀌어 도시 정보를 다시 읽습니다.";
    case domain::CityWorkStatus::InvalidRequest: return "입력 범위를 확인하세요.";
    case domain::CityWorkStatus::Busy: return "진행 중인 작업이 끝난 뒤 다시 요청하세요.";
    case domain::CityWorkStatus::ReadFailed: return "도시 정보를 읽지 못했습니다.";
    case domain::CityWorkStatus::WriteFailed: return "쓰기에 실패하여 이번 변경을 되돌렸습니다.";
    case domain::CityWorkStatus::RollbackFailed: return "복구를 재시도하고 있습니다. 새 작업을 잠시 기다려 주세요.";
    case domain::CityWorkStatus::OwnershipChanged: return "도시 소유 세력이나 값이 바뀌어 해당 작업을 건너뛰었습니다.";
    case domain::CityWorkStatus::NoResources: return "이동 가능한 자원이나 병력이 없습니다.";
    }
    return "";
}

bool submit(CityManagementPanel& panel, services::CommandMailbox& commands,
    std::uint64_t& next_request, domain::CommandPayload payload) {
    const domain::RequestId request{++next_request};
    const domain::Result<void> accepted = commands.push({request, panel.epoch, std::move(payload)});

    if (!accepted) {
        panel.local_error = "요청을 접수하지 못했습니다. 잠시 후 다시 시도하세요.";
        return false;
    }
    panel.pending = request;
    panel.local_error = nullptr;
    return true;
}

void request_action(CityManagementPanel& panel, services::CommandMailbox& commands,
    std::uint64_t& next_request, domain::CityAction action, domain::CityField field = domain::CityField::Gold) {
    domain::CityRequest request;

    request.action = action;
    request.field = field;
    submit(panel, commands, next_request, request);
}

bool city_combo(const char* label, std::uint8_t& selected, const domain::CitySnapshot& snapshot,
    std::uint16_t force, bool restrict_force) {
    const char* preview = selected < domain::CITY_COUNT ? domain::CITY_NAMES[selected].data() : "도시 선택";
    bool changed = false;

    if (ImGui::BeginCombo(label, preview)) {
        for (std::size_t city = 0; city < domain::CITY_COUNT; ++city) {
            if (restrict_force && (!snapshot.cities[city].valid || snapshot.cities[city].force_lord != force)) {
                continue;
            }
            if (ImGui::Selectable(domain::CITY_NAMES[city].data(), selected == city)) {
                selected = static_cast<std::uint8_t>(city);
                changed = true;
            }
        }
        ImGui::EndCombo();
    }
    return changed;
}

void force_label(std::uint16_t id, const domain::OfficerNames* names, std::span<char> output) {
    if (names) {
        const domain::OfficerNames::const_iterator found = names->find(id);

        if (found != names->end()) {
            std::snprintf(output.data(), output.size(), "%s 세력", found->second.c_str());
            return;
        }
    }
    if (id == domain::NO_CITY_OFFICER) {
        std::snprintf(output.data(), output.size(), "세력 선택");
    } else {
        std::snprintf(output.data(), output.size(), "무장 ID %u 세력", static_cast<unsigned>(id));
    }
}

void choose_force(CityManagementPanel& panel, const domain::CitySnapshot& snapshot,
    const domain::OfficerNames* names) {
    std::array<char, 192> label{};

    force_label(panel.selected_force, names, label);
    if (ImGui::BeginCombo("세력", label.data())) {
        std::array<std::uint16_t, domain::CITY_COUNT> seen{};
        std::size_t count = 0;

        for (const domain::CityRow& city : snapshot.cities) {
            if (!city.valid || city.force_lord == domain::NO_CITY_OFFICER ||
                std::find(seen.begin(), seen.begin() + count, city.force_lord) != seen.begin() + count) {
                continue;
            }
            seen[count++] = city.force_lord;
            force_label(city.force_lord, names, label);
            if (ImGui::Selectable(label.data(), panel.selected_force == city.force_lord)) {
                panel.selected_force = city.force_lord;
            }
        }
        ImGui::EndCombo();
    }
}

void draw_settings(CityManagementPanel& panel, services::CommandMailbox& commands, std::uint64_t& next_request) {
    ImGui::TextUnformatted("자동 환전 · 평정에 진입할 때 모든 도시의 초과 군량을 금으로 바꿉니다.");
    panel.settings_dirty = ImGui::Checkbox("자동 환전", &panel.settings.auto_exchange) || panel.settings_dirty;
    ImGui::SameLine();
    panel.settings_dirty = ImGui::Checkbox("반란 카운트 항상 0", &panel.settings.revolt_always_zero) || panel.settings_dirty;
    ImGui::SetNextItemWidth(130);
    panel.settings_dirty = ImGui::InputScalar("군량 한도", ImGuiDataType_U32, &panel.settings.grain_limit) || panel.settings_dirty;
    ImGui::SameLine();
    ImGui::SetNextItemWidth(130);
    panel.settings_dirty = ImGui::InputScalar("남길 군량", ImGuiDataType_U32, &panel.settings.retained_grain) || panel.settings_dirty;
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80);
    panel.settings_dirty = ImGui::InputScalar("군량/금 비율", ImGuiDataType_U32, &panel.settings.exchange_rate) || panel.settings_dirty;
    ImGui::BeginDisabled(!panel.settings_dirty || !domain::valid_city_settings(panel.settings));
    if (ImGui::Button("자동화 설정 적용")) {
        if (submit(panel, commands, next_request, domain::SetCitySettings{panel.settings})) {
            panel.settings_dirty = false;
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("모든 도시 반란 0")) {
        request_action(panel, commands, next_request, domain::CityAction::ResetRevolt);
    }
    ImGui::Separator();
    for (std::size_t field = 0; field < domain::CITY_FIELD_COUNT - 1; ++field) {
        if (field != 0 && field != 4) {
            ImGui::SameLine();
        }
        std::array<char, 64> label{};

        std::snprintf(label.data(), label.size(), "전체 %s 최대", domain::CITY_FIELD_NAMES[field].data());
        if (ImGui::Button(label.data())) {
            request_action(panel, commands, next_request, domain::CityAction::Maximize,
                static_cast<domain::CityField>(field));
        }
    }
}

void draw_city_table(CityManagementPanel& panel, const domain::CitySnapshot& snapshot,
    services::CommandMailbox& commands, std::uint64_t& next_request) {
    constexpr ImGuiTableFlags FLAGS = ImGuiTableFlags_BordersInner | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit;

    if (!ImGui::BeginTable("##cities", 10, FLAGS, ImVec2(0, 0))) {
        return;
    }
    ImGui::TableSetupScrollFreeze(1, 1);
    ImGui::TableSetupColumn("도시", ImGuiTableColumnFlags_WidthFixed, 86);
    for (const std::string_view field : domain::CITY_FIELD_NAMES) {
        ImGui::TableSetupColumn(field.data(), ImGuiTableColumnFlags_WidthFixed, 100);
    }
    ImGui::TableHeadersRow();
    ImGuiListClipper clipper;

    clipper.Begin(static_cast<int>(domain::CITY_COUNT));
    while (clipper.Step()) {
        for (int city = clipper.DisplayStart; city < clipper.DisplayEnd; ++city) {
            const std::size_t index = static_cast<std::size_t>(city);

            ImGui::PushID(city);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(domain::CITY_NAMES[index].data());
            for (std::size_t field = 0; field < domain::CITY_FIELD_COUNT; ++field) {
                ImGui::TableSetColumnIndex(static_cast<int>(field + 1));
                if (!snapshot.cities[index].valid) {
                    ImGui::TextDisabled("—");
                    continue;
                }
                if (field == static_cast<std::size_t>(domain::CityField::Revolt)) {
                    ImGui::Text("%u", snapshot.cities[index].values[field]);
                    continue;
                }
                ImGui::PushID(static_cast<int>(field));
                ImGui::SetNextItemWidth(-1);
                ImGui::InputScalar("##value", ImGuiDataType_U32, &panel.drafts[index][field]);
                panel.editing[index][field] = ImGui::IsItemActive();
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    domain::CityRequest request;

                    request.action = domain::CityAction::Write;
                    request.city = static_cast<std::uint8_t>(city);
                    request.field = static_cast<domain::CityField>(field);
                    request.value = panel.drafts[index][field];
                    submit(panel, commands, next_request, request);
                }
                ImGui::PopID();
            }
            ImGui::PopID();
        }
    }
    ImGui::EndTable();
}

bool edit_route(const char* identity, domain::CitySupportRoute& route, const domain::CitySnapshot& snapshot,
    std::uint16_t force, bool restrict_force) {
    bool changed = false;
    bool percent = route.mode == domain::CitySupportMode::Percent;

    ImGui::PushID(identity);
    ImGui::SetNextItemWidth(130);
    changed = city_combo("출발", route.source, snapshot, force, restrict_force) || changed;
    ImGui::SameLine();
    ImGui::SetNextItemWidth(130);
    changed = city_combo("도착", route.target, snapshot, force, restrict_force) || changed;
    ImGui::SameLine();
    if (ImGui::Checkbox("비율(%)", &percent)) {
        route.mode = percent ? domain::CitySupportMode::Percent : domain::CitySupportMode::Fixed;
        changed = true;
    }
    ImGui::SetNextItemWidth(115);
    changed = ImGui::InputScalar("금", ImGuiDataType_U32, &route.gold) || changed;
    ImGui::SameLine();
    ImGui::SetNextItemWidth(115);
    changed = ImGui::InputScalar("군량", ImGuiDataType_U32, &route.grain) || changed;
    ImGui::SameLine();
    ImGui::SetNextItemWidth(115);
    changed = ImGui::InputScalar("병사", ImGuiDataType_U32, &route.troops) || changed;
    if (percent) {
        route.gold = std::min<std::uint32_t>(route.gold, 100);
        route.grain = std::min<std::uint32_t>(route.grain, 100);
        route.troops = std::min<std::uint32_t>(route.troops, 100);
    }
    ImGui::PopID();
    return changed;
}

void draw_transport(CityManagementPanel& panel, const domain::CitySnapshot& snapshot,
    services::UiBridge& bridge, std::uint64_t& next_request, const domain::OfficerNames* names) {
    services::CommandMailbox& commands = bridge.commands;

    choose_force(panel, snapshot, names);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120);
    ImGui::Combo("표시", &panel.frontier_filter, "전체\0전선\0후방\0");
    if (ImGui::BeginTable("##frontiers", 6, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInner |
            ImGuiTableFlags_ScrollY, ImVec2(0, 170))) {
        for (const char* label : {"도시", "구분", "금", "군량", "병사 / 한도", "인접 적대/공백 도시"}) {
            ImGui::TableSetupColumn(label);
        }
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();
        for (std::size_t index = 0; index < domain::CITY_COUNT; ++index) {
            const domain::CityRow& city = snapshot.cities[index];

            if (!city.valid || city.force_lord != panel.selected_force ||
                (panel.frontier_filter == 1 && !city.frontline) || (panel.frontier_filter == 2 && city.frontline)) {
                continue;
            }
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::TextUnformatted(domain::CITY_NAMES[index].data());
            ImGui::TableNextColumn(); ImGui::TextUnformatted(city.frontline ? "전선" : "후방");
            ImGui::TableNextColumn(); ImGui::Text("%u", city.values[0]);
            ImGui::TableNextColumn(); ImGui::Text("%u", city.values[1]);
            ImGui::TableNextColumn(); ImGui::Text("%u / %u", city.values[2], city.values[7]);
            ImGui::TableNextColumn();
            for (const std::uint8_t neighbor : city.neighbors) {
                if (neighbor != domain::NO_CITY && snapshot.cities[neighbor].force_lord != city.force_lord) {
                    ImGui::TextUnformatted(domain::CITY_NAMES[neighbor].data());
                    ImGui::SameLine();
                }
            }
            ImGui::NewLine();
        }
        ImGui::EndTable();
    }
    ImGui::Separator();
    ImGui::TextUnformatted("즉시 지원 · 같은 세력의 도시끼리 자원과 병력을 이동합니다.");
    edit_route("manual", panel.manual_support, snapshot, panel.selected_force, true);
    ImGui::BeginDisabled(!domain::valid_city_route(panel.manual_support));
    if (ImGui::Button("즉시 지원")) {
        domain::CityRequest request;

        request.action = domain::CityAction::Support;
        request.support = panel.manual_support;
        request.force_lord = panel.selected_force;
        submit(panel, commands, next_request, request);
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("이 설정을 자동 노선에 추가") && domain::valid_city_route(panel.manual_support)) {
        panel.routes.routes.push_back(panel.manual_support);
        panel.routes_dirty = true;
    }
    ImGui::Separator();
    if (ImGui::Checkbox("매년 1월 주인공 세력 자동 지원", &panel.routes.yearly_auto)) {
        panel.routes_dirty = true;
    }
    ImGui::TextDisabled("자동 노선은 현재 후방→전선 조건을 만족할 때만 실행합니다.");
    for (std::size_t index = 0; index < panel.routes.routes.size(); ++index) {
        domain::CitySupportRoute& route = panel.routes.routes[index];

        ImGui::PushID(static_cast<int>(index));
        panel.routes_dirty = ImGui::Checkbox("사용", &route.enabled) || panel.routes_dirty;
        ImGui::SameLine();
        bool removed = false;

        if (ImGui::SmallButton("삭제")) {
            panel.routes.routes.erase(panel.routes.routes.begin() + static_cast<std::ptrdiff_t>(index));
            panel.routes_dirty = true;
            removed = true;
        }
        if (!removed) {
            panel.routes_dirty = edit_route("route", route, snapshot, panel.selected_force, false) || panel.routes_dirty;
        }
        ImGui::PopID();
        if (removed) {
            break;
        }
        ImGui::Separator();
    }
    ImGui::BeginDisabled(!panel.routes_dirty || panel.route_save_pending);
    if (ImGui::Button("노선 적용 및 저장")) {
        own::shared_owner<const domain::CitySupportRoutes> routes = own::make_shared<domain::CitySupportRoutes>(panel.routes);

        if (submit(panel, commands, next_request, domain::SetCitySupportRoutes{routes})) {
            panel.routes_dirty = false;
            panel.route_save_pending = bridge.request_city_support_save(std::move(routes));
            panel.save_message = panel.route_save_pending ? "노선 저장 중" : "노선을 적용했으나 저장 요청을 접수하지 못했습니다.";
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("노선 파일 다시 읽기")) {
        bridge.request_city_support_reload();
        panel.routes_dirty = false;
        panel.observed_routes = {};
    }
    ImGui::SameLine();
    if (ImGui::Button("저장된 노선 모두 실행")) {
        domain::CityRequest request;

        request.action = domain::CityAction::RunRoutes;
        request.force_lord = panel.selected_force;
        submit(panel, commands, next_request, request);
    }
}

void draw_technologies(CityManagementPanel& panel, const domain::CitySnapshot& snapshot,
    services::CommandMailbox& commands, std::uint64_t& next_request, const domain::OfficerNames* names) {
    if (!panel.technology_requested && !panel.pending) {
        request_action(panel, commands, next_request, domain::CityAction::RefreshTechnology);
        panel.technology_requested = true;
    }
    if (ImGui::Button("세력 기술 다시 읽기")) {
        request_action(panel, commands, next_request, domain::CityAction::RefreshTechnology);
    }
    ImGui::TextDisabled("보병·기병·궁병·함선 0~3, 병기 0~5. 현재 군주를 다시 확인한 뒤 적용합니다.");
    if (!ImGui::BeginTable("##faction_technologies", 7, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInner |
            ImGuiTableFlags_ScrollY)) {
        return;
    }
    ImGui::TableSetupColumn("세력 (군주)", ImGuiTableColumnFlags_WidthStretch);
    for (const std::string_view technology : domain::CITY_TECHNOLOGY_NAMES) {
        ImGui::TableSetupColumn(technology.data(), ImGuiTableColumnFlags_WidthFixed, 92);
    }
    ImGui::TableSetupColumn("전체", ImGuiTableColumnFlags_WidthFixed, 50);
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableHeadersRow();
    for (std::size_t faction = 0; faction < snapshot.factions.size(); ++faction) {
        const domain::CityFactionTechnology& row = snapshot.factions[faction];
        std::array<char, 192> label{};

        if (!row.valid) {
            continue;
        }
        force_label(row.lord, names, label);
        ImGui::PushID(static_cast<int>(faction));
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(label.data());
        ImGui::BeginDisabled(row.lord == domain::NO_CITY_OFFICER);
        for (std::size_t technology = 0; technology < row.levels.size(); ++technology) {
            ImGui::TableNextColumn();
            ImGui::PushID(static_cast<int>(technology));
            ImGui::SetNextItemWidth(-1);
            ImGui::InputInt("##technology", &panel.technology_drafts[faction][technology], 1, 1);
            panel.technology_drafts[faction][technology] = std::clamp(panel.technology_drafts[faction][technology],
                0, static_cast<int>(domain::CITY_TECHNOLOGY_LIMITS[technology]));
            panel.technology_editing[faction][technology] = ImGui::IsItemActive();
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                domain::CityRequest request;

                request.action = domain::CityAction::WriteTechnology;
                request.city = static_cast<std::uint8_t>(faction);
                request.technology = static_cast<std::uint8_t>(technology);
                request.force_lord = row.lord;
                request.value = static_cast<std::uint32_t>(panel.technology_drafts[faction][technology]);
                submit(panel, commands, next_request, request);
            }
            ImGui::PopID();
        }
        ImGui::TableNextColumn();
        if (ImGui::Button("MAX")) {
            domain::CityRequest request;

            request.action = domain::CityAction::MaximizeTechnology;
            request.city = static_cast<std::uint8_t>(faction);
            request.force_lord = row.lord;
            submit(panel, commands, next_request, request);
        }
        ImGui::EndDisabled();
        ImGui::PopID();
    }
    ImGui::EndTable();
}

} // namespace

bool on_city_management_completion(CityManagementPanel& panel, const services::CommandCompletion& completion) {
    if (!panel.pending || *panel.pending != completion.request) {
        return false;
    }
    panel.pending.reset();
    if (!completion.result) {
        panel.local_error = "도시 작업을 적용하지 못했습니다. 작업 상태를 확인하고 다시 요청하세요.";
    }
    return true;
}

void draw_city_management(CityManagementPanel& panel, services::UiBridge& bridge,
    domain::GameEpoch epoch, std::uint64_t& next_request,
    const domain::OfficerNames* officer_names) {
    const domain::CitySnapshot& snapshot = bridge.city_management.read();
    services::CommandMailbox& commands = bridge.commands;
    const std::optional<services::ConfigSaveStatus> saved = bridge.take_city_support_save_result();

    if (saved) {
        panel.route_save_pending = false;
        switch (*saved) {
        case services::ConfigSaveStatus::Completed: panel.save_message = "노선을 저장했습니다."; break;
        case services::ConfigSaveStatus::Cancelled: panel.save_message = "노선 저장이 취소되었습니다."; break;
        case services::ConfigSaveStatus::Conflict: panel.save_message = "노선 파일이 외부에서 변경되었습니다. 다시 읽고 저장하세요."; break;
        case services::ConfigSaveStatus::InvalidData: panel.save_message = "노선 내용을 확인하세요. 파일을 변경하지 않았습니다."; break;
        case services::ConfigSaveStatus::Failed: panel.save_message = "노선을 파일에 저장하지 못했습니다."; break;
        }
    }

    if (panel.epoch != epoch) {
        panel.epoch = epoch;
        panel.pending.reset();
        panel.editing = {};
        panel.technology_editing = {};
        panel.technology_requested = false;
        panel.revision = 0;
        panel.selected_force = domain::NO_CITY_OFFICER;
        panel.settings_dirty = false;
        panel.routes_dirty = false;
        panel.local_error = nullptr;
    }
    if (snapshot.epoch == epoch && panel.revision != snapshot.revision) {
        for (std::size_t city = 0; city < domain::CITY_COUNT; ++city) {
            for (std::size_t field = 0; field < domain::CITY_FIELD_COUNT; ++field) {
                if (!panel.editing[city][field]) {
                    panel.drafts[city][field] = snapshot.cities[city].values[field];
                }
            }
        }
        for (std::size_t faction = 0; faction < domain::CITY_FACTION_COUNT; ++faction) {
            for (std::size_t technology = 0; technology < domain::CITY_TECHNOLOGY_LIMITS.size(); ++technology) {
                if (!panel.technology_editing[faction][technology]) {
                    panel.technology_drafts[faction][technology] = snapshot.factions[faction].levels[technology];
                }
            }
        }
        if (!panel.settings_dirty) {
            panel.settings = snapshot.settings;
        }
        if (!panel.routes_dirty && own::identity(panel.observed_routes) != own::identity(snapshot.routes)) {
            panel.observed_routes = snapshot.routes;
            panel.routes = snapshot.routes ? *snapshot.routes : domain::CitySupportRoutes{};
        }
        if (panel.selected_force == domain::NO_CITY_OFFICER) {
            panel.selected_force = snapshot.player_force_lord;
        }
        panel.revision = snapshot.revision;
    }
    if (!panel.open) {
        panel.was_open = false;
        panel.technology_requested = false;
        return;
    }
    if (!panel.was_open && !panel.pending) {
        request_action(panel, commands, next_request, domain::CityAction::Refresh);
    }
    panel.was_open = true;
    ImGui::SetNextWindowSize(ImVec2(1000, 760), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("도시 관리", &panel.open, ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::End();
        return;
    }
    ImGui::Text("%s  %zu / %zu · 성공 %zu", status_text(snapshot.status), snapshot.completed, snapshot.total, snapshot.succeeded);
    if (panel.local_error) {
        ImGui::TextWrapped("%s", panel.local_error);
    }
    if (panel.save_message) {
        ImGui::TextWrapped("%s", panel.save_message);
    }
    if (snapshot.troop_limit_reached) {
        ImGui::TextWrapped("목적 도시의 병사 한도에 맞춰 병력 이동량을 줄였습니다.");
    }
    ImGui::BeginDisabled(panel.pending.has_value());
    if (ImGui::Button("새로고침")) {
        request_action(panel, commands, next_request, domain::CityAction::Refresh);
    }
    ImGui::SameLine();
    if (ImGui::Button("진행 중인 작업 중지")) {
        request_action(panel, commands, next_request, domain::CityAction::Cancel);
    }
    ImGui::EndDisabled();
    if (!snapshot.available || snapshot.epoch != epoch) {
        ImGui::TextWrapped("도시 데이터 테이블이 준비되기를 기다립니다.");
        ImGui::End();
        return;
    }
    ImGui::BeginDisabled(panel.pending.has_value() || snapshot.locks_commands);
    if (ImGui::BeginTabBar("##city_tabs")) {
        if (ImGui::BeginTabItem("도시 리스트")) {
            draw_settings(panel, commands, next_request);
            draw_city_table(panel, snapshot, commands, next_request);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("수송")) {
            draw_transport(panel, snapshot, bridge, next_request, officer_names);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("세력 기술")) {
            draw_technologies(panel, snapshot, commands, next_request, officer_names);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::EndDisabled();
    ImGui::End();
}

} // namespace s8::overlay
