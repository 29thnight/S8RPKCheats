#pragma once

#include "../game/stratagem5_binding.h"
#include "../game/stratagem5_count.h"
#include "../game/stratagem5_camp.h"
#include "../game/stratagem5_model_binding.h"
#include "../game/short_battle_cooldown.h"
#include "../game/recurring_features.h"
#include "../game/troop_count_combat.h"
#include "../game/reinforcement_arrival.h"
#include "../game/infinite_gift.h"
#include "../game/marriage_condition.h"
#include "../game/instant_attitude.h"
#include "../game/tech_city_visibility.h"
#include "../game/infinite_talk.h"
#include "../game/infinite_duel_debate.h"
#include "../game/city_conversion.h"
#include "../game/city_management.h"
#include "../game/corps_deployment.h"
#include "../game/fast_relationship.h"
#include "../game/tech_zero.h"
#include "../game/self_heal.h"
#include "../game/dongto.h"
#include "../game/celestial.h"
#include "../game/catapult.h"
#include "../game/terrain_tactics.h"
#include "../game/def_building.h"
#include "../game/road_block.h"
#include "../game/skill_condition.h"
#include "../game/battle_date.h"
#include "../game/battle_weather.h"
#include "../game/terrain_bonus.h"
#include "../game/officer_stats.h"
#include "../game/all_aggressive.h"
#include "../game/zero_infamy.h"
#include "../game/siege.h"
#include "../game/tavern_requests.h"
#include "../game/specialty_distribution.h"
#include "../game/battle_map_shuffle.h"
#include "../game/faction_lord_bonus.h"
#include "../game/undiscovered_ronin.h"
#include "../game/jewel_open.h"
#include "../game/secondary_jewel_hook.h"
#include "../game/instant_loyalty.h"
#include "../game/love_hate.h"
#include "../game/instant_resonance.h"
#include "../game/domestics.h"
#include "../game/mission_appointment_limit.h"
#include "../game/domestic_reward.h"
#include "../game/ship_weaponization.h"
#include "../game/battle_skill_counts.h"
#include "../game/officer_roster.h"
#include "../game/auto_affinity.h"
#include "../game/battle_observation.h"
#include "../game/battle_state.h"
#include "../game/start_setting.h"
#include "../game/tengi.h"
#include "../game/ai_officer_growth.h"
#include "../game/special_ability_assignment.h"
#include "../game/officer_tools.h"
#include "../game/officer_notifications.h"
#include "stratagem5_lifecycle.h"
#include "native_feature_control.h"
#include "legacy_feature_control.h"
#include "trait_control.h"
#include "../domain/feature_flags.h"
#include "../domain/config_snapshot.h"
#include "../domain/work_budget.h"

#include <cstdint>
#include <string_view>
#include <string>
#include <span>
#include <vector>
#include <cstddef>

namespace s8::services {
struct ConfigLoad;
}

