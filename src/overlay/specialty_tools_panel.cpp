#include "specialty_tools_panel.h"

#include "../domain/city_management.h"
#include "../../third_party/imgui_legacy/imgui.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace s8::overlay {
namespace {

using Status = domain::SpecialtyStatus;

const char* status_text(Status status) {
    switch (status) {
    case Status::Ready: return "준비";
    case Status::Reading: return "명품 조회 중";
    case Status::Distributing: return "빈 도시 슬롯에 배분 중";
    case Status::Giving: return "선택한 명품 수여 중";
    case Status::Complete: return "완료";
    case Status::Cancelled: return "취소됨";
    case Status::SessionChanged: return "게임이 바뀌었습니다. 다시 조회하세요.";
    case Status::InvalidTarget: return "수여할 무장을 확인하지 못했습니다.";
    case Status::ReadFailed: return "명품 데이터를 읽지 못했습니다.";
    case Status::Changed: return "대상 또는 소유 상태가 바뀌었습니다.";
    case Status::WriteFailed: return "적용에 실패하여 이전 값을 복구했습니다.";
    case Status::RollbackFailed: return "일부 값의 복구를 확인하지 못했습니다. 작업을 중단했습니다.";
    case Status::Busy: return "다른 명품 작업을 처리 중입니다.";
    }
    return "알 수 없는 상태";
}

bool submit(SpecialtyToolsPanel& panel, services::SpecialtyBridge& bridge,
    const domain::SpecialtySnapshot& snapshot, domain::SpecialtyAction action, bool player = false) {
    domain::SpecialtyRequest request;

    request.epoch = panel.epoch;
    request.generation = snapshot.generation;
    request.action = action;
    request.target = static_cast<std::uint16_t>(std::clamp(panel.target, 1, 5102));
    request.player = player;
    request.selected = panel.selected;
    if (!bridge.push(request)) {
        panel.error = "명품 요청을 전달하지 못했습니다. 잠시 후 다시 시도하세요.";
        return false;
    }
    panel.error = nullptr;
    return true;
}

const char* officer_name(const domain::OfficerNames* names, std::uint16_t id) {
    if (names) {
        const domain::OfficerNames::const_iterator found = names->find(id);

        if (found != names->end()) {
            return found->second.c_str();
        }
    }
    return "이름 미상";
}

const char* specialty_name(const domain::SpecialtyDefinitions* definitions, std::size_t id) {
    return definitions && id < definitions->size() && !(*definitions)[id].name.empty()
        ? (*definitions)[id].name.c_str() : "이름 미상";
}

void draw_attributes(const domain::SpecialtyRow& row) {
    constexpr std::array<const char*, 27> TYPES{
        "없음", "명마", "검", "도", "대도", "창", "극", "도끼", "채찍", "둔기", "암기", "활",
        "병서", "정서", "사서", "경서", "논문", "의서", "기서", "지도", "장식", "보물", "약", "술", "옥새", "기타", "기증품"};
    constexpr std::array<const char*, 6> ABILITIES{"없음", "통솔", "무력", "지력", "정치", "매력"};
    constexpr std::array<const char*, 4> EFFECTS{"없음", "퇴각 확실", "수명 연장", "능력 효과"};

    ImGui::Text("종류: %s / 가치: %u", row.type < TYPES.size() ? TYPES[row.type] : "미상", row.value);
    ImGui::Text("기재 ID: %u / 레벨: %u", row.trait, row.trait_level);
    ImGui::Text("능력: %s +%u", row.ability < ABILITIES.size() ? ABILITIES[row.ability] : "미상", row.ability_value);
    ImGui::Text("효과: %s / 값: %u", row.effect < EFFECTS.size() ? EFFECTS[row.effect] : "미상", row.effect_value);
}

void draw_owner(const domain::SpecialtyRow& row, const domain::OfficerNames* names) {
    if (row.city < domain::CITY_COUNT) {
        ImGui::Text("%s (도시)", domain::CITY_NAMES[row.city].data());
    } else if (row.owner_id != 0) {
        ImGui::Text("%s (ID %u)", officer_name(names, row.owner_id), row.owner_id);
    } else if ((row.owner & 0x0000ffffffffffffULL) > 0x10000) {
        ImGui::TextDisabled("소유자 확인 필요");
    } else {
        ImGui::TextUnformatted("미소유");
    }
}

void draw_item(SpecialtyToolsPanel& panel, std::uint16_t id, const domain::SpecialtySnapshot& snapshot,
    const domain::SpecialtyDefinitions* definitions) {
    if (id == 0 || id >= domain::SPECIALTY_COUNT || !snapshot.items[id].valid) {
        ImGui::TextDisabled("-");
        return;
    }
    const domain::SpecialtyRow& row = snapshot.items[id];
    char label[192]{};

    std::snprintf(label, sizeof(label), "%s (ID %u)", specialty_name(definitions, id), id);
    ImGui::BeginDisabled(snapshot.busy || !snapshot.available);
    if (ImGui::Selectable(label, panel.selected.test(id))) {
        panel.selected.flip(id);
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::BeginTooltip();
        draw_attributes(row);
        if (definitions && !(*definitions)[id].description.empty()) {
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 28);
            ImGui::TextUnformatted((*definitions)[id].description.c_str());
            ImGui::PopTextWrapPos();
        }
        ImGui::Text("주소: 0x%llX", static_cast<unsigned long long>(row.object));
        ImGui::EndTooltip();
    }
    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("명품 주소 복사")) {
            char address[32]{};

            std::snprintf(address, sizeof(address), "0x%llX", static_cast<unsigned long long>(row.object));
            ImGui::SetClipboardText(address);
        }
        ImGui::EndPopup();
    }
}

