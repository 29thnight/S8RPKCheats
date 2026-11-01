#include "trait_text_service.h"
#include "trait_text_config.h"
#include "../platform/windows/config_commit.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <array>
#include <string_view>
#include <utility>

namespace s8::services {

TraitTextService::TraitTextService(std::filesystem::path directory)
    : legacy_path(std::move(directory) / L"trait_texts.ini") {}

void TraitTextService::load_legacy(UiBridge& bridge) {
    const domain::Result<std::optional<std::string>> loaded =
        platform::windows::read_config_for_save(legacy_path);

    if (!loaded) {
        bridge.publish_notice("특기 텍스트 설정 파일을 읽지 못했습니다.");
        return;
    }
    domain::Result<domain::TraitTextEdits> rows = read_legacy_trait_text_config(
        *loaded ? std::string_view(**loaded) : std::string_view{});

    if (!rows) {
        bridge.publish_notice("특기 텍스트 설정이 유효하지 않아 이전 편집값을 유지합니다.");
        return;
    }

    legacy_baseline = *loaded;
    legacy_loaded = true;
    bridge.publish_legacy_trait_texts(own::make_shared<domain::TraitTextEdits>(std::move(*rows)));
}

void TraitTextService::set_embedded_source(std::filesystem::path source, std::string contents) {
    embedded_path = std::move(source);
    embedded_baseline = std::move(contents);
}

void TraitTextService::clear_embedded_source() {
    embedded_path.clear();
    embedded_baseline.reset();
}

void TraitTextService::poll(UiBridge& bridge) {
    const std::optional<TraitTextSaveRequest> request = bridge.take_trait_text_save();

    if (!request) {
        return;
    }
    if (!request->rows || (request->embedded ? !embedded_baseline : !legacy_loaded)) {
        bridge.publish_trait_text_save_result(ConfigSaveStatus::InvalidData);
        return;
    }

    domain::Result<std::string> bytes = request->embedded
        ? update_embedded_trait_text_config(*embedded_baseline, *request->rows)
        : write_legacy_trait_text_config(*request->rows);

    if (!bytes) {
        bridge.publish_trait_text_save_result(ConfigSaveStatus::InvalidData);
        return;
    }

    const std::filesystem::path& path = request->embedded ? embedded_path : legacy_path;
    std::optional<std::string>& baseline = request->embedded ? embedded_baseline : legacy_baseline;
    const platform::windows::ConfigCommitResult committed =
        platform::windows::commit_config(path, baseline, *bytes, {});
    ConfigSaveStatus status = ConfigSaveStatus::Failed;

    switch (committed.status) {
    case platform::windows::ConfigCommitStatus::Completed:
        baseline = std::move(*bytes);
        status = ConfigSaveStatus::Completed;

        if (request->apply_external) {
            // VERSION.dll v1.4.7: F9 editor's Settings Load button.
            HWND editor = nullptr;
            HWND candidate = nullptr;
            HMODULE owner = nullptr;
            std::array<wchar_t, 32768> module_path{};

            while ((candidate = FindWindowExW(nullptr, candidate, L"SAN8R_EditorGUI", nullptr)) != nullptr) {
                DWORD process_id = 0;

                GetWindowThreadProcessId(candidate, &process_id);
                if (process_id == GetCurrentProcessId()) {
                    editor = candidate;
                    break;
                }
            }
            const LONG_PTR procedure = editor ? GetWindowLongPtrW(editor, GWLP_WNDPROC) : 0;
            const std::filesystem::path expected_module = embedded_path.parent_path() / L"version.dll";
            const bool supported = procedure &&
                GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                    reinterpret_cast<LPCWSTR>(procedure), &owner) &&
                GetModuleFileNameW(owner, module_path.data(), static_cast<DWORD>(module_path.size())) &&
                CompareStringOrdinal(module_path.data(), -1, expected_module.c_str(), -1, TRUE) == CSTR_EQUAL &&
                static_cast<std::uintptr_t>(procedure) - reinterpret_cast<std::uintptr_t>(owner) == 0x14cc0;
            DWORD_PTR result = 0;

            if (!supported) {
                bridge.publish_notice("기재 설정은 저장했지만 호환되는 VERSION.dll 기재 에디터를 찾지 못했습니다.");
                status = ConfigSaveStatus::Failed;
            } else if (!SendMessageTimeoutW(editor, WM_COMMAND, 0x40a, 0,
                    SMTO_ABORTIFHUNG | SMTO_BLOCK, 3000, &result)) {
                bridge.publish_notice("기재 설정은 저장했지만 VERSION.dll의 적용 요청 처리가 완료되지 않았습니다.");
                status = ConfigSaveStatus::Failed;
            } else {
                bridge.publish_notice("기재 설정을 저장하고 VERSION.dll에 설정 로드 명령을 전달했습니다.");
            }
        }
        break;
    case platform::windows::ConfigCommitStatus::Conflict:
        status = ConfigSaveStatus::Conflict;
        break;
    case platform::windows::ConfigCommitStatus::Cancelled:
        status = ConfigSaveStatus::Cancelled;
        break;
    case platform::windows::ConfigCommitStatus::Failed:
        break;
    }
    bridge.publish_trait_text_save_result(status);
}

} // namespace s8::services
