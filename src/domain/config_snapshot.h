#pragma once

#include "feature_flags.h"
#include "stratagem5.h"
#include "self_heal.h"
#include "dongto.h"
#include "celestial.h"
#include "catapult.h"
#include "terrain_tactics.h"
#include "def_building.h"
#include "terrain_bonus.h"
#include "secondary_jewel.h"
#include "domestics.h"
#include "domestic_reward.h"
#include "officer_basic.h"
#include "officer_roster.h"
#include "tengi.h"
#include "ai_officer_growth.h"
#include "special_ability_assignment.h"
#include "native_features.h"
#include "legacy_features.h"
#include "trait_control.h"
#include "city_management.h"
#include "ownership.h"
#include "player_resources.h"

#include <cstdint>
#include <optional>

namespace s8::domain {

enum class GameMemoryStatus {
    Searching, SearchWaiting, ModuleReadFailed, RootReadFailed,
    WaitingForGame, PlayerReadFailed, WaitingForPlayer, Ready
};

struct ConfigSnapshot {
    FeatureFlags features;
    bool short_battle_cooldown_enabled = false;
    std::int32_t short_battle_cooldown_days = 3;
    bool troop_count_combat_enabled = false;
    bool reinforcement_arrival_enabled = false;
    bool tech_city_visibility_enabled = false;
    bool stratagem5_enabled = false;
    Stratagem5Settings stratagem5_settings;
    SelfHealSettings self_heal_settings;
    DongtoSettings dongto_settings;
    CelestialSettings celestial_settings;
    CatapultSettings catapult_settings;
    TerrainTacticsSettings terrain_tactics_settings;
    DefBuildingSettings def_building_settings;
    int skip_days = 0;
    TerrainBonus terrain_bonus;
    int siege_heal_rate = 10;
    bool all_jewels_open = false;
    std::optional<SecondaryJewelMask> secondary_jewels;
    std::optional<std::array<std::uint8_t, 34>> jewel_bitmap;
    DomesticsMultipliers domestics;
    std::optional<std::int32_t> mission_appointment_limit;
    bool mission_appointment_limit_managed = false;
    bool config_loaded = false;
    float notification_speed = 100.0f;
    DomesticRewardMode domestic_reward_mode = DomesticRewardMode::Off;
    std::optional<std::uint16_t> protagonist_id;
    std::optional<std::array<std::uint8_t, 35>> protagonist_tactic_levels;
    std::optional<std::array<std::uint8_t, 24>> protagonist_skill_levels;
    std::optional<std::array<std::uint8_t, 11>> protagonist_aptitudes;
    std::optional<OfficerBasicValues> protagonist_basic;
    std::optional<std::uint8_t> protagonist_gender;
    std::optional<OfficerDetail> selected_officer;
    std::optional<OfficerTraits> protagonist_traits;
    float speed_multiplier = 1.0f;
    TengiSettings tengi_settings;
    bool tengi_layout_supported = false;
    bool tengi_ready = false;
    std::uint8_t tengi_momentum = 0;
    int tengi_error = 0;
    AiGrowthSettings ai_growth_settings;
    bool ai_growth_running = false;
    std::size_t ai_growth_cursor = 0;
    bool ai_growth_setting_failed = false;
    std::optional<AiGrowthReport> ai_growth_report;
    std::optional<AiGrowthPreview> ai_growth_preview;
    SpecialAssignmentSummary special_assignment;
    own::shared_owner<const std::vector<SpecialAssignmentResult>> special_assignment_results;
    std::uint64_t special_assignment_generation = 0;
    NativeFeatureStatuses native_features{};
    TraitControlView traits;
    CitySettings city_settings;
    float ui_scale = 1.0f;
    std::optional<PlayerResources> player_resources;
    LegacyFeatureStatuses legacy_features{};
    OfficerFactionColor selected_faction_color;
    OfficerFactionColor protagonist_faction_color;
    own::shared_owner<const OfficerRelations> selected_officer_relations;
    own::shared_owner<const OfficerRelations> protagonist_relations;
    GameMemoryStatus game_memory_status = GameMemoryStatus::Searching;
    std::size_t game_memory_scan_cursor = 0;
    std::size_t game_memory_image_size = 0;
    std::uint64_t game_memory_scan_passes = 0;
};

} // namespace s8::domain