namespace s8::runtime {

struct PendingTraitChange {
    game::TraitObjectSearch search;
    std::uintptr_t game_base;
    std::uint16_t officer_id;
    std::uint8_t slot;
    std::uintptr_t target;
    bool protagonist;
};

struct Runtime {
    bool stopping = false;
    bool stopped = false;
    bool stopped_state_published = false;
    std::uint64_t completed_stop_steps = 0;
    std::uint64_t failed_stop_steps = 0;
    std::size_t next_stop_step = 0;
    std::size_t next_game_job = 0;
    std::size_t next_ui_job = 0;
    std::optional<domain::ConfigSnapshot> stop_snapshot;
    NativeFeatureControl native_features;
    LegacyFeatureControl legacy_features;
    TraitControl traits;
    game::AiGrowthState ai_growth;
    domain::AiGrowthSettings ai_growth_settings;
    std::optional<domain::AiGrowthReport> ai_growth_last_report;
    std::optional<domain::AiGrowthPreview> ai_growth_preview;
    std::uint64_t ai_growth_preview_ms = 0;
    std::uint16_t pending_annual_assignment = 0;
    game::SpecialAbilityAssignment special_assignment;
    own::unique_owner<game::OfficerTools> officer_tools = own::make_unique<game::OfficerTools>();
    own::unique_owner<game::OfficerNotifications> officer_notices = own::make_unique<game::OfficerNotifications>();
    domain::OfficerNoticesSnapshot officer_notices_snapshot;
    bool officer_tools_publish_pending = true;
    bool officer_notices_publish_pending = true;
    game::CityManagement city_management;
    std::uint64_t city_published_revision = ~std::uint64_t{0};
    own::unique_owner<game::CorpsDeployment> corps_deployment = own::make_unique<game::CorpsDeployment>();
    std::uint64_t corps_published_revision = ~std::uint64_t{0};
    own::shared_owner<const std::vector<domain::SpecialAssignmentResult>> special_assignment_results;
    own::shared_owner<const std::vector<domain::SpecialAssignmentResult>> special_assignment_notices;
    std::size_t special_assignment_notice_cursor = 0;
    std::uint64_t special_assignment_generation = 0;
    game::StartSettingState start_setting;
    game::TengiState tengi;
    domain::TengiSettings tengi_settings;
    float speed_multiplier = 1.0f;
    float ui_scale = 1.0f;
    std::optional<domain::PlayerResources> player_resources;
    std::uint64_t player_resources_next_ms = 0;
    bool speed_clock_configured = false;
    int speed_clock_error = 0;
    std::vector<std::byte> module_image;
    std::uintptr_t image_module = 0;
    domain::GameEpoch epoch{0};
    domain::FeatureFlags features;
    std::array<game::CityConversion, 4> city_conversions{};
    std::array<bool, 4> city_config_applied{};
    std::uintptr_t game_base_pointer = 0;
    game::Stratagem5Binding stratagem5;
    game::Stratagem5Count stratagem5_count;
    game::Stratagem5Camp stratagem5_camp;
    game::Stratagem5ModelBinding stratagem5_model;
    std::uintptr_t stratagem5_attack_owner = 0;
    std::uintptr_t stratagem5_defense_owner = 0;
    game::ShortBattleCooldown short_battle_cooldown;
    game::RecurringFeatures recurring_features;
    game::TroopCountCombat troop_count_combat;
    game::ReinforcementArrival reinforcement_arrival;
    game::InfiniteGift infinite_gift;
    game::InfiniteTalk infinite_talk;
    game::InfiniteDuelDebate infinite_duel_debate;
    bool infinite_duel_enabled = false;
    bool infinite_debate_enabled = false;
    bool infinite_duel_config_applied = false;
    bool infinite_debate_config_applied = false;
    bool infinite_talk_enabled = false;
    bool infinite_talk_config_applied = false;
    game::MarriageCondition marriage_condition;
    bool marriage_enabled = false;
    game::InstantAttitude instant_attitude;
    game::FastRelationship fast_relationship;
    game::TechZero tech_zero;
    domain::SelfHealSettings self_heal_settings;
    domain::DongtoSettings dongto_settings;
    domain::CelestialSettings celestial_settings;
    domain::CatapultSettings catapult_settings;
    domain::TerrainTacticsSettings terrain_tactics_settings;
    domain::DefBuildingSettings def_building_settings;
    std::array<game::RoadBlock, 2> road_blocks{};
    std::array<bool, 3> skill_condition_applied{};
    game::BattleDate battle_date;
    std::optional<int> pending_battle_days_remaining;
    game::BattleWeather battle_weather;
    game::ShipWeaponization ship_weaponization;
    game::BattleSkillCounts battle_skill_counts;
    std::optional<domain::SkillCounts> skill_counts;
    game::TerrainBonusState terrain_bonus_state;
    game::OfficerStatsMonitor officer_stats;
    game::OfficerRosterScan officer_roster;
    std::optional<PendingTraitChange> pending_trait_change;
    game::AutoAffinity auto_affinity;
    std::optional<domain::OfficerDetail> selected_officer;
    std::optional<domain::OfficerTraits> protagonist_traits;
    game::AllAggressive all_aggressive;
    game::ZeroInfamy zero_infamy;
    std::array<game::Siege, 2> siege{};
    game::TavernRequests tavern_requests;
    std::optional<domain::NamedSpecialties> named_specialties;
    std::mt19937 specialty_random;
    bool specialty_last_council = false;
    bool specialty_assignment_requested = false;
    game::BattleMapShuffle battle_map_shuffle;
    game::FactionLordBonus faction_lord_bonus;
    game::UndiscoveredRonin undiscovered_ronin;
    game::JewelOpen jewel_open;
    domain::DomesticsMultipliers domestics;
    game::Domestics domestics_patch;
    game::MissionAppointmentLimit mission_appointment_limit;
    game::DomesticReward domestic_reward;
    std::optional<std::uint16_t> protagonist_id;
    std::optional<std::array<std::uint8_t, 35>> protagonist_tactic_levels;
    std::optional<std::array<std::uint8_t, 24>> protagonist_skill_levels;
    std::optional<std::array<std::uint8_t, 11>> protagonist_aptitudes;
    std::optional<domain::OfficerBasicValues> protagonist_basic;
    std::optional<std::uint8_t> protagonist_gender;
    std::optional<std::int32_t> mission_appointment_limit_value;
    float notification_speed = 100.0f;
    game::InstantLoyalty instant_loyalty;
    game::LoveHate love_hate;
    game::InstantResonance instant_resonance;
    std::optional<domain::SecondaryJewelMask> secondary_jewels;
    std::uint8_t last_relevant_phase = 0;
    int siege_heal_rate = 10;
    domain::TerrainBonus terrain_bonus;
    int skip_days = 0;
    bool battle_mods_refreshed = false;
    std::uint8_t battle_refresh_done = 0;
    std::size_t next_battle_refresh = 0;
    std::size_t next_battle_job = 0;
    bool stratagem_refresh_next = true;
    game::BattleReadiness battle_readiness;
    game::BattleState battle_state;
    bool battle_seen = false;
    bool fast_relationship_enabled = false;
    bool fast_relationship_config_applied = false;
    game::TechCityVisibility tech_city_visibility;
    bool tech_city_visibility_enabled = false;
    bool instant_attitude_enabled = false;
    bool instant_attitude_config_applied = false;
    bool marriage_config_applied = false;
    bool infinite_gift_enabled = false;
    bool infinite_gift_config_applied = false;
    std::uint64_t config_first_game_base_ms = 0;
    std::uint64_t config_first_player_ms = 0;
    bool reinforcement_arrival_enabled = false;
    bool troop_count_combat_enabled = false;
    bool short_battle_cooldown_enabled = false;
    int short_battle_cooldown_days = 3;
    std::uintptr_t cached_game_base = 0;
    std::uint64_t game_base_probe_ms = 0;
    std::uintptr_t loop_game_base = 0;
    std::uintptr_t loop_player = 0;
    std::uint64_t loop_probe_ms = 0;
    bool loop_game_base_ready = false;
    bool loop_player_ready = false;
    bool loop_context_observed = false;
    std::uintptr_t session_player = 0;
    std::uintptr_t session_game_base = 0;
    bool config_loaded = false;
    std::optional<std::string> config_baseline;
    bool config_baseline_published = false;
    Stratagem5Lifecycle stratagem5_lifecycle;
};

void change_game_session(Runtime& runtime);
domain::Result<void> stop_game(Runtime& runtime, std::uintptr_t game_module);
domain::ConfigSnapshot config_snapshot(const Runtime& runtime);
bool initialize_game_base(Runtime& runtime, std::span<const std::byte> image, std::uintptr_t module_base);
bool initialize_game_base_from_module(Runtime& runtime, std::uintptr_t module_base);
void request_stratagem5(Runtime& runtime, bool enabled);
bool enable_stratagem5_data(Runtime& runtime, std::uintptr_t module_base);
bool disable_stratagem5_data(Runtime& runtime);
game::Stratagem5SettingsResult set_stratagem5_settings(Runtime& runtime, domain::Stratagem5Settings settings);
void load_stratagem5_config(Runtime& runtime, std::string_view text);
struct Stratagem5UiActions {
    void* context;
    bool (*prepare_bridge)(void* context);
    bool (*set_metadata)(void* context, bool enabled);
    bool (*ensure_count_capture)(void* context);
};
bool on_stratagem5_lifecycle(Runtime& runtime, std::uintptr_t module_base, bool battle_active,
                             bool safe_non_battle, const Stratagem5UiActions& ui);
void on_battle_end(Runtime& runtime);
void on_battle_ready(Runtime& runtime, std::uintptr_t module_base, int day, int unit_count,
    const domain::WorkBudget& work_budget = {});
void on_active_battle_observed(Runtime& runtime, std::uintptr_t module_base, std::uintptr_t game_base,
                               const game::BattleObservation& observation, std::uint64_t now_ms,
    const domain::WorkBudget& work_budget = {});
void monitor_battle(Runtime& runtime, std::uintptr_t module_base, std::uintptr_t game_base,
                    std::uint64_t now_ms, const Stratagem5UiActions& ui,
    const domain::WorkBudget& work_budget = {});
void on_battle_update(Runtime& runtime, std::uintptr_t module_base, int unit_count,
                      std::uintptr_t unit_list, bool skip_stratagem5, const domain::WorkBudget& work_budget = {});
bool on_stratagem_metadata(Runtime& runtime, std::uintptr_t metadata_table);
bool on_stratagem5_context(Runtime& runtime, std::uintptr_t metadata_table, std::uintptr_t dialog,
                            std::uintptr_t attack_owner, std::uintptr_t defense_owner);
std::optional<std::uintptr_t> find_stratagem5_metadata(std::span<const std::byte> image,
                                                        std::uintptr_t game_base);
bool update_stratagem5_count(Runtime& runtime, std::uintptr_t metadata_table,
    std::uintptr_t dialog, std::uintptr_t attack_owner, std::uintptr_t defense_owner);
bool set_stratagem5_count_request(Runtime& runtime, bool enabled,
                                  std::uintptr_t attack_owner, std::uintptr_t defense_owner);
bool attach_stratagem5_camp(Runtime& runtime, std::uintptr_t metadata_table);
bool restore_stratagem5_camp(Runtime& runtime);
bool extend_stratagem5_model(Runtime& runtime, std::uintptr_t dialog, std::uintptr_t metadata_table);
bool restore_stratagem5_model(Runtime& runtime);
bool set_short_battle_cooldown(Runtime& runtime, std::uintptr_t module_base, bool enabled, std::int32_t days);
void load_short_battle_cooldown_config(Runtime& runtime, std::string_view text, std::uintptr_t module_base);
bool set_troop_count_combat(Runtime& runtime, std::uintptr_t module_base, bool enabled);
bool set_reinforcement_arrival(Runtime& runtime, std::uintptr_t module_base, bool enabled);
bool set_tech_city_visibility(Runtime& runtime, std::uintptr_t module_base, bool enabled);
bool set_infinite_gift(Runtime& runtime, std::uintptr_t module_base,
                       std::span<const std::byte> image, bool enabled);
void apply_config(Runtime& runtime, services::ConfigLoad config, std::uintptr_t game_module);
void tick_recurring_features(Runtime& runtime, std::uintptr_t module_base,
                             std::uintptr_t game_base, bool game_base_ready,
                             std::uintptr_t player, bool player_ready);
void observe_game_context(Runtime& runtime, std::uintptr_t game_module, std::uint64_t now_ms,
    const domain::WorkBudget& work_budget = {});
void tick_game(Runtime& runtime, std::uintptr_t game_module, std::uint64_t now_ms,
    const domain::WorkBudget& work_budget = {});

} // namespace s8::runtime
