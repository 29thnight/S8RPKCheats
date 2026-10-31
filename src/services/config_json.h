#pragma once

#include "config_changes.h"
#include "../domain/error.h"
#include "../domain/secondary_jewel.h"
#include "../domain/def_building.h"
#include "../domain/officer_roster.h"
#include "../domain/tengi.h"

#include <span>
#include <array>
#include <optional>
#include <string>
#include <string_view>

namespace s8::services {

// Preserves the original bytes of fields outside changes. The caller owns I/O.
domain::Result<std::string> update_config_json(std::string_view source, std::span<const ConfigChange> changes);
struct SecondaryJewelConfig {
    domain::SecondaryJewelMask mask;
    bool mask_present = false;
    bool valid = true;
};

using TerrainBonusChanges = std::array<std::array<std::optional<std::int16_t>, 18>, 13>;
using DefBuildingChanges = std::array<std::optional<int>, domain::DEF_BUILDING_KEYS.size()>;
struct ScalarConfigChanges {
    std::optional<bool> ai_growth_enabled;
    std::optional<int> ai_growth_speed;
    std::optional<bool> ai_growth_restore_none;
    std::optional<bool> annual_special_abilities;
    std::optional<int> city_grain_limit;
    std::optional<int> city_retained_grain;
    std::optional<int> city_exchange_rate;
    std::optional<bool> city_auto_exchange;
    std::optional<bool> city_revolt_zero;
    std::optional<float> speed_multiplier;
    std::array<std::optional<bool>, domain::TENGI_EVENT_COUNT> tengi_allowed;
    std::array<std::optional<int>, domain::TENGI_EVENT_COUNT> tengi_duration;
    std::array<std::optional<int>, domain::TENGI_EVENT_COUNT> tengi_cooldown;
    std::optional<float> notification_speed;
    std::optional<float> ui_scale;
    std::optional<float> domestics_player;
    std::optional<float> domestics_force;
    std::optional<bool> all_jewels_open;
    std::optional<int> siege_heal_rate;
    std::optional<int> skip_days;
    std::array<std::optional<int>, 3> heal_amount;
    std::array<std::optional<int>, 2> heal_range;
    std::array<std::optional<int>, 3> dongto_probability;
    std::array<std::optional<int>, 3> dongto_status_probability;
    std::array<std::optional<int>, 3> celestial_probability;
    std::array<std::optional<int>, 3> celestial_amount;
    std::array<std::optional<int>, 3> celestial_range;
    std::array<std::optional<int>, 3> water_amount;
    std::array<std::optional<int>, 3> water_range;
    std::array<std::optional<int>, 3> stone_amount;
    std::array<std::optional<int>, 3> stone_range;
    std::optional<int> catapult_min_range;
    std::optional<int> catapult_max_range;
    std::optional<int> catapult_amount;
    std::optional<int> catapult_range;
    std::optional<int> short_battle_cooldown_days;
    std::optional<bool> short_battle_cooldown_enabled;
    std::optional<bool> tech_city_visibility;
    std::optional<bool> reinforcement_arrival;
    std::optional<bool> troop_count_combat;
    bool stratagem5_enabled = false;
    std::array<std::optional<int>, 9> stratagem5;
};

domain::Result<std::string> config_lines_for_load(std::string_view source,
                                                  SecondaryJewelConfig* secondary = nullptr,
                                                  TerrainBonusChanges* terrain_bonus = nullptr,
                                                  DefBuildingChanges* def_building = nullptr,
                                                  ScalarConfigChanges* scalars = nullptr);
domain::Result<SecondaryJewelConfig> read_secondary_jewel_config(std::string_view source);
domain::Result<domain::OfficerNames> read_officer_names(std::string_view source);
domain::Result<std::string> update_officer_name(std::string_view source, int id, std::string_view name);

} // namespace s8::services
