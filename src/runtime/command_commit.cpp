#include "command_commit.h"
#include "officer_tools_control.h"
#include "city_control.h"
#include "corps_control.h"
#include "../platform/windows/process_memory.h"
#include "../platform/windows/speed_clock.h"
#include "../game/game_base.h"
#include "../game/player_resources.h"

#include <type_traits>
#include <algorithm>
#include <utility>
#include <variant>
#include <cmath>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

namespace s8::runtime {

std::size_t commit_commands(Runtime& runtime, services::CommandMailbox& mailbox,
                            std::uintptr_t game_module, std::size_t max_commands,
                            std::chrono::steady_clock::duration time_budget,
                            services::UiBridge* ui) {
    const platform::windows::RealClockScope real_clock;

    if (runtime.stopping) {
        return 0;
    }

    const auto deadline = std::chrono::steady_clock::now() + time_budget;
    std::size_t processed = 0;
    std::array<bool, 5103> ronin_changes{};
    bool ronin_changed = false;
    std::array<services::CompletionToken, services::CommandMailbox::CAPACITY> ronin_completions{};
    std::size_t ronin_completion_count = 0;
    while (processed < max_commands && std::chrono::steady_clock::now() < deadline) {
        auto accepted = mailbox.pop();
        if (!accepted) {
            break;
        }
        domain::Result<void> result;
        bool settings_changed_on_failure = false;
        bool save_config = true;
        if (accepted->command.epoch != runtime.epoch) {
            result = std::unexpected(domain::Error{domain::ErrorCode::StaleEpoch});
        } else {
            result = std::visit([&](const auto& command) -> domain::Result<void> {
                using T = std::decay_t<decltype(command)>;
                if constexpr (std::is_same_v<T, domain::SetFeature>) {
                    if (request_native_feature(runtime, command.feature, command.enabled)) {
                        return {};
                    }
                    if (request_legacy_feature(runtime, command.feature, command.enabled)) {
                        return {};
                    }

                    switch (command.feature) {
                    case domain::FeatureId::InfiniteAp:
                        runtime.recurring_features.infinite_action_points = command.enabled;
                        break;
                    case domain::FeatureId::FastJewel:
                        runtime.recurring_features.infinite_jewel_changes = command.enabled;
                        break;
                    case domain::FeatureId::InfiniteBanquet:
                        runtime.recurring_features.infinite_banquet = command.enabled;
                        break;
                    case domain::FeatureId::InfiniteMediation:
                        runtime.recurring_features.infinite_mediation = command.enabled;
                        break;
                    case domain::FeatureId::DateAlways15:
                        if (command.enabled) {
                            runtime.features.set(domain::FeatureId::DateDynamic, false);
                        }
                        break;
                    case domain::FeatureId::DateDynamic:
                        if (command.enabled) {
                            runtime.features.set(domain::FeatureId::DateAlways15, false);
                        }
                        break;
                    case domain::FeatureId::SkipDaysEnabled:
                    case domain::FeatureId::AutoStatUp99:
                    case domain::FeatureId::AllAggressive:
                    case domain::FeatureId::ZeroInfamy:
                    case domain::FeatureId::InfiniteTavernRequests:
                    case domain::FeatureId::AutoFillSpecialties:
                    case domain::FeatureId::AutoLoadMenu:
                    case domain::FeatureId::ShowWidgetNotif:
                    case domain::FeatureId::ShowWidgetHero:
                    case domain::FeatureId::ShowWidgetAllOfficers:
                    case domain::FeatureId::FileLog:
                    case domain::FeatureId::MonitorRonin:
                    case domain::FeatureId::OfficerChangeNotify:
                        break;
                    case domain::FeatureId::TraitViewer:
                        request_trait_viewer(runtime.traits, command.enabled);
                        break;
                    case domain::FeatureId::AutoAffinityGrowth:
                        runtime.auto_affinity = {};
                        break;
                    case domain::FeatureId::UndiscoveredToRonin:
                        if (!command.enabled) {
                            runtime.undiscovered_ronin = {};
                        }
                        break;
                    case domain::FeatureId::BattleMapShuffle:
                        if (!command.enabled) {
                            game::disable_battle_map_shuffle(runtime.battle_map_shuffle);
                        }
                        break;
                    case domain::FeatureId::SiegeWarfare:
                    case domain::FeatureId::SiegeWarfare2: {
                        const bool wide = command.feature == domain::FeatureId::SiegeWarfare2;
                        if (command.enabled) {
                            game::disable_siege(runtime.siege[!wide], game_module);
                            runtime.features.set(wide ? domain::FeatureId::SiegeWarfare : domain::FeatureId::SiegeWarfare2, false);
                        } else {
                            game::disable_siege(runtime.siege[wide], game_module);
                        }
                        break;
                    }
                    case domain::FeatureId::WeatherSkillSimple:
                        if (command.enabled) {
                            runtime.features.set(domain::FeatureId::WeatherSkillComplex, false);
                        }
                        break;
                    case domain::FeatureId::WeatherSkillComplex:
                        if (command.enabled) {
                            runtime.features.set(domain::FeatureId::WeatherSkillSimple, false);
                        }
                        break;
                    case domain::FeatureId::TerrainAbilityAtkDef:
                        if (command.enabled) {
                            runtime.features.set(domain::FeatureId::TerrainAbilityAll, false);
                        }
                        break;
                    case domain::FeatureId::TerrainAbilityAll:
                        if (command.enabled) {
                            runtime.features.set(domain::FeatureId::TerrainAbilityAtkDef, false);
                        }
                        break;
                    case domain::FeatureId::TechZero:
                        game::set_tech_zero(runtime.tech_zero, command.enabled);
                        break;
                    case domain::FeatureId::StartSetting:
                    case domain::FeatureId::InfTengi:
                        break;
                    case domain::FeatureId::SelfHeal:
                        if (!game::set_self_heal(game_module, command.enabled, runtime.self_heal_settings)) {
                            return std::unexpected(domain::Error{domain::ErrorCode::PlatformFailure});
                        }
                        break;
                    case domain::FeatureId::Dongto:
                        if (!game::set_dongto(game_module, command.enabled, runtime.dongto_settings)) {
                            return std::unexpected(domain::Error{domain::ErrorCode::PlatformFailure});
                        }
                        break;
                    case domain::FeatureId::Celestial:
                        if (!game::set_celestial(game_module, command.enabled, runtime.celestial_settings)) {
                            return std::unexpected(domain::Error{domain::ErrorCode::PlatformFailure});
                        }
                        break;
                    case domain::FeatureId::Catapult:
                        if (!game::set_catapult(game_module, command.enabled, runtime.catapult_settings)) {
                            return std::unexpected(domain::Error{domain::ErrorCode::PlatformFailure});
                        }
                        break;
                    case domain::FeatureId::TerrainIgnore:
                        if (!game::set_terrain_tactics(game_module, command.enabled, runtime.terrain_tactics_settings)) {
                            return std::unexpected(domain::Error{domain::ErrorCode::PlatformFailure});
                        }
                        break;
                    case domain::FeatureId::DefBuilding:
                        if (!game::set_def_building(game_module, command.enabled, runtime.def_building_settings)) {
                            return std::unexpected(domain::Error{domain::ErrorCode::PlatformFailure});
                        }
                        break;
                    default:
                        return std::unexpected(domain::Error{domain::ErrorCode::InvalidData});
                    }
                    runtime.features.set(command.feature, command.enabled);
                    if (command.feature == domain::FeatureId::TerrainIgnore ||
                        command.feature == domain::FeatureId::WeatherSkillSimple ||
                        command.feature == domain::FeatureId::WeatherSkillComplex) {
                        game::update_battle_weather(runtime.battle_weather, game_module, GetTickCount64(),
                            runtime.features.get(domain::FeatureId::WeatherSkillSimple),
                            runtime.features.get(domain::FeatureId::WeatherSkillComplex), true,
                            runtime.features.get(domain::FeatureId::TerrainIgnore));
                    }
                    return {};
                } else if constexpr (std::is_same_v<T, domain::SetSpeedMultiplier>) {
                    runtime.speed_multiplier = domain::normalize_speed_multiplier(command.multiplier);
                    request_legacy_group(runtime, domain::LegacyFeatureGroup::SpeedClock, true);
                    return {};
                } else if constexpr (std::is_same_v<T, domain::SetTengiSettings>) {
                    runtime.tengi_settings = domain::normalize_tengi_settings(command.settings);
                    runtime.tengi_settings.force_quarterly = runtime.features.get(domain::FeatureId::InfTengi);
                    return {};
                } else if constexpr (std::is_same_v<T, domain::SetAiGrowthEnabled>) {
                    if (!game::set_ai_growth_enabled(game_module, runtime.ai_growth_settings, command.enabled)) {
                        return std::unexpected(domain::Error{domain::ErrorCode::PlatformFailure});
                    }

                    runtime.ai_growth.applied_game_speed = command.enabled ? runtime.ai_growth_settings.speed : 0;
                    runtime.ai_growth.game_setting_failed = false;

                    if (!command.enabled) {
                        runtime.pending_annual_assignment = 0;

                        if (runtime.special_assignment.summary.annual) {
                            game::cancel_special_ability_assignment(runtime.special_assignment);
                        }
                    }

                    return {};
                } else if constexpr (std::is_same_v<T, domain::SetAiGrowthSpeed>) {
                    if (!runtime.ai_growth_settings.enabled || command.speed < 1 || command.speed > 3) {
                        return std::unexpected(domain::Error{domain::ErrorCode::InvalidData});
                    }

                    if (!game::set_ai_growth_speed(game_module, command.speed)) {
                        return std::unexpected(domain::Error{domain::ErrorCode::PlatformFailure});
                    }

                    runtime.ai_growth_settings.speed = command.speed;
                    runtime.ai_growth.applied_game_speed = command.speed;
                    runtime.ai_growth.game_setting_failed = false;
                    return {};
                } else if constexpr (std::is_same_v<T, domain::SetAnnualSpecialAbilities>) {
                    if (command.enabled && !runtime.ai_growth_settings.enabled) {
                        return std::unexpected(domain::Error{domain::ErrorCode::InvalidData});
                    }

                    runtime.ai_growth_settings.annual_special_abilities = command.enabled;

                    if (!command.enabled) {
                        runtime.pending_annual_assignment = 0;

                        if (runtime.special_assignment.summary.annual) {
                            game::cancel_special_ability_assignment(runtime.special_assignment);
                        }
                    }

                    return {};
                } else if constexpr (std::is_same_v<T, domain::StartSpecialAbilityAssignment>) {
                    save_config = false;

                    if (!ui || !runtime.skill_counts || !game::start_special_ability_assignment(
                            runtime.special_assignment, game_module, runtime.loop_game_base, runtime.loop_player,
                            runtime.epoch, ui->skill_counts_view(), false)) {
                        return std::unexpected(domain::Error{domain::ErrorCode::InvalidData});
                    }

                    runtime.special_assignment_results.reset();
                    return {};
                } else if constexpr (std::is_same_v<T, domain::CancelSpecialAbilityAssignment>) {
                    save_config = false;

                    if (!game::cancel_special_ability_assignment(runtime.special_assignment)) {
                        return std::unexpected(domain::Error{domain::ErrorCode::InvalidData});
                    }

                    return {};
                } else if constexpr (std::is_same_v<T, domain::SetShortBattleCooldown>) {
                    runtime.short_battle_cooldown_enabled = command.enabled;
                    runtime.short_battle_cooldown_days = command.days;
                    request_legacy_group(runtime, domain::LegacyFeatureGroup::ShortBattleCooldown, true);
                    return {};
                } else if constexpr (std::is_same_v<T, domain::SetTroopCountCombat>) {
                    runtime.troop_count_combat_enabled = command.enabled;
                    request_legacy_group(runtime, domain::LegacyFeatureGroup::TroopCountCombat, true);
                    return {};
                } else if constexpr (std::is_same_v<T, domain::SetReinforcementArrival>) {
                    runtime.reinforcement_arrival_enabled = command.enabled;
                    request_legacy_group(runtime, domain::LegacyFeatureGroup::ReinforcementArrival, true);
                    return {};
                } else if constexpr (std::is_same_v<T, domain::SetTechCityVisibility>) {
                    runtime.tech_city_visibility_enabled = command.enabled;
                    request_legacy_group(runtime, domain::LegacyFeatureGroup::TechCityVisibility, true);
                    return {};
                } else if constexpr (std::is_same_v<T, domain::SetStratagem5Enabled>) {
                    request_stratagem5(runtime, command.enabled);
                    return {};
                } else if constexpr (std::is_same_v<T, domain::SetStratagem5Settings>) {
                    const auto status = set_stratagem5_settings(runtime, command.settings);
                    if (status == game::Stratagem5SettingsResult::write_failed) {
                        settings_changed_on_failure = true;
                        return std::unexpected(domain::Error{domain::ErrorCode::PlatformFailure});
                    }
                    return {};
                } else if constexpr (std::is_same_v<T, domain::SetSelfHealSettings>) {
                    runtime.self_heal_settings = command.settings;
                    constexpr std::array IDS{domain::FeatureId::HealLv1Self, domain::FeatureId::HealLv2Self,
                                             domain::FeatureId::HealLv3Self};
                    for (std::size_t i = 0; i < IDS.size(); ++i) {
                        runtime.features.set(IDS[i], command.settings.self[i]);
                    }
                    if (!game::set_self_heal(game_module, runtime.features.get(domain::FeatureId::SelfHeal),
                                             runtime.self_heal_settings)) {
                        settings_changed_on_failure = true;
                        return std::unexpected(domain::Error{domain::ErrorCode::PlatformFailure});
                    }
                    return {};
                } else if constexpr (std::is_same_v<T, domain::SetDongtoSettings>) {
                    runtime.dongto_settings = command.settings;
                    if (!game::set_dongto(game_module, runtime.features.get(domain::FeatureId::Dongto),
                                          runtime.dongto_settings)) {
                        settings_changed_on_failure = true;
                        return std::unexpected(domain::Error{domain::ErrorCode::PlatformFailure});
                    }
                    return {};
                } else if constexpr (std::is_same_v<T, domain::SetCelestialSettings>) {
                    runtime.celestial_settings = command.settings;
                    if (!game::set_celestial(game_module, runtime.features.get(domain::FeatureId::Celestial),
                                             runtime.celestial_settings)) {
                        settings_changed_on_failure = true;
                        return std::unexpected(domain::Error{domain::ErrorCode::PlatformFailure});
                    }
                    return {};
                } else if constexpr (std::is_same_v<T, domain::SetCatapultSettings>) {
                    runtime.catapult_settings = command.settings;
                    if (!game::set_catapult(game_module, runtime.features.get(domain::FeatureId::Catapult),
                                            runtime.catapult_settings)) {
                        settings_changed_on_failure = true;
                        return std::unexpected(domain::Error{domain::ErrorCode::PlatformFailure});
                    }
                    return {};
                } else if constexpr (std::is_same_v<T, domain::SetTerrainTacticsSettings>) {
                    runtime.terrain_tactics_settings = command.settings;
                    if (!game::set_terrain_tactics(game_module, runtime.features.get(domain::FeatureId::TerrainIgnore),
                                                   runtime.terrain_tactics_settings)) {
                        settings_changed_on_failure = true;
                        return std::unexpected(domain::Error{domain::ErrorCode::PlatformFailure});
                    }
                    return {};
                } else if constexpr (std::is_same_v<T, domain::SetDefBuildingSettings>) {
                    runtime.def_building_settings = command.settings;
                    if (!game::set_def_building(game_module, runtime.features.get(domain::FeatureId::DefBuilding),
                                                 runtime.def_building_settings)) {
                        settings_changed_on_failure = true;
                        return std::unexpected(domain::Error{domain::ErrorCode::PlatformFailure});
                    }
                    return {};
                } else if constexpr (std::is_same_v<T, domain::SetSkipDays>) {
                    runtime.skip_days = command.days;
                    return {};
                } else if constexpr (std::is_same_v<T, domain::SetTerrainBonus>) {
                    runtime.terrain_bonus = command.table;
                    return {};
                } else if constexpr (std::is_same_v<T, domain::SetSiegeHealRate>) {
                    runtime.siege_heal_rate = command.rate;
                    return {};
                } else if constexpr (std::is_same_v<T, domain::SetAllJewelsOpen>) {
                    if (!game::set_all_jewels_open(runtime.jewel_open, runtime.cached_game_base, command.enabled)) {
                        return std::unexpected(domain::Error{domain::ErrorCode::PlatformFailure});
                    }
                    return {};
                } else if constexpr (std::is_same_v<T, domain::SetSecondaryJewels>) {
                    runtime.secondary_jewels = command.mask;
                    request_legacy_group(runtime, domain::LegacyFeatureGroup::SecondaryJewels, true);
                    return {};
                } else if constexpr (std::is_same_v<T, domain::SetDomesticsMultipliers>) {
                    runtime.domestics = command.multipliers;
                    runtime.domestics_patch.multipliers = command.multipliers;
                    if (runtime.features.get(domain::FeatureId::Domestics)) {
                        request_legacy_group(runtime, domain::LegacyFeatureGroup::Domestics, true);
                    }
                    return {};
                } else if constexpr (std::is_same_v<T, domain::SetMissionAppointmentLimit>) {
                    save_config = false;
                    const bool applied = game::set_mission_appointment_limit(runtime.mission_appointment_limit,
                                                                              game_module, command.value);
                    runtime.mission_appointment_limit_value = game::read_mission_appointment_limit(game_module);
                    if (!applied) {
                        return std::unexpected(domain::Error{domain::ErrorCode::PlatformFailure});
                    }
                    return {};
                } else if constexpr (std::is_same_v<T, domain::RestoreMissionAppointmentLimit>) {
                    save_config = false;
                    const bool restored = game::restore_mission_appointment_limit(runtime.mission_appointment_limit,
                                                                                   game_module);
                    runtime.mission_appointment_limit_value = game::read_mission_appointment_limit(game_module);
                    if (!restored) {
                        return std::unexpected(domain::Error{domain::ErrorCode::PlatformFailure});
                    }
                    return {};
                } else if constexpr (std::is_same_v<T, domain::SetNotificationSpeed>) {
                    runtime.notification_speed = command.speed;
                    return {};
                } else if constexpr (std::is_same_v<T, domain::SetUiScale>) {
                    if (!std::isfinite(command.scale) || command.scale < 0.5f || command.scale > 3.0f) {
                        return std::unexpected(domain::Error{domain::ErrorCode::InvalidData});
                    }
                    runtime.ui_scale = command.scale;
                    return {};
                } else if constexpr (std::is_same_v<T, domain::SetPlayerResource>) {
                    save_config = false;
                    const domain::Result<void> applied = game::write_player_resource(runtime.game_base_pointer,
                        runtime.loop_game_base, runtime.loop_player, command);

                    runtime.player_resources = game::read_player_resources(runtime.game_base_pointer,
                        runtime.loop_game_base, runtime.loop_player);
                    return applied;
                } else if constexpr (std::is_same_v<T, domain::SetDomesticRewardMode>) {
                    save_config = false;
                    if (!game::set_domestic_reward(runtime.domestic_reward, game_module, command.mode)) {
                        return std::unexpected(domain::Error{domain::ErrorCode::PlatformFailure});
                    }
                    return {};
                } else if constexpr (std::is_same_v<T, domain::SetSkillCount>) {
                    save_config = false;
                    if (!runtime.skill_counts) {
                        return std::unexpected(domain::Error{domain::ErrorCode::InvalidData});
                    }
                    auto& counts = *runtime.skill_counts;
                    bool changed = false;
                    if (command.count > 0) {
                        auto& skills = counts[command.officer_id];
                        const auto previous = skills.find(command.offset);
                        if (previous == skills.end() || previous->second != command.count) {
                            skills[command.offset] = command.count;
                            changed = true;
                        }
                    } else if (const auto officer = counts.find(command.officer_id); officer != counts.end()) {
                        changed = officer->second.erase(command.offset) != 0;
                        if (officer->second.empty()) {
                            counts.erase(officer);
                        }
                    }
                    if (ui && changed) {
                        ui->publish_skill_counts_view(counts);
                        ui->request_skill_counts_save(counts);
                    }
                    return {};
                } else if constexpr (std::is_same_v<T, domain::SetProtagonistTacticLevel>) {
                    save_config = false;
                    const auto player = game::read_player(game::read_game_base(runtime.game_base_pointer));
                    std::uint16_t officer_id = 0;
                    if (command.index >= 35 || command.level > 3 || !player ||
                        !platform::windows::read_memory(player + 8, std::as_writable_bytes(std::span(&officer_id, 1))) ||
                        officer_id != command.officer_id ||
                        !platform::windows::write_data_memory(player + 0x139 + command.index,
                            std::as_bytes(std::span(&command.level, 1)))) {
                        return std::unexpected(domain::Error{domain::ErrorCode::PlatformFailure});
                    }
                    if (runtime.protagonist_tactic_levels && runtime.protagonist_id == officer_id) {
                        (*runtime.protagonist_tactic_levels)[command.index] = command.level;
                    }
                    return {};
                } else if constexpr (std::is_same_v<T, domain::SetProtagonistTraining>) {
                    save_config = false;
                    const auto player = game::read_player(game::read_game_base(runtime.game_base_pointer));
                    std::uint16_t officer_id = 0;
                    const bool valid = command.aptitude ? command.index < 11 : command.index < 24 && command.value <= 3;
                    const auto offset = (command.aptitude ? 0xc4 : 0x1d0) + command.index;
                    if (!valid || !player ||
                        !platform::windows::read_memory(player + 8, std::as_writable_bytes(std::span(&officer_id, 1))) ||
                        officer_id != command.officer_id || !platform::windows::write_data_memory(player + offset,
                            std::as_bytes(std::span(&command.value, 1)))) {
                        return std::unexpected(domain::Error{domain::ErrorCode::PlatformFailure});
                    }
                    if (runtime.protagonist_id == officer_id) {
                        if (command.aptitude && runtime.protagonist_aptitudes) {
                            (*runtime.protagonist_aptitudes)[command.index] = command.value;
                        } else if (!command.aptitude && runtime.protagonist_skill_levels) {
                            (*runtime.protagonist_skill_levels)[command.index] = command.value;
                        }
                    }
                    return {};
                } else if constexpr (std::is_same_v<T, domain::SetProtagonistGender>) {
                    save_config = false;
                    const auto player = game::read_player(game::read_game_base(runtime.game_base_pointer));
                    std::uint16_t officer_id = 0;
                    if (command.gender > 1 || !player ||
                        !platform::windows::read_memory(player + 8, std::as_writable_bytes(std::span(&officer_id, 1))) ||
                        officer_id != command.officer_id ||
                        !platform::windows::write_data_memory(player + 0x30, std::as_bytes(std::span(&command.gender, 1)))) {
                        return std::unexpected(domain::Error{domain::ErrorCode::PlatformFailure});
                    }
                    runtime.protagonist_gender = command.gender;
                    if (game::write_roster_officer_gender(runtime.officer_roster, game_module, officer_id, command.gender) &&
                        runtime.selected_officer && runtime.selected_officer->id == officer_id) {
                        runtime.selected_officer->gender = command.gender;
                    }
                    return {};
                } else if constexpr (std::is_same_v<T, domain::SetProtagonistBasic>) {
                    save_config = false;
                    const auto player = game::read_player(game::read_game_base(runtime.game_base_pointer));
                    std::uint16_t officer_id = 0;
                    if (command.index >= domain::officer_basic_fields.size() || !player ||
                        !platform::windows::read_memory(player + 8, std::as_writable_bytes(std::span(&officer_id, 1))) ||
                        officer_id != command.officer_id) {
                        return std::unexpected(domain::Error{domain::ErrorCode::PlatformFailure});
                    }
                    const auto field = domain::officer_basic_fields[command.index];
                    if (!platform::windows::write_data_memory(player + field.offset,
                            std::as_bytes(std::span(&command.value, 1)).first(field.size))) {
                        return std::unexpected(domain::Error{domain::ErrorCode::PlatformFailure});
                    }
                    if (runtime.protagonist_basic && runtime.protagonist_id == officer_id) {
                        (*runtime.protagonist_basic)[command.index] = field.size == 1 ? command.value & 0xff
                            : field.size == 2 ? command.value & 0xffff : command.value;
                    }
                    return {};
                } else if constexpr (std::is_same_v<T, domain::RefreshOfficerRoster>) {
                    save_config = false;
                    runtime.officer_roster = {};
                    runtime.selected_officer.reset();
                    runtime.officer_roster.requested = true;
                    if (ui) {
                        ui->publish_officer_roster({runtime.epoch, {}, true});
                    }
                    return {};
                } else if constexpr (std::is_same_v<T, domain::SelectRosterOfficer>) {
                    save_config = false;
                    const auto detail = game::read_roster_officer(runtime.officer_roster, game_module, command.officer_id);
                    if (!detail) {
                        return std::unexpected(domain::Error{domain::ErrorCode::PlatformFailure});
                    }
                    runtime.selected_officer = *detail;
                    return {};
                } else if constexpr (std::is_same_v<T, domain::SetRosterOfficerGender>) {
                    save_config = false;
                    if (!game::write_roster_officer_gender(runtime.officer_roster, game_module,
                                                           command.officer_id, command.gender)) {
                        return std::unexpected(domain::Error{domain::ErrorCode::PlatformFailure});
                    }
                    if (runtime.selected_officer && runtime.selected_officer->id == command.officer_id) {
                        runtime.selected_officer->gender = command.gender;
                    }
                    return {};
                } else if constexpr (std::is_same_v<T, domain::SetRosterOfficerVoice>) {
                    save_config = false;
                    if (!game::write_roster_officer_voice(runtime.officer_roster, game_module,
                                                          command.officer_id, command.voice)) {
                        return std::unexpected(domain::Error{domain::ErrorCode::PlatformFailure});
                    }
                    if (runtime.selected_officer && runtime.selected_officer->id == command.officer_id) {
                        runtime.selected_officer->voice = command.voice;
                    }
                    return {};
                } else if constexpr (std::is_same_v<T, domain::ApplyTraitTextEdits>) {
                    save_config = false;
                    return request_trait_text_apply(runtime.traits, command);
                } else if constexpr (std::is_same_v<T, domain::OfficerToolsRequest>) {
                    save_config = false;
                    return submit_officer_tools_request(runtime, ui, game_module, command);
                } else if constexpr (std::is_same_v<T, domain::CityRequest>) {
                    save_config = false;
                    return submit_city_request(runtime, game_module, command);
                } else if constexpr (std::is_same_v<T, domain::CorpsRequest>) {
                    save_config = false;
                    return submit_corps_request(runtime, game_module, command);
                } else if constexpr (std::is_same_v<T, domain::SetCitySettings>) {
                    if (!game::set_city_settings(runtime.city_management, command.settings)) {
                        return std::unexpected(domain::Error{domain::ErrorCode::InvalidData});
                    }
                    return {};
                } else if constexpr (std::is_same_v<T, domain::SetCitySupportRoutes>) {
                    save_config = false;
                    if (!game::set_city_support_routes(runtime.city_management, command.routes)) {
                        return std::unexpected(domain::Error{domain::ErrorCode::InvalidData});
                    }
                    return {};
                } else if constexpr (std::is_same_v<T, domain::SetRosterOfficerRonin>) {
                    save_config = false;
                    if (!game::set_roster_officer_ronin(runtime.officer_roster, game_module,
                                                         command.officer_id, command.undiscovered_only)) {
                        return std::unexpected(domain::Error{domain::ErrorCode::PlatformFailure});
                    }
                    if (runtime.selected_officer && runtime.selected_officer->id == command.officer_id) {
                        runtime.selected_officer->status = 0x58;
                    }
                    ronin_changes[command.officer_id] = true;
                    game::update_ronin_notice_baseline(*runtime.officer_notices, command.officer_id, 0x58);
                    ronin_changed = true;
                    return {};
                } else if constexpr (std::is_same_v<T, domain::ReviveRosterOfficer>) {
                    save_config = false;
                    const auto player = game::read_player(game::read_game_base(runtime.game_base_pointer));
                    if (!game::revive_roster_officer(runtime.officer_roster, game_module, player,
                                                      command.officer_id, command.city)) {
                        return std::unexpected(domain::Error{domain::ErrorCode::PlatformFailure});
                    }
                    if (runtime.selected_officer && runtime.selected_officer->id == command.officer_id) {
                        runtime.selected_officer = game::read_roster_officer(runtime.officer_roster, game_module, command.officer_id);
                    }
                    ronin_changes[command.officer_id] = true;
                    game::update_ronin_notice_baseline(*runtime.officer_notices, command.officer_id, 0x58);
                    ronin_changed = true;
                    return {};
                } else if constexpr (std::is_same_v<T, domain::MoveRosterOfficer>) {
                    save_config = false;
                    if (!game::move_roster_officer(runtime.officer_roster, game_module, command.officer_id, command.city)) {
                        return std::unexpected(domain::Error{domain::ErrorCode::PlatformFailure});
                    }
                    if (runtime.selected_officer && runtime.selected_officer->id == command.officer_id) {
                        runtime.selected_officer = game::read_roster_officer(runtime.officer_roster, game_module, command.officer_id);
                    }
                    return {};
                } else if constexpr (std::is_same_v<T, domain::SetRosterOfficerTrait>) {
                    save_config = false;
                    const auto base = game::read_game_base(runtime.game_base_pointer);
                    const auto player = command.protagonist ? game::read_player(base) : 0;
                    const auto target = command.protagonist && !player ? 0 : game::resolve_trait_target(
                        runtime.officer_roster, game_module, command.officer_id, player);
                    if (game::set_officer_trait_direct(target, base, command.slot, command.trait_id)) {
                        runtime.pending_trait_change.reset();
                        if (command.protagonist) {
                            runtime.protagonist_traits = game::read_officer_traits(target);
                        }
                        if (runtime.selected_officer && runtime.selected_officer->id == command.officer_id) {
                            runtime.selected_officer = game::read_roster_officer(runtime.officer_roster, game_module, command.officer_id);
                        }
                        return {};
                    }
                    const auto vtable = game::resolve_officer_trait_vtable(target, base);
                    if (!vtable || command.slot >= 3 || !command.trait_id) {
                        return std::unexpected(domain::Error{domain::ErrorCode::PlatformFailure});
                    }
                    runtime.pending_trait_change = PendingTraitChange{
                        game::TraitObjectSearch{.vtable = vtable, .trait_id = command.trait_id},
                        base, command.officer_id, command.slot, target, command.protagonist};
                    if (ui) {
                        ui->publish_notice("기재 객체를 검색하고 있습니다.");
                    }
                    return {};
                } else if constexpr (std::is_same_v<T, domain::SetRosterOfficerBasic>) {
                    save_config = false;
                    if (!game::write_roster_officer_basic(runtime.officer_roster, game_module,
                                                          command.officer_id, command.index, command.value)) {
                        return std::unexpected(domain::Error{domain::ErrorCode::PlatformFailure});
                    }
                    if (runtime.selected_officer && runtime.selected_officer->id == command.officer_id) {
                        const auto field = domain::officer_basic_fields[command.index];
                        runtime.selected_officer->basic[command.index] = field.size == 1 ? command.value & 0xff
                            : field.size == 2 ? command.value & 0xffff : command.value;
                    }
                    return {};
                } else {
                    save_config = false;
                    if (!game::write_roster_officer_training(runtime.officer_roster, game_module,
                            command.officer_id, command.kind, command.index, command.value)) {
                        return std::unexpected(domain::Error{domain::ErrorCode::PlatformFailure});
                    }
                    if (runtime.selected_officer && runtime.selected_officer->id == command.officer_id) {
                        switch (command.kind) {
                        case domain::OfficerTrainingKind::Tactic:
                            runtime.selected_officer->tactic_levels[command.index] = command.value;
                            break;
                        case domain::OfficerTrainingKind::Skill:
                            runtime.selected_officer->skill_levels[command.index] = command.value;
                            break;
                        case domain::OfficerTrainingKind::Aptitude:
                            runtime.selected_officer->aptitudes[command.index] = command.value;
                            break;
                        }
                    }
                    return {};
                }
            }, accepted->command.payload);
        }
        if (result && runtime.selected_officer &&
            (std::holds_alternative<domain::SetProtagonistBasic>(accepted->command.payload) ||
             std::holds_alternative<domain::SetProtagonistGender>(accepted->command.payload) ||
             std::holds_alternative<domain::SetProtagonistTacticLevel>(accepted->command.payload) ||
             std::holds_alternative<domain::SetProtagonistTraining>(accepted->command.payload)) &&
            runtime.protagonist_id == runtime.selected_officer->id) {
            if (auto detail = game::read_roster_officer(runtime.officer_roster, game_module, runtime.selected_officer->id)) {
                runtime.selected_officer = std::move(*detail);
            }
        }
        if (ui) {
            const auto snapshot = config_snapshot(runtime);
            ui->publish({runtime.epoch, snapshot});
            if (save_config && (result || settings_changed_on_failure)) {
                ui->request_config_save(snapshot);
            }
        }
        if (result && (std::holds_alternative<domain::SetRosterOfficerRonin>(accepted->command.payload) ||
                       std::holds_alternative<domain::ReviveRosterOfficer>(accepted->command.payload))) {
            ronin_completions[ronin_completion_count++] = accepted->completion;
        } else {
            mailbox.complete(accepted->completion, std::move(result));
        }
        ++processed;
    }
    if (ronin_changed) {
        for (auto& entry : runtime.officer_roster.entries) {
            if (ronin_changes[entry.id]) {
                entry.status = 0x58;
            }
        }
        if (ui) {
            if (const auto current = ui->officer_roster_view(); current && current->epoch == runtime.epoch) {
                auto roster = *current;
                for (auto& entry : roster.entries) {
                    if (ronin_changes[entry.id]) {
                        entry.status = 0x58;
                    }
                }
                ui->publish_officer_roster(std::move(roster));
            }
        }
    }
    for (std::size_t i = 0; i < ronin_completion_count; ++i) {
        mailbox.complete(ronin_completions[i], {});
    }
    return processed;
}

namespace {

std::string special_assignment_line(const domain::SpecialAssignmentResult& result,
                                    const domain::OfficerNames* names) {

    std::string line;

    if (names) {

        const domain::OfficerNames::const_iterator found = names->find(result.officer_id);

        if (found != names->end()) {
            line = found->second;
        }
    }

    line += " (ID " + std::to_string(result.officer_id) + ") : ";

    bool first = true;

    for (std::size_t index = 0; index < domain::SPECIAL_ABILITY_COUNT; ++index) {

        if ((result.abilities & (1u << index)) == 0) {
            continue;
        }

        if (!first) {
            line += ", ";
        }

        line += domain::SPECIAL_ABILITY_NAMES[index];
        first = false;
    }

    return line;
}

void service_ai_jobs(Runtime& runtime, services::UiBridge& ui, std::uintptr_t game_module) {

    if (runtime.ai_growth.report) {

        const domain::AiGrowthReport report = *runtime.ai_growth.report;

        runtime.ai_growth_last_report = report;
        runtime.ai_growth.report.reset();
        ui.publish_notice(std::to_string(report.year) + "년 AI 성장 " +
            (report.outcome == domain::AiGrowthOutcome::Completed ? "완료: " : "중단: ") +
            std::to_string(report.consumed) + "명 누적 EXP 소비, " + std::to_string(report.changed) +
            "명 성장, " + std::to_string(report.failed) + "명 적용 실패");

        if (report.outcome == domain::AiGrowthOutcome::Completed && runtime.ai_growth_settings.enabled &&
            runtime.ai_growth_settings.annual_special_abilities) {
            runtime.pending_annual_assignment = report.year;
        }
    }

    if (runtime.pending_annual_assignment != 0 &&
        (!runtime.ai_growth_settings.enabled || !runtime.ai_growth_settings.annual_special_abilities ||
         runtime.ai_growth.completed_year != runtime.pending_annual_assignment)) {
        runtime.pending_annual_assignment = 0;
    }

    if (runtime.pending_annual_assignment != 0 && runtime.skill_counts &&
        game::start_special_ability_assignment(runtime.special_assignment, game_module,
            runtime.loop_game_base, runtime.loop_player, runtime.epoch, ui.skill_counts_view(), true)) {
        runtime.pending_annual_assignment = 0;
        runtime.special_assignment_results.reset();
    }

    if (runtime.skill_counts && game::update_special_ability_assignment(runtime.special_assignment,
            game_module, runtime.loop_game_base, runtime.loop_player, runtime.epoch, *runtime.skill_counts)) {

        const domain::SpecialAssignmentSummary& summary = runtime.special_assignment.summary;
        const std::span<const domain::SpecialAssignmentResult> results =
            game::special_ability_assignment_results(runtime.special_assignment);

        runtime.special_assignment_results = own::make_shared<std::vector<domain::SpecialAssignmentResult>>(
            results.begin(), results.end());
        ++runtime.special_assignment_generation;

        if (summary.assigned != 0) {
            ui.publish_skill_counts_view(*runtime.skill_counts);
            ui.request_skill_counts_save(*runtime.skill_counts);
            runtime.ship_weaponization.last_active_unit = 0;
        }

        ui.publish_notice(std::string(summary.annual ? "연간 특수능력 " : "특수능력 자동 부여 ") +
            (summary.phase == domain::SpecialAssignmentPhase::Completed ? "완료: " : "중단: ") +
            std::to_string(summary.assigned_officers) + "명 / 신규 " + std::to_string(summary.assigned) + "건");

        if (summary.annual && summary.assigned != 0) {
            runtime.special_assignment_notices = runtime.special_assignment_results;
            runtime.special_assignment_notice_cursor = results.size() > 200 ? results.size() - 200 : 0;
        }
    }

    if (runtime.special_assignment_notices) {

        const own::shared_owner<const domain::OfficerNames> names = ui.officer_names_view();
        const std::size_t end = std::min(runtime.special_assignment_notices->size(),
                                        runtime.special_assignment_notice_cursor + 8);

        while (runtime.special_assignment_notice_cursor < end) {

            const domain::SpecialAssignmentResult& result =
                (*runtime.special_assignment_notices)[runtime.special_assignment_notice_cursor++];

            ui.publish_notice("[연간 특수능력] " + special_assignment_line(result, names ? &*names : nullptr));
        }

        if (runtime.special_assignment_notice_cursor == runtime.special_assignment_notices->size()) {
            runtime.special_assignment_notices.reset();
        }
    }
}

} // namespace

std::size_t commit_ui_commands(Runtime& runtime, services::UiBridge& ui,
                               std::uintptr_t game_module, std::size_t max_commands,
                               std::chrono::steady_clock::duration time_budget) {
    const platform::windows::RealClockScope real_clock;

    if (runtime.stopping) {
        return 0;
    }

    if (runtime.pending_trait_change) {
        auto& pending = *runtime.pending_trait_change;
        const auto player = pending.protagonist ? game::read_player(pending.game_base) : 0;
        const auto target = pending.protagonist && !player ? 0 : game::resolve_trait_target(
            runtime.officer_roster, game_module, pending.officer_id, player);
        if (game::read_game_base(runtime.game_base_pointer) != pending.game_base ||
            !target || target != pending.target) {
            runtime.pending_trait_change.reset();
            ui.publish_notice("게임 상태가 바뀌어 기재 검색을 중단했습니다.");
        } else {
            game::scan_trait_object(pending.search, 2 * 1024 * 1024);
            if (pending.search.complete) {
                const bool applied = pending.search.result && game::set_officer_trait_object(
                    target, pending.slot, pending.search.trait_id, pending.search.result, pending.search.vtable);
                if (applied && pending.protagonist) {
                    runtime.protagonist_traits = game::read_officer_traits(target);
                }
                if (applied && runtime.selected_officer && runtime.selected_officer->id == pending.officer_id) {
                    runtime.selected_officer = game::read_roster_officer(runtime.officer_roster, game_module, pending.officer_id);
                }
                ui.publish_notice(applied ? "기재를 변경했습니다." : "기재 객체를 찾지 못했거나 적용하지 못했습니다.");
                runtime.pending_trait_change.reset();
            }
        }
    }
    ui.officer_roster_scan_cursor.store(runtime.officer_roster.requested
        ? static_cast<int>(runtime.officer_roster.cursor) : -1);
    if (runtime.auto_affinity.report) {
        const auto report = *runtime.auto_affinity.report;
        switch (report.outcome) {
        case game::AutoAffinityOutcome::Completed:
            ui.publish_notice("AI 친밀도 자동 성장: " + std::to_string(report.applied) + "쌍 적용, " +
                              std::to_string(report.stale) + "쌍 변경됨, " + std::to_string(report.failed) + "쌍 실패");
            break;
        case game::AutoAffinityOutcome::InsufficientCandidates:
            ui.publish_notice("AI 친밀도 자동 성장: 대상 무장이 부족합니다.");
            break;
        case game::AutoAffinityOutcome::NoRelationTable:
            ui.publish_notice("AI 친밀도 자동 성장: 관계 테이블을 찾지 못했습니다.");
            break;
        case game::AutoAffinityOutcome::DateChanged:
            ui.publish_notice("AI 친밀도 자동 성장: 날짜가 바뀌어 중단했습니다. 적용 " +
                              std::to_string(report.applied) + "쌍");
            break;
        }
        runtime.auto_affinity.report.reset();
    }
    if (runtime.loop_game_base_ready && !runtime.config_loaded) {
        if (auto config = ui.take_config_load()) {
            apply_config(runtime, std::move(*config), game_module);
            runtime.config_loaded = true;
        }
    }
    runtime.mission_appointment_limit_value = game::read_mission_appointment_limit(game_module);
    if (runtime.officer_roster.completed) {
        ui.publish_officer_roster({runtime.epoch, std::move(runtime.officer_roster.entries), false});
        runtime.officer_roster.completed = false;
    } else if (runtime.officer_roster.requested && runtime.officer_roster.cursor > 0 &&
               (runtime.officer_roster.published_base != runtime.officer_roster.base ||
                runtime.officer_roster.published_size != runtime.officer_roster.entries.size())) {
        ui.publish_officer_roster({runtime.epoch, runtime.officer_roster.entries, true});
        runtime.officer_roster.published_base = runtime.officer_roster.base;
        runtime.officer_roster.published_size = runtime.officer_roster.entries.size();
    }
    std::uint16_t protagonist_id = 0;
    if (runtime.loop_player_ready && platform::windows::read_memory(runtime.loop_player + 8,
            std::as_writable_bytes(std::span(&protagonist_id, 1))) && protagonist_id) {
        runtime.protagonist_id = protagonist_id;
        runtime.protagonist_traits = game::read_officer_traits(runtime.loop_player);
        std::uint8_t gender = 0;
        if (platform::windows::read_memory(runtime.loop_player + 0x30, std::as_writable_bytes(std::span(&gender, 1)))) {
            runtime.protagonist_gender = gender;
        } else {
            runtime.protagonist_gender.reset();
        }
        std::array<std::uint8_t, 35> levels{};
        if (platform::windows::read_memory(runtime.loop_player + 0x139, std::as_writable_bytes(std::span(levels)))) {
            runtime.protagonist_tactic_levels = levels;
        } else {
            runtime.protagonist_tactic_levels.reset();
        }
        std::array<std::uint8_t, 24> skills{};
        if (platform::windows::read_memory(runtime.loop_player + 0x1d0, std::as_writable_bytes(std::span(skills)))) {
            runtime.protagonist_skill_levels = skills;
        } else {
            runtime.protagonist_skill_levels.reset();
        }
        std::array<std::uint8_t, 11> aptitudes{};
        if (platform::windows::read_memory(runtime.loop_player + 0xc4, std::as_writable_bytes(std::span(aptitudes)))) {
            runtime.protagonist_aptitudes = aptitudes;
        } else {
            runtime.protagonist_aptitudes.reset();
        }
        domain::OfficerBasicValues basic{};
        bool basic_read = true;
        for (std::size_t i = 0; i < basic.size(); ++i) {
            const auto field = domain::officer_basic_fields[i];
            if (!platform::windows::read_memory(runtime.loop_player + field.offset,
                    std::as_writable_bytes(std::span(&basic[i], 1)).first(field.size))) {
                basic_read = false;
                break;
            }
        }
        if (basic_read) {
            runtime.protagonist_basic = basic;
        } else {
            runtime.protagonist_basic.reset();
        }
    } else {
        runtime.protagonist_id.reset();
        runtime.protagonist_traits.reset();
        runtime.protagonist_gender.reset();
        runtime.protagonist_tactic_levels.reset();
        runtime.protagonist_skill_levels.reset();
        runtime.protagonist_aptitudes.reset();
        runtime.protagonist_basic.reset();
    }
    if (runtime.pending_battle_days_remaining) {
        ui.publish_notice("남은 전투 일자 : " + std::to_string(*runtime.pending_battle_days_remaining) + " 일");
        runtime.pending_battle_days_remaining.reset();
    }
    if (runtime.undiscovered_ronin.completed_modified) {
        ui.publish_notice(std::to_string(*runtime.undiscovered_ronin.completed_modified) +
                          "명의 미발견 무장이 재야로 변경되었습니다.");
        runtime.undiscovered_ronin.completed_modified.reset();
    }
    if (auto definitions = ui.take_named_specialties()) {
        runtime.named_specialties = std::move(definitions->first);
        runtime.specialty_random.seed(definitions->second);
    }
    if (auto counts = ui.take_skill_counts()) {
        runtime.skill_counts = std::move(*counts);
        ui.publish_skill_counts_view(*runtime.skill_counts);
        runtime.ship_weaponization.last_active_unit = 0;
    }
    service_ai_jobs(runtime, ui, game_module);
    service_officer_tools(runtime, ui, game_module);
    service_city_management(runtime, ui, game_module);
    service_corps_deployment(runtime, ui, game_module);
    consume_trait_configuration(runtime.traits, ui);

    const std::uint64_t native_now_ms = GetTickCount64();
    constexpr std::uint8_t PATCH_CONTROLLER_COUNT = 3;

    // Each controller also rotates its own groups. A busy legacy group cannot
    // monopolize the native/trait controllers, nor can their retries starve it.
    for (std::uint8_t offset = 0; offset < PATCH_CONTROLLER_COUNT; ++offset) {

        const std::uint8_t controller = static_cast<std::uint8_t>(
            (runtime.legacy_features.next_controller + offset) % PATCH_CONTROLLER_COUNT);
        bool attempted = false;

        if (controller == 0) {
            attempted = reconcile_legacy_features(runtime, game_module, native_now_ms);
        } else if (controller == 1) {
            attempted = reconcile_native_features(runtime, game_module, native_now_ms);
        } else if (runtime.config_loaded) {
            attempted = reconcile_trait_control(runtime.traits, game_module, runtime.loop_game_base,
                runtime.epoch.value, native_now_ms, ui.external_trait_runtime.load(std::memory_order_acquire));
        }
        if (attempted) {
            runtime.legacy_features.next_controller = static_cast<std::uint8_t>((controller + 1) % PATCH_CONTROLLER_COUNT);
            break;
        }
    }

    if (runtime.legacy_features.notice) {

        const LegacyFeatureNotice notice = *runtime.legacy_features.notice;
        std::string message = domain::legacy_feature_label(notice.group);

        message += notice.succeeded ? ": 요청한 설정을 적용했습니다." :
            ": 적용하지 못했습니다. 요청을 유지하고 다시 시도합니다.";
        ui.publish_notice(std::move(message));
        runtime.legacy_features.notice.reset();
    }

    if (runtime.traits.notice) {
        ui.publish_notice(std::move(*runtime.traits.notice));
        runtime.traits.notice.reset();
    }

    if (runtime.native_features.notice) {

        const NativeFeatureNotice notice = *runtime.native_features.notice;
        const domain::NativeFeatureStatus& status = notice.status;
        std::string message = domain::native_feature_label(notice.group);

        if (status.phase == domain::NativeFeaturePhase::Applied) {
            message += ": 적용했습니다.";
        } else if (status.phase == domain::NativeFeaturePhase::Disabled) {
            message += ": 해제했습니다.";
        } else {
            message += ": ";
            message += domain::native_feature_error_text(status.error);
        }

        ui.publish_notice(std::move(message));
        runtime.native_features.notice.reset();
    }
    if (ui.ui_rendered.exchange(false)) {
        game::update_zero_infamy(runtime.zero_infamy, runtime.cached_game_base, GetTickCount64(),
                                 runtime.features.get(domain::FeatureId::ZeroInfamy));
    }
    if (runtime.config_loaded && !runtime.config_baseline_published) {
        ui.publish_config_baseline({runtime.config_baseline});
        runtime.config_baseline_published = true;
    }
    const bool officer_detail_visible = ui.officer_detail_visible.exchange(false);

    if (officer_detail_visible && runtime.selected_officer &&
        game::roster_traits_changed(runtime.officer_roster, game_module, *runtime.selected_officer)) {
        if (auto detail = game::read_roster_officer(runtime.officer_roster, game_module, runtime.selected_officer->id)) {
            runtime.selected_officer = std::move(*detail);
        }
    }
    if (officer_detail_visible && runtime.selected_officer) {

        const std::uint64_t now_ms = GetTickCount64();

        if (!runtime.ai_growth_preview || runtime.ai_growth_preview->officer_id != runtime.selected_officer->id ||
            now_ms - runtime.ai_growth_preview_ms >= 250) {

            const std::uintptr_t officer = game::resolve_trait_target(runtime.officer_roster, game_module,
                runtime.selected_officer->id, 0);
            domain::AiGrowthPreview preview{};

            runtime.ai_growth_preview_ms = now_ms;

            if (officer && game::read_current_ai_growth_preview(game_module, officer, runtime.loop_player,
                    runtime.ai_growth_settings.speed, preview)) {
                runtime.ai_growth_preview = preview;
            } else {
                runtime.ai_growth_preview.reset();
            }
        }
    }
    ui.publish({runtime.epoch, config_snapshot(runtime)});
    return commit_commands(runtime, ui.commands, game_module, max_commands, time_budget, &ui);
}

void publish_stopped_state(Runtime& runtime, services::UiBridge& ui) {

    if (!runtime.stopped || runtime.stopped_state_published) {
        return;
    }

    if (runtime.skill_counts) {
        ui.publish_skill_counts_view(*runtime.skill_counts);
        ui.request_skill_counts_save(*runtime.skill_counts);
    }

    if (runtime.config_loaded) {

        const domain::ConfigSnapshot snapshot = config_snapshot(runtime);

        ui.publish({runtime.epoch, snapshot});
        ui.request_config_save(snapshot);
    }

    if (runtime.special_assignment.summary.phase == domain::SpecialAssignmentPhase::Cancelled &&
        runtime.special_assignment.summary.assigned != 0) {
        ui.publish_notice("종료 전 특수능력 부여 결과 " +
            std::to_string(runtime.special_assignment.summary.assigned_officers) + "명 / " +
            std::to_string(runtime.special_assignment.summary.assigned) + "건을 저장합니다.");
    }

    runtime.stopped_state_published = true;
}

} // namespace s8::runtime
