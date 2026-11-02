#include "trait_text_editor.h"
#include "native_trait_editor.h"

#include "feature_menu.h"
#include "../services/ui_bridge.h"
#include "../platform/windows/hook.h"
#include "../../third_party/imgui_legacy/imgui.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <string_view>
#include <utility>
#include <mutex>
#include <atomic>
#include <memory>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

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

NativeTraitDraft::NativeTraitDraft(domain::TraitTextEdit source) : row_(std::move(source)) {}

const std::string& NativeTraitDraft::name() const {
    return row_.name.empty() ? row_.original_name : row_.name;
}

const std::string& NativeTraitDraft::description() const {
    return row_.description.empty() ? row_.original_description : row_.description;
}

std::string NativeTraitDraft::effect_description(std::size_t slot,
    const domain::TraitEffectDefinitions& definitions) const {
    const domain::TraitEffect& effect = (*row_.effects)[slot];
    if (effect.effect_id == 0) return "(없음)";

    const auto found = definitions.find(effect.effect_id);
    if (found == definitions.end()) {
        return "효과 " + std::to_string(effect.effect_id) + " / 값 " +
            std::to_string(effect.value1) + " / P " + std::to_string(effect.value2);
    }
    const auto& definition = found->second;
    std::string text = definition.text;
    const auto value = definition.values.find(effect.value1);
    const auto param = definition.params.find(effect.value2);
    const std::array<std::pair<std::string_view, std::string>, 2> substitutions{{
        {"{value}", value == definition.values.end() ? std::to_string(effect.value1) : value->second},
        {"{param}", param == definition.params.end() ? std::to_string(effect.value2) : param->second}
    }};
    for (const auto& [token, replacement] : substitutions) {
        std::size_t offset = 0;
        while ((offset = text.find(token, offset)) != std::string::npos) {
            text.replace(offset, token.size(), replacement);
            offset += replacement.size();
        }
    }
    // Both stored fields remain visible even when the effect has no placeholder.
    return text + "  [값 " + std::to_string(effect.value1) + " / P " +
        std::to_string(effect.value2) + "]";
}

domain::TraitTextValidation NativeTraitDraft::set_name(std::string text) {
    domain::TraitTextEdit candidate = row_;
    candidate.name = text == row_.original_name ? std::string{} : std::move(text);
    const auto validation = domain::validate_trait_text_edit(candidate);
    if (validation == domain::TraitTextValidation::Valid) row_ = std::move(candidate);
    return validation;
}

domain::TraitTextValidation NativeTraitDraft::set_description(std::string text) {
    domain::TraitTextEdit candidate = row_;
    candidate.description = text == row_.original_description ? std::string{} : std::move(text);
    const auto validation = domain::validate_trait_text_edit(candidate);
    if (validation == domain::TraitTextValidation::Valid) row_ = std::move(candidate);
    return validation;
}

void NativeTraitDraft::set_effect(std::size_t slot, domain::TraitEffect effect) {
    (*row_.effects)[slot] = effect.effect_id == 0 ? domain::TraitEffect{} : effect;
}

