#pragma once

#include "feature_flags.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace s8::domain {

enum class NativeFeatureGroup : std::uint8_t {
    AiWar,
    PrisonerCapture,
    GovernorPrisoner,
    StratagemGauge,
    IsolatedTerritory,
    ReinforcementPlacement,
    CouncilContinue,
    MissionCpuExclusion,
    CouncilExecuteFree,
    AffinityDisplay,
    RulerTransferProposal,
    Count
};

enum class NativeFeaturePhase : std::uint8_t {
    Disabled,
    Pending,
    Applied,
    Failed,
    Uncertain
};

enum class NativeFeatureError : std::uint8_t {
    None,
    InvalidInput,
    SignatureMismatch,
    Busy,
    ThreadEnumeration,
    ThreadCapacity,
    ThreadAccess,
    ThreadSuspend,
    ThreadContext,
    Write,
    Rollback,
    Resume
};

struct NativeFeatureStatus {
    NativeFeaturePhase phase = NativeFeaturePhase::Disabled;
    NativeFeatureError error = NativeFeatureError::None;
    bool applied = false;
};

inline constexpr std::size_t NATIVE_FEATURE_GROUP_COUNT =
    static_cast<std::size_t>(NativeFeatureGroup::Count);
using NativeFeatureStatuses = std::array<NativeFeatureStatus, NATIVE_FEATURE_GROUP_COUNT>;

constexpr std::optional<NativeFeatureGroup> native_feature_group(FeatureId feature) {

    switch (feature) {
    case FeatureId::AiWarImprove:
        return NativeFeatureGroup::AiWar;
    case FeatureId::PrisonerCaptureManagement:
        return NativeFeatureGroup::PrisonerCapture;
    case FeatureId::GovernorPrisonerDisposal:
    case FeatureId::GovernorPrisonerConsumePrivilege:
        return NativeFeatureGroup::GovernorPrisoner;
    case FeatureId::MaxAttackStratagemGauge:
    case FeatureId::MaxDefenseStratagemGauge:
        return NativeFeatureGroup::StratagemGauge;
    case FeatureId::IsolatedTerritoryMovement:
        return NativeFeatureGroup::IsolatedTerritory;
    case FeatureId::ReinforcementDefenderPlacement:
        return NativeFeatureGroup::ReinforcementPlacement;
    case FeatureId::CouncilContinueAfterMove:
        return NativeFeatureGroup::CouncilContinue;
    case FeatureId::MissionCpuHeroExclusion:
        return NativeFeatureGroup::MissionCpuExclusion;
    case FeatureId::CouncilExecuteFreeOfficers:
        return NativeFeatureGroup::CouncilExecuteFree;
    case FeatureId::AffinityDisplay:
        return NativeFeatureGroup::AffinityDisplay;
    case FeatureId::RulerTransferProposal:
        return NativeFeatureGroup::RulerTransferProposal;
    default:
        return std::nullopt;
    }
}

constexpr const char* native_feature_label(NativeFeatureGroup group) {

    switch (group) {
    case NativeFeatureGroup::AiWar: return "AI 전쟁 개선";
    case NativeFeatureGroup::PrisonerCapture: return "포로 포획 관리";
    case NativeFeatureGroup::GovernorPrisoner: return "태수 포로 처분";
    case NativeFeatureGroup::StratagemGauge: return "전법 게이지";
    case NativeFeatureGroup::IsolatedTerritory: return "단절 영토 무장 이동 제한";
    case NativeFeatureGroup::ReinforcementPlacement: return "수비 증원 배치";
    case NativeFeatureGroup::CouncilContinue: return "이동 후 평정 계속";
    case NativeFeatureGroup::MissionCpuExclusion: return "CPU 임무에서 주인공 제외";
    case NativeFeatureGroup::CouncilExecuteFree: return "세력 도시 재야 무장 처단";
    case NativeFeatureGroup::AffinityDisplay: return "상성 수치 표시";
    case NativeFeatureGroup::RulerTransferProposal: return "부하의 군주 이동 제안";
    default: return "게임 패치";
    }
}

constexpr const char* native_feature_error_text(NativeFeatureError error) {

    switch (error) {
    case NativeFeatureError::None: return "없음";
    case NativeFeatureError::InvalidInput: return "게임 모듈 또는 입력을 확인할 수 없음";
    case NativeFeatureError::SignatureMismatch: return "게임 코드 서명 불일치";
    case NativeFeatureError::Busy: return "실행 중인 코드 또는 후보 준비 대기";
    case NativeFeatureError::ThreadEnumeration: return "게임 스레드 목록 확인 실패";
    case NativeFeatureError::ThreadCapacity: return "게임 스레드 수가 안전 한도를 초과함";
    case NativeFeatureError::ThreadAccess: return "게임 스레드 접근 실패";
    case NativeFeatureError::ThreadSuspend: return "게임 스레드 일시 정지 실패";
    case NativeFeatureError::ThreadContext: return "게임 스레드 실행 위치 확인 실패";
    case NativeFeatureError::Write: return "패치 준비 또는 메모리 쓰기 실패";
    case NativeFeatureError::Rollback: return "실패한 패치 복구를 확인할 수 없음";
    case NativeFeatureError::Resume: return "게임 스레드 재개를 확인할 수 없음";
    default: return "알 수 없는 오류";
    }
}

} // namespace s8::domain
