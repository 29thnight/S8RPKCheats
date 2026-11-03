#include "trait_text_editor.h"
#include "native_trait_editor.h"

#include "feature_menu.h"
#include "../services/ui_bridge.h"
#include "../platform/windows/hook.h"
#include "../domain/secondary_jewel_definitions.h"
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

void draw_rows(FeatureMenu& menu, std::size_t tab) {
    auto& editor = menu.trait_text_editor;
    const auto& source = editor.sources[tab];
    if (!source || source->empty()) {
        ImGui::TextUnformatted("공개된 기재 데이터가 없습니다.");
        return;
    }
    ImGui::Text("항목 수: %u", static_cast<unsigned>(source->size()));
    ImGui::InputTextWithHint("##trait_filter", "이름 검색", editor.filter.data(), editor.filter.size());
    if (ImGui::BeginTable("trait_debug_rows", 2, ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn("기재", ImGuiTableColumnFlags_WidthFixed, 210.0f * menu.dpi_scale);
        ImGui::TableSetupColumn("정보", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextColumn();
        ImGui::BeginChild("trait_names", ImVec2(0, 330.0f * menu.dpi_scale), true);
        const std::string_view filter(editor.filter.data());
        for (std::size_t index = 0; index < source->size(); ++index) {
            const auto& row = (*source)[index];
            const auto& name = row.name.empty() ? row.original_name : row.name;
            if (!filter.empty() && name.find(filter) == std::string::npos &&
                row.original_name.find(filter) == std::string::npos) continue;
            ImGui::PushID(static_cast<int>(index));
            if (ImGui::Selectable(name.c_str(), editor.selected[tab] == static_cast<int>(index))) {
                editor.selected[tab] = static_cast<int>(index);
            }
            ImGui::SameLine();
            ImGui::TextDisabled("%u", static_cast<unsigned>(row.trait_id));
            ImGui::PopID();
        }
        ImGui::EndChild();
        ImGui::TableNextColumn();
        const int selected = editor.selected[tab];
        if (selected >= 0 && static_cast<std::size_t>(selected) < source->size()) {
            const auto& row = (*source)[static_cast<std::size_t>(selected)];
            ImGui::Text("ID: %u", static_cast<unsigned>(row.trait_id));
            ImGui::TextWrapped("이름: %s", row.name.c_str());
            ImGui::TextWrapped("설명: %s", row.description.c_str());
            ImGui::TextWrapped("원문 이름: %s", row.original_name.c_str());
            ImGui::TextWrapped("원문 설명: %s", row.original_description.c_str());
            if (row.effects) {
                for (std::size_t slot = 0; slot < row.effects->size(); ++slot) {
                    const auto& effect = (*row.effects)[slot];
                    ImGui::Text("효과 %u: ID %u / 값 %u / P %u", static_cast<unsigned>(slot + 1),
                        static_cast<unsigned>(effect.effect_id), static_cast<unsigned>(effect.value1),
                        static_cast<unsigned>(effect.value2));
                }
            }
            ImGui::TextWrapped("텍스트 검증: %s",
                domain::trait_text_validation_text(domain::validate_trait_text_edit(row)));
        } else {
            ImGui::TextUnformatted("왼쪽에서 조회할 기재를 선택하세요.");
        }
        ImGui::EndTable();
    }
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

    editor.sources = {view.legacy_rows, view.embedded_rows};
}

void draw_trait_debug_window(FeatureMenu& menu) {
    auto& editor = menu.trait_text_editor;
    if (!editor.debug_enabled) return;
    ImGui::SetNextWindowSize(ImVec2(760 * menu.dpi_scale, 520 * menu.dpi_scale), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("기재 디버그###S8RPK.TraitDebug", &editor.debug_enabled)) {
        const auto& traits = menu.snapshot.traits;
        ImGui::TextUnformatted("런타임에서 공개한 기재 데이터 · 읽기 전용");
        ImGui::Text("확장 기재: %s", domain::trait_apply_status_text(traits.catalog));
        ImGui::Text("기본 텍스트: %s", domain::trait_apply_status_text(traits.legacy_text));
        ImGui::Text("기재 표시: %s / 실제 적용: %s", domain::trait_apply_status_text(traits.viewer),
            traits.viewer_applied ? "예" : "아니오");
        ImGui::Text("데이터 세대: %llu", static_cast<unsigned long long>(traits.source_generation));
        if (editor.save_pending || editor.save_rows) ImGui::TextUnformatted("네이티브 편집 저장 대기 중");
        if (editor.save_status) {
            ImGui::Text("최근 저장: %s", *editor.save_status == services::ConfigSaveStatus::Completed ? "완료" :
                *editor.save_status == services::ConfigSaveStatus::Conflict ? "외부 변경과 충돌" : "실패");
        }
        if (ImGui::BeginTabBar("trait_debug_sources")) {
            if (ImGui::BeginTabItem("기본 텍스트")) {
                draw_rows(menu, 0);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("확장 기재")) {
                draw_rows(menu, 1);
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}

#include "native_trait_editor.inl"

} // namespace s8::overlay