void draw_target_picker(SpecialtyToolsPanel& panel, const domain::OfficerNames* names) {
    ImGui::SetNextItemWidth(120);
    ImGui::InputInt("수여할 무장 ID", &panel.target);
    panel.target = std::clamp(panel.target, 1, 5102);
    const char* preview = officer_name(names, static_cast<std::uint16_t>(panel.target));

    if (ImGui::BeginCombo("이름으로 선택", preview)) {
        const bool filter_changed = ImGui::InputText("검색", panel.filter.data(), panel.filter.size());

        if (filter_changed || !panel.target_list_open) {
            panel.filtered_count = 0;
            for (std::uint16_t id = 1; id <= 5102; ++id) {
                if (!panel.filter[0] || std::strstr(officer_name(names, id), panel.filter.data())) {
                    panel.filtered_ids[panel.filtered_count++] = id;
                }
            }
        }
        panel.target_list_open = true;
        ImGuiListClipper clipper;

        clipper.Begin(static_cast<int>(panel.filtered_count));
        while (clipper.Step()) {
            for (int index = clipper.DisplayStart; index < clipper.DisplayEnd; ++index) {
                const std::uint16_t id = panel.filtered_ids[static_cast<std::size_t>(index)];
                char label[192]{};

                std::snprintf(label, sizeof(label), "%s (ID %u)", officer_name(names, id), id);
                if (ImGui::Selectable(label, panel.target == id)) {
                    panel.target = id;
                }
            }
        }
        ImGui::EndCombo();
    } else {
        panel.target_list_open = false;
    }
}

} // namespace