void NativeTraitDraft::input_text(std::uintptr_t module, bool edit_description,
    std::function<void(NativeTraitInputResult, domain::TraitTextValidation)> completion) {
    // CInputStringState configuration copied at RVA 0x32860. Its native byte
    // strings use the game's language code page, not UTF-8 (RVA 0x17F3250).
    struct InputConfig {
        std::int32_t x, y, width, height;
        const char* initial;
        std::int32_t limit, rows;
        char* output;
        std::int32_t background_width, font_size, flags, reserved;
        std::array<char, 16> string_inline;
        std::uint64_t string_length, string_capacity;
        std::int32_t option1, option2, foreground_layer, padding;
    };
    static_assert(sizeof(InputConfig) == 0x68);
    static_assert(offsetof(InputConfig, output) == 0x20);
    static_assert(offsetof(InputConfig, string_inline) == 0x38);
    static_assert(sizeof(std::function<void(void*)>) == 0x40);

    const std::string& source = edit_description ? description() : name();
    std::wstring wide(source.size(), L'\0');
    const int units = source.empty() ? 0 : MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
        source.data(), static_cast<int>(source.size()), wide.data(), static_cast<int>(wide.size()));
    if (!source.empty() && units == 0) {
        completion(NativeTraitInputResult::InvalidText, domain::TraitTextValidation::InvalidUtf8);
        return;
    }
    const auto code_page = reinterpret_cast<unsigned(*)()>(module + 0x17F3250)();
    input_.fill('\0');
    BOOL substituted = FALSE;
    const int bytes = units == 0 ? 0 : WideCharToMultiByte(code_page, WC_NO_BEST_FIT_CHARS,
        wide.data(), units, input_.data(), static_cast<int>(input_.size() - 1), nullptr, &substituted);
    if (substituted || (units != 0 && bytes == 0)) {
        completion(NativeTraitInputResult::UnrepresentableText, domain::TraitTextValidation::Valid);
        return;
    }
    initial_input_.assign(input_.data(), static_cast<std::size_t>(bytes));
    InputConfig config{};
    config.x = edit_description ? 480 : 600;
    config.y = edit_description ? 240 : 430;
    config.width = config.background_width = edit_description ? 960 : 720;
    config.height = edit_description ? 600 : 96;
    config.initial = initial_input_.c_str();
    config.limit = edit_description ? 513 : 6;
    config.rows = edit_description ? 16 : 1;
    config.output = input_.data();
    config.font_size = 24;
    config.string_capacity = 15;
    config.option1 = config.option2 = 1;
    // CInputStringState::Init (0x1EC6296) sets layout layer 12 when this
    // flag is enabled. The retained detail dialog is on layer 6.
    config.foreground_layer = 1;
    const auto manager = reinterpret_cast<void*(*)()>(module + 0xF0B0)();
    // Native text editing (0x135F71F) forces text/mouse input mode while
    // CInputStringState is active, instead of automatic keyboard navigation.
    auto* ui_manager = static_cast<std::byte*>(reinterpret_cast<void*(*)()>(module + 0xF410)());
    auto* input_mode = reinterpret_cast<int*>(ui_manager + 0x608);
    const int previous_input_mode = *input_mode;
    *input_mode = 0;
    // UI activation runs outside the state coroutine (manager+0x48 is null).
    // Queue input and return to the UI dispatcher; do not suspend/resume a
    // coroutine here. The completion updates the retained detail layout.
    std::function<void(void*)> closed =
        [this, edit_description, code_page, input_mode, previous_input_mode,
            completion = std::move(completion)](void* state) {
            *input_mode = previous_input_mode;
            // Native teardown copies output even on cancel. Never consume it
            // unless the state reports the confirmed result.
            const int result = *reinterpret_cast<const int*>(static_cast<const char*>(state) + 8);
            if (result != 0x7FFFFFFD) {
                completion(NativeTraitInputResult::Cancelled, domain::TraitTextValidation::Valid);
                return;
            }
            std::array<wchar_t, 4096> converted{};
            const int count = MultiByteToWideChar(code_page, MB_ERR_INVALID_CHARS,
                input_.data(), -1, converted.data(), static_cast<int>(converted.size()));
            if (count == 0) {
                completion(NativeTraitInputResult::InvalidText, domain::TraitTextValidation::InvalidUtf8);
                return;
            }
            std::string text(static_cast<std::size_t>(count - 1) * 3, '\0');
            const int length = count == 1 ? 0 : WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                converted.data(), count - 1, text.data(), static_cast<int>(text.size()), nullptr, nullptr);
            if (count != 1 && length == 0) {
                completion(NativeTraitInputResult::InvalidText, domain::TraitTextValidation::InvalidUtf8);
                return;
            }
            text.resize(static_cast<std::size_t>(length));
            const auto validation = edit_description ? set_description(std::move(text)) : set_name(std::move(text));
            completion(validation == domain::TraitTextValidation::Valid ? NativeTraitInputResult::Accepted :
                NativeTraitInputResult::InvalidText, validation);
        };
    using Enqueue = void(*)(void*, const char*, const InputConfig*, std::function<void(void*)>*);
    reinterpret_cast<Enqueue>(module + 0x32860)(manager, "CInputStringState", &config, &closed);
}

void NativeTraitDraft::input_number(std::uintptr_t module, std::uint16_t initial,
    std::function<void(bool, std::uint16_t)> completion) {
    // RVA 0x173120 copies this 0x28-byte configuration. The native number
    // dialog writes int32, so its destination must not be a TraitEffect field.
    struct NumberConfig {
        std::int32_t x, y, reserved1, reserved2, minimum, maximum;
        std::int32_t* output;
        std::int32_t flags, padding;
    };
    static_assert(sizeof(NumberConfig) == 0x28);
    number_ = initial;
    NumberConfig config{960, 540, 0, 0, 0, 65535, &number_, 0x1A, 0};
    std::function<void(void*)> closed = [this, completion = std::move(completion)](void* state) {
        const int result = *reinterpret_cast<const int*>(static_cast<const char*>(state) + 8);
        const bool accepted = result == 0x7FFFFFFD;
        completion(accepted, accepted ? static_cast<std::uint16_t>(number_) : std::uint16_t{});
    };
    const auto manager = reinterpret_cast<void*(*)()>(module + 0xF0B0)();
    using Enqueue = void(*)(void*, const char*, const NumberConfig*, std::function<void(void*)>*);
    reinterpret_cast<Enqueue>(module + 0x173120)(manager, "CInputKeyDlgState", &config, &closed);
}

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

#include "native_trait_editor.inl"

} // namespace s8::overlay
