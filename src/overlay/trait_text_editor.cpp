#include "trait_text_editor.h"

#include "feature_menu.h"
#include "../services/ui_bridge.h"
#include "../../third_party/imgui_legacy/imgui.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <string_view>
#include <utility>

namespace s8::overlay {
namespace {

template<std::size_t N>
void copy_text(std::array<char, N>& target, const std::string& source) {

    const std::size_t count = std::min(source.size(), N - 1);

    std::memcpy(target.data(), source.data(), count);
    target[count] = '\0';
}

bool validate_rows(TraitTextEditor& editor, std::size_t tab) {

    for (std::size_t index = 0; index < editor.rows[tab].size(); ++index) {

        const domain::TraitTextValidation validation = domain::validate_trait_text_edit(editor.rows[tab][index]);

        if (validation != domain::TraitTextValidation::Valid) {
            editor.validation = validation;
            editor.invalid_row = index;
            return false;
        }
    }

    editor.validation = domain::TraitTextValidation::Valid;
    return true;
}

void draw_rows(FeatureMenu& menu, services::CommandMailbox& mailbox, std::size_t tab) {

    TraitTextEditor& editor = menu.trait_text_editor;
    domain::TraitTextEdits& rows = editor.rows[tab];
    const bool external = tab != 0 && menu.snapshot.traits.catalog == domain::TraitApplyStatus::ExternalRuntime;
    const bool locked = menu.pending.has_value() || editor.save_pending || editor.save_rows || !menu.ready;

    if (rows.empty()) {
        ImGui::TextUnformatted(tab == 0 ? "기본 기재 텍스트를 읽는 중입니다." :
            "로드된 확장 기재 텍스트가 없습니다. S8RPK_traits_default.json을 확인해 주세요.");
        return;
    }

    ImGui::BeginDisabled(locked);
    ImGui::InputTextWithHint("##trait_text_filter", "이름 검색", editor.filter.data(), editor.filter.size());

    if (ImGui::BeginTable("trait_text_columns", 2, ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn("기재", ImGuiTableColumnFlags_WidthFixed, 210.0f * menu.dpi_scale);
        ImGui::TableSetupColumn("텍스트", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextColumn();
        ImGui::BeginChild("trait_text_names", ImVec2(0, 330.0f * menu.dpi_scale), true);

        const std::string_view filter(editor.filter.data());

        for (std::size_t index = 0; index < rows.size(); ++index) {

            const domain::TraitTextEdit& row = rows[index];
            const std::string& name = row.name.empty() ? row.original_name : row.name;

            if (!filter.empty() && name.find(filter) == std::string::npos &&
                row.original_name.find(filter) == std::string::npos) {
                continue;
            }

            ImGui::PushID(static_cast<int>(index));

            if (ImGui::Selectable(name.c_str(), editor.selected[tab] == static_cast<int>(index))) {
                editor.selected[tab] = static_cast<int>(index);
            }

            ImGui::SameLine();
            ImGui::TextDisabled("%u", tab == 0 ? static_cast<unsigned>(index + 1) : static_cast<unsigned>(row.trait_id));
            ImGui::PopID();
        }

        ImGui::EndChild();
        ImGui::TableNextColumn();

        const int selected = editor.selected[tab];

        if (selected >= 0 && static_cast<std::size_t>(selected) < rows.size()) {

            domain::TraitTextEdit& row = rows[static_cast<std::size_t>(selected)];

            if (editor.loaded_tab != static_cast<int>(tab) || editor.loaded_row != selected) {
                copy_text(editor.name, row.name);
                copy_text(editor.description, row.description);
                editor.loaded_tab = static_cast<int>(tab);
                editor.loaded_row = selected;
            }

            ImGui::Text("원문 이름: %s", row.original_name.c_str());

            if (ImGui::InputTextWithHint("이름", "빈 값은 원문 사용", editor.name.data(), editor.name.size())) {
                row.name.assign(editor.name.data());
                editor.dirty[tab] = true;
                editor.validation = domain::validate_trait_text_edit(row);
                editor.invalid_row = static_cast<std::size_t>(selected);
            }

            ImGui::TextUnformatted("설명 (빈 값은 원문 사용)");

            if (ImGui::InputTextMultiline("##trait_text_description", editor.description.data(),
                    editor.description.size(), ImVec2(-1, 100.0f * menu.dpi_scale))) {
                row.description.assign(editor.description.data());
                editor.dirty[tab] = true;
                editor.validation = domain::validate_trait_text_edit(row);
                editor.invalid_row = static_cast<std::size_t>(selected);
            }

            if (row.effects && ImGui::BeginTable("trait_effect_rows", 3, ImGuiTableFlags_SizingStretchProp)) {
                ImGui::TableSetupColumn("효과 종류", ImGuiTableColumnFlags_WidthStretch, 3.0f);
                ImGui::TableSetupColumn("값", ImGuiTableColumnFlags_WidthFixed, 100.0f * menu.dpi_scale);
                ImGui::TableSetupColumn("P", ImGuiTableColumnFlags_WidthFixed, 100.0f * menu.dpi_scale);
                ImGui::TableHeadersRow();

                for (std::size_t index = 0; index < row.effects->size(); ++index) {
                    domain::TraitEffect& effect = (*row.effects)[index];
                    std::string label = effect.effect_id == 0 ? "(없음)" : std::to_string(effect.effect_id);

                    if (menu.trait_effects) {
                        const auto definition = menu.trait_effects->find(effect.effect_id);

                        if (definition != menu.trait_effects->end()) {
                            label += " " + definition->second.text;
                        }
                    }
                    ImGui::PushID(static_cast<int>(index));
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth(-1);

                    if (ImGui::BeginCombo("##effect_type", label.c_str())) {
                        if (ImGui::Selectable("(없음)", effect.effect_id == 0)) {
                            effect = {};
                            editor.dirty[tab] = true;
                        }
                        if (menu.trait_effects) {
                            for (const auto& [id, definition] : *menu.trait_effects) {
                                const std::string choice = std::to_string(id) + " " + definition.text;

                                if (ImGui::Selectable(choice.c_str(), effect.effect_id == id)) {
                                    effect.effect_id = id;
                                    editor.dirty[tab] = true;
                                }
                            }
                        }
                        ImGui::EndCombo();
                    }
                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth(-1);
                    editor.dirty[tab] |= ImGui::InputScalar("##effect_value", ImGuiDataType_U16, &effect.value1);
                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth(-1);
                    editor.dirty[tab] |= ImGui::InputScalar("##effect_parameter", ImGuiDataType_U16, &effect.value2);
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }

            if (ImGui::CollapsingHeader("원문 설명")) {
                ImGui::TextWrapped("%s", row.original_description.c_str());
            }

            if (ImGui::Button("선택 항목의 편집값 비우기")) {
                row.name.clear();
                row.description.clear();
                editor.name[0] = '\0';
                editor.description[0] = '\0';
                editor.dirty[tab] = true;
                editor.validation = domain::TraitTextValidation::Valid;
            }
        } else {
            ImGui::TextUnformatted("왼쪽에서 편집할 기재를 선택하세요.");
        }

        ImGui::EndTable();
    }

    if (editor.validation != domain::TraitTextValidation::Valid) {
        ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.3f, 1.0f), "%u번: %s",
            static_cast<unsigned>(editor.invalid_row + 1), domain::trait_text_validation_text(editor.validation));
    }

    if (ImGui::Button("게임에 적용") && validate_rows(editor, tab)) {

        own::shared_owner<const domain::TraitTextEdits> request_rows = own::make_shared<domain::TraitTextEdits>(rows);

        if (external) {
            editor.save_rows = std::move(request_rows);
            editor.save_embedded = true;
            editor.apply_external = true;
            editor.save_status.reset();
        } else {
            const domain::RequestId request{++menu.next_request};
            const domain::Result<void> accepted = mailbox.push({request, menu.epoch,
                domain::ApplyTraitTextEdits{tab != 0, std::move(request_rows)}});

            if (accepted) {
                menu.pending = request;
                menu.pending_notice = "기재 편집 적용을 요청했습니다.";
            } else {
                menu.error = accepted.error();
            }
        }
    }

    ImGui::SameLine();

    if (ImGui::Button("파일에 저장") && validate_rows(editor, tab)) {
        editor.save_rows = own::make_shared<domain::TraitTextEdits>(rows);
        editor.save_embedded = tab != 0;
        editor.apply_external = false;
        editor.save_status.reset();
    }

    ImGui::SameLine();

    if (ImGui::Button("파일 다시 읽기 (편집 취소)")) {
        editor.reload_requested = true;
        editor.dirty.fill(false);
        editor.sources = {};
        editor.loaded_tab = -1;
        editor.loaded_row = -1;
        editor.validation = domain::TraitTextValidation::Valid;
    }

    ImGui::EndDisabled();

    if (external) {
        ImGui::TextDisabled("게임에 적용: 설정을 저장한 뒤 VERSION.dll의 설정 로드를 요청합니다.");
    }
    ImGui::TextDisabled("이름 5글자, 설명 512글자까지. 원문의 서식 자리표시자를 바꿀 수 없습니다.");
    ImGui::TextDisabled("저장 파일: %s", tab == 0 ? "trait_texts.ini" : "현재 로드된 기재 JSON 파일");
}

} // namespace

void update_trait_text_editor(TraitTextEditor& editor, const domain::TraitControlView& view,
    services::UiBridge& bridge) {

    const std::optional<services::ConfigSaveStatus> status = bridge.take_trait_text_save_result();

    if (status) {
        editor.save_pending = false;
        editor.save_status = *status;

        if (*status == services::ConfigSaveStatus::Completed) {
            editor.dirty[editor.save_embedded ? 1 : 0] = false;
            if (!editor.apply_external) {
                bridge.publish_notice("기재 편집 파일을 저장했습니다.");
            }
        } else if (*status == services::ConfigSaveStatus::Conflict) {
            bridge.publish_notice("기재 파일이 외부에서 변경되었습니다. 다시 읽은 뒤 저장해 주세요.");
        } else {
            if (!editor.apply_external) {
                bridge.publish_notice("기재 편집 파일을 저장하지 못했습니다.");
            }
        }
    }

    if (editor.save_rows && !editor.save_pending &&
        bridge.request_trait_text_save({editor.save_embedded, editor.save_rows, editor.apply_external})) {
        editor.save_pending = true;
        editor.save_rows = {};
    }

    if (editor.reload_requested) {
        bridge.request_trait_text_reload();
        editor.reload_requested = false;
    }

    const std::array<own::shared_owner<const domain::TraitTextEdits>, 2> incoming{view.legacy_rows, view.embedded_rows};

    for (std::size_t tab = 0; tab < incoming.size(); ++tab) {

        if (incoming[tab] && own::identity(incoming[tab]) != own::identity(editor.sources[tab]) &&
            !editor.dirty[tab] && !editor.save_pending && !editor.save_rows) {
            editor.sources[tab] = incoming[tab];
            editor.rows[tab] = *incoming[tab];
            editor.selected[tab] = -1;
            editor.loaded_tab = -1;
            editor.loaded_row = -1;
        }
    }
}

void draw_trait_text_editor_content(FeatureMenu& menu, services::CommandMailbox& mailbox) {
    TraitTextEditor& editor = menu.trait_text_editor;
    ImGui::Text("확장 기재: %s", domain::trait_apply_status_text(menu.snapshot.traits.catalog));

    if (menu.error) {
        ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.3f, 1.0f), "적용 요청 실패 (%d)", static_cast<int>(menu.error->code));
    }

    if (editor.save_pending || editor.save_rows) {
        ImGui::TextUnformatted("파일 저장 대기 중입니다.");
    } else if (editor.save_status) {
        ImGui::Text("%s: %s", editor.apply_external ? "저장·외부 적용 요청" : "파일 저장",
            *editor.save_status == services::ConfigSaveStatus::Completed ? "완료" :
            *editor.save_status == services::ConfigSaveStatus::Conflict ? "외부 변경과 충돌" : "실패");
    }

    draw_rows(menu, mailbox, 1);

}

} // namespace s8::overlay
