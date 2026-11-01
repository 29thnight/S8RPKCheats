#include "native_feature_control.h"
#include "runtime.h"

namespace s8::runtime {
namespace {

constexpr std::uint64_t RETRY_INTERVAL_MS = 100;
using PatchResult = platform::windows::CodePatchResult;

game::NativePatchGroup& patch_group(NativeFeatureControl& control, domain::NativeFeatureGroup group) {

    switch (group) {
    case domain::NativeFeatureGroup::AiWar: return control.ai_war.patches;
    case domain::NativeFeatureGroup::PrisonerCapture: return control.prisoner_capture.patches;
    case domain::NativeFeatureGroup::GovernorPrisoner: return control.governor_prisoner.patches;
    case domain::NativeFeatureGroup::StratagemGauge: return control.stratagem_gauge.patches;
    case domain::NativeFeatureGroup::IsolatedTerritory: return control.isolated_territory.patches;
    case domain::NativeFeatureGroup::ReinforcementPlacement: return control.reinforcement_placement.patches;
    case domain::NativeFeatureGroup::CouncilContinue: return control.council_continue.patches;
    case domain::NativeFeatureGroup::MissionCpuExclusion: return control.mission_cpu_exclusion.patches;
    case domain::NativeFeatureGroup::CouncilExecuteFree: return control.council_execute_free.patches;
    case domain::NativeFeatureGroup::AffinityDisplay: return control.affinity_display.patches;
    case domain::NativeFeatureGroup::RulerTransferProposal: return control.ruler_transfer.patches;
    default: return control.ai_war.patches;
    }
}

bool desired_group(const domain::FeatureFlags& flags, domain::NativeFeatureGroup group) {

    switch (group) {
    case domain::NativeFeatureGroup::AiWar:
        return flags.get(domain::FeatureId::AiWarImprove);
    case domain::NativeFeatureGroup::PrisonerCapture:
        return flags.get(domain::FeatureId::PrisonerCaptureManagement);
    case domain::NativeFeatureGroup::GovernorPrisoner:
        return flags.get(domain::FeatureId::GovernorPrisonerDisposal);
    case domain::NativeFeatureGroup::StratagemGauge:
        return flags.get(domain::FeatureId::MaxAttackStratagemGauge) ||
            flags.get(domain::FeatureId::MaxDefenseStratagemGauge);
    case domain::NativeFeatureGroup::IsolatedTerritory:
        return flags.get(domain::FeatureId::IsolatedTerritoryMovement);
    case domain::NativeFeatureGroup::ReinforcementPlacement:
        return flags.get(domain::FeatureId::ReinforcementDefenderPlacement);
    case domain::NativeFeatureGroup::CouncilContinue:
        return flags.get(domain::FeatureId::CouncilContinueAfterMove);
    case domain::NativeFeatureGroup::MissionCpuExclusion:
        return flags.get(domain::FeatureId::MissionCpuHeroExclusion);
    case domain::NativeFeatureGroup::CouncilExecuteFree:
        return flags.get(domain::FeatureId::CouncilExecuteFreeOfficers);
    case domain::NativeFeatureGroup::AffinityDisplay:
        return flags.get(domain::FeatureId::AffinityDisplay);
    case domain::NativeFeatureGroup::RulerTransferProposal:
        return flags.get(domain::FeatureId::RulerTransferProposal);
    default:
        return false;
    }
}

domain::NativeFeatureError feature_error(PatchResult result) {

    switch (result) {
    case PatchResult::Applied:
    case PatchResult::AlreadyApplied:
        return domain::NativeFeatureError::None;
    case PatchResult::InvalidInput: return domain::NativeFeatureError::InvalidInput;
    case PatchResult::ExpectedBytesMismatch: return domain::NativeFeatureError::SignatureMismatch;
    case PatchResult::Busy: return domain::NativeFeatureError::Busy;
    case PatchResult::ThreadEnumerationFailed: return domain::NativeFeatureError::ThreadEnumeration;
    case PatchResult::ThreadCapacityExceeded: return domain::NativeFeatureError::ThreadCapacity;
    case PatchResult::ThreadAccessFailed: return domain::NativeFeatureError::ThreadAccess;
    case PatchResult::ThreadSuspendFailed: return domain::NativeFeatureError::ThreadSuspend;
    case PatchResult::ThreadContextFailed: return domain::NativeFeatureError::ThreadContext;
    case PatchResult::WriteFailed: return domain::NativeFeatureError::Write;
    case PatchResult::RollbackFailed: return domain::NativeFeatureError::Rollback;
    case PatchResult::ResumeFailed: return domain::NativeFeatureError::Resume;
    default: return domain::NativeFeatureError::InvalidInput;
    }
}

bool apply_group(Runtime& runtime, std::uintptr_t module_base, domain::NativeFeatureGroup group,
    bool stopping) {

    NativeFeatureControl& control = runtime.native_features;
    const domain::FeatureFlags& flags = runtime.features;
    const bool enabled = !stopping && desired_group(flags, group);

    switch (group) {
    case domain::NativeFeatureGroup::AiWar:
        return game::set_ai_war_policy(control.ai_war, module_base, enabled);
    case domain::NativeFeatureGroup::PrisonerCapture:
        return game::set_prisoner_capture(control.prisoner_capture, module_base, enabled);
    case domain::NativeFeatureGroup::GovernorPrisoner:
        return game::set_governor_prisoner_disposal(control.governor_prisoner, module_base,
            enabled, !stopping && flags.get(domain::FeatureId::GovernorPrisonerConsumePrivilege));
    case domain::NativeFeatureGroup::StratagemGauge:
        return game::set_stratagem_gauge(control.stratagem_gauge, module_base, runtime.module_image,
            !stopping && flags.get(domain::FeatureId::MaxAttackStratagemGauge),
            !stopping && flags.get(domain::FeatureId::MaxDefenseStratagemGauge));
    case domain::NativeFeatureGroup::IsolatedTerritory:
        return game::set_isolated_territory(control.isolated_territory, module_base, enabled);
    case domain::NativeFeatureGroup::ReinforcementPlacement:
        return game::set_reinforcement_placement(control.reinforcement_placement, module_base, enabled);
    case domain::NativeFeatureGroup::CouncilContinue:
        return game::set_council_continue(control.council_continue, module_base, enabled);
    case domain::NativeFeatureGroup::MissionCpuExclusion:
        return game::set_mission_cpu_exclusion(control.mission_cpu_exclusion, module_base, enabled);
    case domain::NativeFeatureGroup::CouncilExecuteFree:
        return game::set_council_execute_free(control.council_execute_free, module_base, runtime.epoch, enabled);
    case domain::NativeFeatureGroup::AffinityDisplay:
        return game::set_affinity_display(control.affinity_display, module_base, enabled);
    case domain::NativeFeatureGroup::RulerTransferProposal:
        return game::set_ruler_transfer_proposal(control.ruler_transfer, module_base, enabled);
    default:
        return false;
    }
}

bool requires_stop(NativeFeatureControl& control, domain::NativeFeatureGroup group) {

    const game::NativePatchGroup& patches = patch_group(control, group);

    return patches.applied || patches.uncertain ||
        (group == domain::NativeFeatureGroup::CouncilExecuteFree && control.council_execute_free.preparing_enable);
}

void record_result(NativeFeatureControl& control, domain::NativeFeatureGroup group, bool succeeded,
    std::uint64_t now_ms) {

    const std::size_t index = static_cast<std::size_t>(group);
    const game::NativePatchGroup& patches = patch_group(control, group);
    domain::NativeFeatureStatus& status = control.statuses[index];
    const domain::NativeFeatureStatus previous = status;

    status.applied = patches.applied;
    status.error = succeeded ? domain::NativeFeatureError::None : feature_error(patches.last_result);

    if (succeeded) {
        status.phase = patches.applied ? domain::NativeFeaturePhase::Applied : domain::NativeFeaturePhase::Disabled;
        control.pending[index] = false;
    } else if (patches.last_result == PatchResult::Busy && !patches.uncertain) {
        status.phase = domain::NativeFeaturePhase::Pending;
        control.retry_after_ms[index] = now_ms + RETRY_INTERVAL_MS;
        return;
    } else {
        status.phase = patches.uncertain ? domain::NativeFeaturePhase::Uncertain : domain::NativeFeaturePhase::Failed;
        control.pending[index] = false;
    }

    if (control.announce[index] || (!succeeded &&
        (previous.phase != status.phase || previous.error != status.error))) {
        control.notice = NativeFeatureNotice{group, status};
    }

    control.announce[index] = false;
}

} // namespace

bool request_native_feature(Runtime& runtime, domain::FeatureId feature, bool enabled) {

    const std::optional<domain::NativeFeatureGroup> group = domain::native_feature_group(feature);

    if (!group) {
        return false;
    }

    NativeFeatureControl& control = runtime.native_features;
    const std::size_t index = static_cast<std::size_t>(*group);

    runtime.features.set(feature, enabled);
    control.pending[index] = true;
    control.announce[index] = true;
    control.retry_after_ms[index] = 0;
    control.statuses[index].phase = domain::NativeFeaturePhase::Pending;
    control.statuses[index].error = domain::NativeFeatureError::None;
    return true;
}

void retry_failed_native_features(Runtime& runtime) {
    if (runtime.stopping) {
        return;
    }
    NativeFeatureControl& control = runtime.native_features;

    for (std::size_t index = 0; index < domain::NATIVE_FEATURE_GROUP_COUNT; ++index) {
        const domain::NativeFeatureGroup group = static_cast<domain::NativeFeatureGroup>(index);
        const game::NativePatchGroup& patches = patch_group(control, group);
        domain::NativeFeatureStatus& status = control.statuses[index];

        if (status.phase != domain::NativeFeaturePhase::Failed ||
            status.error != domain::NativeFeatureError::SignatureMismatch ||
            !desired_group(runtime.features, group) || patches.applied || patches.uncertain) {
            continue;
        }
        control.pending[index] = true;
        control.retry_after_ms[index] = 0;
        status.phase = domain::NativeFeaturePhase::Pending;
        status.error = domain::NativeFeatureError::None;
    }
}

bool reconcile_native_features(Runtime& runtime, std::uintptr_t module_base, std::uint64_t now_ms) {

    NativeFeatureControl& control = runtime.native_features;

    if (!runtime.config_loaded) {
        return false;
    }

    if (!control.initialized) {
        control.initialized = true;

        for (std::size_t index = 0; index < domain::NATIVE_FEATURE_GROUP_COUNT; ++index) {

            const domain::NativeFeatureGroup group = static_cast<domain::NativeFeatureGroup>(index);

            if (desired_group(runtime.features, group)) {
                control.pending[index] = true;
                control.statuses[index].phase = domain::NativeFeaturePhase::Pending;
            }
        }
    }

    if (control.council_reset_pending && now_ms >= control.council_reset_after_ms) {

        const bool reset = game::reset_council_continue(control.council_continue);
        constexpr std::size_t index = static_cast<std::size_t>(domain::NativeFeatureGroup::CouncilContinue);
        domain::NativeFeatureStatus& status = control.statuses[index];
        const domain::NativeFeatureStatus previous = status;
        const game::NativePatchGroup& patches = control.council_continue.patches;

        control.council_reset_pending = !reset;
        control.council_reset_after_ms = now_ms + RETRY_INTERVAL_MS;
        status.applied = patches.applied;
        status.error = reset ? domain::NativeFeatureError::None : feature_error(patches.last_result);

        if (reset) {
            status.phase = control.pending[index] ? domain::NativeFeaturePhase::Pending :
                (patches.applied ? domain::NativeFeaturePhase::Applied : domain::NativeFeaturePhase::Disabled);
        } else if (patches.last_result == PatchResult::Busy && !patches.uncertain) {
            status.phase = domain::NativeFeaturePhase::Pending;
        } else {
            status.phase = patches.uncertain ? domain::NativeFeaturePhase::Uncertain : domain::NativeFeaturePhase::Failed;

            if (previous.phase != status.phase || previous.error != status.error) {
                control.notice = NativeFeatureNotice{domain::NativeFeatureGroup::CouncilContinue, status};
            }
        }

        return true;
    }

    for (std::size_t offset = 0; offset < domain::NATIVE_FEATURE_GROUP_COUNT; ++offset) {

        const std::size_t index = (control.next_group + offset) % domain::NATIVE_FEATURE_GROUP_COUNT;

        if (!control.pending[index] || now_ms < control.retry_after_ms[index]) {
            continue;
        }

        const domain::NativeFeatureGroup group = static_cast<domain::NativeFeatureGroup>(index);
        const bool succeeded = apply_group(runtime, module_base, group, false);

        control.next_group = (index + 1) % domain::NATIVE_FEATURE_GROUP_COUNT;
        record_result(control, group, succeeded, now_ms);
        return true;
    }

    return false;
}

void change_native_feature_epoch(Runtime& runtime) {

    NativeFeatureControl& control = runtime.native_features;

    game::change_reinforcement_epoch(runtime.epoch);
    game::reset_council_execute_free(control.council_execute_free, runtime.epoch);
    control.council_reset_pending = control.council_continue.cave != 0;
    control.council_reset_after_ms = 0;
}

void update_native_feature_candidates(Runtime& runtime, std::uintptr_t module_base,
    const domain::WorkBudget& work_budget) {

    game::CouncilExecuteFree& state = runtime.native_features.council_execute_free;

    if (state.preparing_enable || state.patches.applied) {
        game::update_council_execute_candidates(state, module_base, runtime.epoch, 128, work_budget);
    }
}

domain::Result<void> stop_native_features(Runtime& runtime, std::uintptr_t module_base) {

    NativeFeatureControl& control = runtime.native_features;

    control.pending.fill(false);
    control.council_reset_pending = false;

    for (std::size_t offset = 0; offset < domain::NATIVE_FEATURE_GROUP_COUNT; ++offset) {

        const std::size_t index = (control.next_group + offset) % domain::NATIVE_FEATURE_GROUP_COUNT;
        const domain::NativeFeatureGroup group = static_cast<domain::NativeFeatureGroup>(index);

        if (!requires_stop(control, group)) {
            continue;
        }

        const bool succeeded = apply_group(runtime, module_base, group, true);
        const game::NativePatchGroup& patches = patch_group(control, group);

        control.next_group = (index + 1) % domain::NATIVE_FEATURE_GROUP_COUNT;
        record_result(control, group, succeeded, 0);

        if (!succeeded) {
            return std::unexpected(domain::Error{
                patches.last_result == PatchResult::Busy ? domain::ErrorCode::QueueFull : domain::ErrorCode::PlatformFailure,
                static_cast<std::uint32_t>(patches.last_result)});
        }

        for (std::size_t remaining = 0; remaining < domain::NATIVE_FEATURE_GROUP_COUNT; ++remaining) {

            if (requires_stop(control, static_cast<domain::NativeFeatureGroup>(remaining))) {
                return std::unexpected(domain::Error{domain::ErrorCode::QueueFull,
                    static_cast<std::uint32_t>(PatchResult::Busy)});
            }
        }

        return {};
    }

    return {};
}

} // namespace s8::runtime
