#pragma once

#include "../domain/legacy_features.h"
#include "../domain/secondary_jewel.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace s8::runtime {

struct Runtime;

struct LegacyFeatureNotice {
    domain::LegacyFeatureGroup group;
    bool succeeded = false;
};

struct LegacyFeatureControl {
    domain::LegacyFeatureStatuses statuses{};
    std::array<bool, domain::LEGACY_FEATURE_GROUP_COUNT> pending{};
    std::array<bool, domain::LEGACY_FEATURE_GROUP_COUNT> manual{};
    std::array<bool, domain::LEGACY_FEATURE_GROUP_COUNT> announce{};
    std::array<std::uint64_t, domain::LEGACY_FEATURE_GROUP_COUNT> retry_after_ms{};
    std::optional<domain::SecondaryJewelMask> applied_secondary_jewels;
    bool applied_speed_enabled = false;
    std::optional<LegacyFeatureNotice> notice;
    std::size_t next_group = 0;
    std::uint8_t next_controller = 0;
};

// Desired values live in Runtime, so later UI requests replace queued config
// requests without keeping an old payload that could be applied afterwards.
bool request_legacy_feature(Runtime& runtime, domain::FeatureId feature, bool enabled);
void request_legacy_group(Runtime& runtime, domain::LegacyFeatureGroup group, bool manual);
void queue_legacy_configuration(Runtime& runtime);
void change_legacy_feature_epoch(Runtime& runtime);
void observe_legacy_player(Runtime& runtime, std::uint64_t now_ms);
// At most one group (or one city-conversion phase) per admitted boundary.
// Each failed group has its own deadline and yields to all other ready groups.
bool reconcile_legacy_features(Runtime& runtime, std::uintptr_t module_base, std::uint64_t now_ms);

} // namespace s8::runtime
