#pragma once

#include "../domain/error.h"
#include "../domain/native_features.h"
#include "../game/ai_war_policy.h"
#include "../game/prisoner_capture.h"
#include "../game/governor_prisoner_disposal.h"
#include "../game/stratagem_gauge.h"
#include "../game/isolated_territory.h"
#include "../game/reinforcement_placement.h"
#include "../game/council_continue.h"
#include "../game/mission_cpu_exclusion.h"
#include "../game/council_execute_free.h"
#include "../game/affinity_display.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace s8::runtime {

struct Runtime;

struct NativeFeatureNotice {
    domain::NativeFeatureGroup group;
    domain::NativeFeatureStatus status;
};

struct NativeFeatureControl {
    game::AiWarPolicy ai_war;
    game::PrisonerCapture prisoner_capture;
    game::GovernorPrisonerDisposal governor_prisoner;
    game::StratagemGauge stratagem_gauge;
    game::IsolatedTerritory isolated_territory;
    game::ReinforcementPlacement reinforcement_placement;
    game::CouncilContinue council_continue;
    game::MissionCpuExclusion mission_cpu_exclusion;
    game::CouncilExecuteFree council_execute_free;
    game::AffinityDisplay affinity_display;
    domain::NativeFeatureStatuses statuses{};
    std::array<bool, domain::NATIVE_FEATURE_GROUP_COUNT> pending{};
    std::array<bool, domain::NATIVE_FEATURE_GROUP_COUNT> announce{};
    std::array<std::uint64_t, domain::NATIVE_FEATURE_GROUP_COUNT> retry_after_ms{};
    std::optional<NativeFeatureNotice> notice;
    std::size_t next_group = 0;
    bool initialized = false;
    bool council_reset_pending = false;
    std::uint64_t council_reset_after_ms = 0;
};

bool request_native_feature(Runtime& runtime, domain::FeatureId feature, bool enabled);
// At most one patch transaction per admitted commit boundary. Busy groups retain
// the desired setting and yield to other groups until their next retry deadline.
bool reconcile_native_features(Runtime& runtime, std::uintptr_t module_base, std::uint64_t now_ms);
void change_native_feature_epoch(Runtime& runtime);
void update_native_feature_candidates(Runtime& runtime, std::uintptr_t module_base);
// Restore at most one active group per call. QueueFull means another boundary is
// needed; desired configuration is preserved for the final settings snapshot.
domain::Result<void> stop_native_features(Runtime& runtime, std::uintptr_t module_base);

} // namespace s8::runtime