void draw_specialty_tools(SpecialtyToolsPanel& panel, services::UiBridge& bridge,
    domain::GameEpoch epoch, const domain::OfficerNames* names) {
    if (!panel.open) {
        panel.was_open = false;
        return;
    }
    const domain::SpecialtySnapshot& snapshot = bridge.specialties.read();
    const own::shared_owner<const domain::SpecialtyDefinitions> definitions = bridge.specialty_definitions_view();

    if (panel.epoch != epoch || panel.generation != snapshot.generation) {
        panel.epoch = epoch;
        panel.generation = snapshot.generation;
        panel.selected.reset();
    }
    if (!panel.was_open) {
        panel.was_open = true;
        if (!snapshot.busy) {
            submit(panel, bridge.specialties, snapshot, domain::SpecialtyAction::Refresh);
        }
    }
    ImGui::SetNextWindowSize(ImVec2(840, 650), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("명품 관리", &panel.open)) {
        ImGui::End();
        return;
    }
    ImGui::BeginDisabled(snapshot.busy);
    if (ImGui::Button("새로 조회")) {
        submit(panel, bridge.specialties, snapshot, domain::SpecialtyAction::Refresh);
    }
    ImGui::SameLine();
    if (ImGui::Button("빈 도시 슬롯에 즉시 자동 배분")) {
        submit(panel, bridge.specialties, snapshot, domain::SpecialtyAction::Distribute);
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!snapshot.busy);
    if (ImGui::Button("작업 취소")) {
        submit(panel, bridge.specialties, snapshot, domain::SpecialtyAction::Cancel);
    }
    ImGui::EndDisabled();
    ImGui::Text("%s (%zu / %zu), 성공 %zu, 실패 %zu", status_text(snapshot.status),
        snapshot.completed, snapshot.total, snapshot.succeeded, snapshot.failed);
    if (snapshot.last_error != Status::Ready) {
        ImGui::TextWrapped("%s", status_text(snapshot.last_error));
    }
    if (panel.error) {
        ImGui::TextWrapped("%s", panel.error);
    }
    ImGui::Separator();
    draw_target_picker(panel, names);
    ImGui::Text("선택한 명품: %zu", panel.selected.count());
    ImGui::SameLine();
    if (ImGui::SmallButton("전체 선택 해제")) {
        panel.selected.reset();
    }
    ImGui::BeginDisabled(snapshot.busy || !snapshot.available || snapshot.epoch != epoch || panel.selected.none());
    if (ImGui::Button("선택 무장에게 수여")) {
        submit(panel, bridge.specialties, snapshot, domain::SpecialtyAction::Give);
    }
    ImGui::SameLine();
    if (ImGui::Button("주인공에게 수여")) {
        submit(panel, bridge.specialties, snapshot, domain::SpecialtyAction::Give, true);
    }
    ImGui::EndDisabled();

    const domain::SpecialtyDefinitions* const definitions_view = definitions ? &*definitions : nullptr;
    constexpr ImGuiTableFlags FLAGS = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable;

    if (ImGui::BeginTabBar("명품 현황")) {
        if (ImGui::BeginTabItem("도시별 진열")) {
            if (ImGui::BeginTable("명품 도시", 4, FLAGS, ImVec2(0, -1))) {
                ImGui::TableSetupColumn("도시");
                ImGui::TableSetupColumn("슬롯 1");
                ImGui::TableSetupColumn("슬롯 2");
                ImGui::TableSetupColumn("슬롯 3");
                ImGui::TableHeadersRow();
                ImGuiListClipper clipper;

                const float row_height = ImGui::GetTextLineHeightWithSpacing() * 2 + ImGui::GetStyle().CellPadding.y * 2;

                clipper.Begin(static_cast<int>(domain::CITY_COUNT), row_height);
                while (clipper.Step()) {
                    for (int city = clipper.DisplayStart; city < clipper.DisplayEnd; ++city) {
                        ImGui::TableNextRow(0, row_height);
                        ImGui::TableSetColumnIndex(0);
                        ImGui::TextUnformatted(domain::CITY_NAMES[static_cast<std::size_t>(city)].data());
                        for (int slot = 0; slot < 3; ++slot) {
                            const std::size_t index = static_cast<std::size_t>(city * 3 + slot);
                            const domain::SpecialtySlot& value = snapshot.slots[index];

                            ImGui::TableSetColumnIndex(slot + 1);
                            ImGui::PushID(static_cast<int>(index));
                            if (value.valid && value.enabled) {
                                draw_item(panel, value.id, snapshot, definitions_view);
                                ImGui::TextDisabled("%s", value.bought ? "구매됨" : "구매 가능");
                            } else {
                                ImGui::TextDisabled("-");
                            }
                            ImGui::PopID();
                        }
                    }
                }
                ImGui::EndTable();
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("전체 명품과 소유자")) {
            if (ImGui::BeginTable("명품 소유", 2, FLAGS, ImVec2(0, -1))) {
                ImGui::TableSetupColumn("명품");
                ImGui::TableSetupColumn("소유자");
                ImGui::TableHeadersRow();
                ImGuiListClipper clipper;

                clipper.Begin(static_cast<int>(domain::SPECIALTY_COUNT - 1));
                while (clipper.Step()) {
                    for (int index = clipper.DisplayStart; index < clipper.DisplayEnd; ++index) {
                        const std::uint16_t id = static_cast<std::uint16_t>(index + 1);

                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::PushID(index);
                        draw_item(panel, id, snapshot, definitions_view);
                        ImGui::PopID();
                        ImGui::TableSetColumnIndex(1);
                        if (snapshot.items[id].valid) {
                            draw_owner(snapshot.items[id], names);
                        }
                    }
                }
                ImGui::EndTable();
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}

} // namespace s8::overlay
