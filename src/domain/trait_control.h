#pragma once

#include "ownership.h"
#include "trait_text_edit.h"

#include <cstdint>

namespace s8::domain {

enum class TraitApplyStatus : std::uint8_t {
    Disabled,
    Pending,
    WaitingForTable,
    Applied,
    UnsupportedVersion,
    InvalidText,
    AllocationFailed,
    HookFailed,
    MemoryFailed,
    Conflict,
    CallbackBusy,
    ExternalRuntime,
    LiveObjects,
    RollbackFailed
};

struct TraitControlView {
    TraitApplyStatus viewer = TraitApplyStatus::Disabled;
    TraitApplyStatus catalog = TraitApplyStatus::Disabled;
    TraitApplyStatus legacy_text = TraitApplyStatus::Disabled;
    bool viewer_applied = false;
    own::shared_owner<const TraitTextEdits> legacy_rows;
    own::shared_owner<const TraitTextEdits> embedded_rows;
    std::uint64_t source_generation = 0;
};

struct ApplyTraitTextEdits {
    bool embedded = false;
    own::shared_owner<const TraitTextEdits> rows;
};

constexpr const char* trait_apply_status_text(TraitApplyStatus status) {

    switch (status) {
    case TraitApplyStatus::Disabled: return "해제됨";
    case TraitApplyStatus::Pending: return "적용 대기";
    case TraitApplyStatus::WaitingForTable: return "게임 기재 테이블 대기";
    case TraitApplyStatus::Applied: return "적용됨";
    case TraitApplyStatus::UnsupportedVersion: return "게임 코드 또는 버전 불일치";
    case TraitApplyStatus::InvalidText: return "텍스트 형식이 올바르지 않음";
    case TraitApplyStatus::AllocationFailed: return "패치 저장 공간 확보 실패";
    case TraitApplyStatus::HookFailed: return "게임 콜백 연결 실패";
    case TraitApplyStatus::MemoryFailed: return "메모리 적용 또는 복구 실패";
    case TraitApplyStatus::Conflict: return "게임 데이터가 변경되어 적용 또는 복구 보류";
    case TraitApplyStatus::CallbackBusy: return "사용 중인 게임 콜백 대기";
    case TraitApplyStatus::ExternalRuntime: return "외부 version.dll에서 확장 기재 관리 중";
    case TraitApplyStatus::LiveObjects: return "이미 열린 게임 UI의 종료 대기";
    case TraitApplyStatus::RollbackFailed: return "이전 패치 복구를 확인할 수 없음";
    default: return "알 수 없는 상태";
    }
}

constexpr const char* trait_text_validation_text(TraitTextValidation validation) {

    switch (validation) {
    case TraitTextValidation::Valid: return "유효함";
    case TraitTextValidation::InvalidId: return "기재 번호가 올바르지 않습니다.";
    case TraitTextValidation::InvalidUtf8: return "올바르지 않은 UTF-8 문자가 있습니다.";
    case TraitTextValidation::NameTooLong: return "이름은 UTF-16 기준 5글자까지 입력할 수 있습니다.";
    case TraitTextValidation::DescriptionTooLong: return "설명은 UTF-16 기준 512글자까지 입력할 수 있습니다.";
    case TraitTextValidation::FormatMismatch: return "설명의 서식 자리표시자는 원문의 순서와 형식을 유지해야 합니다.";
    default: return "텍스트를 확인해 주세요.";
    }
}

} // namespace s8::domain
