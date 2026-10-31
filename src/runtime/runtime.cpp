#include "runtime.h"
#include "../game/stratagem5_config.h"
#include "../game/stratagem5_owner.h"
#include "../game/legacy_config.h"
#include "../game/game_base.h"
#include "../game/stratagem5_metadata.h"
#include "../platform/windows/process_memory.h"
#include "../platform/windows/module_image.h"
#include "../services/ui_bridge.h"

#include <utility>

namespace s8::runtime {

domain::ConfigSnapshot config_snapshot(const Runtime& runtime) {
    return domain::ConfigSnapshot{
        runtime.features,
        runtime.short_battle_cooldown_enabled,
        runtime.short_battle_cooldown_days,
        runtime.troop_count_combat_enabled,
        runtime.reinforcement_arrival_enabled,
        runtime.tech_city_visibility_enabled,
        runtime.stratagem5_lifecycle.desired.load(),
        runtime.stratagem5.runtime.settings,
        runtime.self_heal_settings,
        runtime.dongto_settings,
        runtime.celestial_settings,
        runtime.catapult_settings,
        runtime.terrain_tactics_settings,
        runtime.def_building_settings,
        runtime.skip_days,
        runtime.terrain_bonus,
        runtime.siege_heal_rate,
        runtime.jewel_open.preferred,
        runtime.secondary_jewels,
        runtime.jewel_open.bitmap,
        runtime.domestics,
        runtime.mission_appointment_limit_value,
        runtime.mission_appointment_limit.managed,
        runtime.config_loaded,
        runtime.notification_speed,
        static_cast<domain::DomesticRewardMode>(runtime.domestic_reward.mode.load()),
        runtime.protagonist_id,
        runtime.protagonist_tactic_levels,
        runtime.protagonist_skill_levels,
        runtime.protagonist_aptitudes,
        runtime.protagonist_basic,
        runtime.protagonist_gender,
        runtime.selected_officer,
        runtime.protagonist_traits,
    };
}

bool initialize_game_base(Runtime& runtime, std::span<const std::byte> image, std::uintptr_t module_base) {
    if (const auto address = game::find_game_base_pointer(image, module_base))
        runtime.game_base_pointer = *address;
    return runtime.game_base_pointer != 0;
}

bool initialize_game_base_from_module(Runtime& runtime, std::uintptr_t module_base) {
    const auto image = platform::windows::read_module_image(module_base);
    return image && initialize_game_base(runtime, *image, module_base);
}

void change_game_session(Runtime& runtime) {
    runtime.skill_condition_applied.fill(false);
    runtime.battle_date = {};
    runtime.battle_weather = {};
    runtime.ship_weaponization = {};
    runtime.battle_skill_counts = {};
    runtime.terrain_bonus_state = {};
    runtime.officer_stats = {};
    runtime.officer_roster = {};
    runtime.pending_trait_change.reset();
    runtime.auto_affinity = {};
    runtime.selected_officer.reset();
    runtime.protagonist_traits.reset();
    runtime.protagonist_id.reset();
    runtime.protagonist_tactic_levels.reset();
    runtime.protagonist_skill_levels.reset();
    runtime.protagonist_aptitudes.reset();
    runtime.protagonist_basic.reset();
    runtime.protagonist_gender.reset();
    runtime.all_aggressive = {};
    runtime.zero_infamy = {};
    runtime.siege = {};
    runtime.tavern_requests = {};
    runtime.specialty_last_council = false;
    game::reset_secondary_jewel_cache();
    runtime.battle_map_shuffle = {};
    runtime.faction_lord_bonus = {};
    runtime.undiscovered_ronin = {};
    runtime.last_relevant_phase = 0;
    runtime.battle_mods_refreshed = false;
    runtime.battle_readiness = {};
    runtime.battle_state = {};
    runtime.battle_seen = false;
    reset_stratagem5_battle(runtime.stratagem5_lifecycle);
    game::change_stratagem5_session(runtime.stratagem5);
    game::reset_stratagem5_count(runtime.stratagem5_count);
    game::reset_stratagem5_camp(runtime.stratagem5_camp);
    game::reset_stratagem5_model(runtime.stratagem5_model);
    runtime.stratagem5_attack_owner = 0;
    runtime.stratagem5_defense_owner = 0;
    ++runtime.epoch.value;
}

void request_stratagem5(Runtime& runtime, bool enabled) {
    request_stratagem5(runtime.stratagem5_lifecycle, enabled);
}

bool enable_stratagem5_data(Runtime& runtime, std::uintptr_t module_base) {
    return game::enable_stratagem5(runtime.stratagem5, runtime.epoch, module_base);
}

bool disable_stratagem5_data(Runtime& runtime) {
    return game::disable_stratagem5(runtime.stratagem5);
}

game::Stratagem5SettingsResult set_stratagem5_settings(Runtime& runtime, domain::Stratagem5Settings settings) {
    return game::set_stratagem5_settings(runtime.stratagem5, settings);
}

void load_stratagem5_config(Runtime& runtime, std::string_view text) {
    const auto config = game::read_stratagem5_config(text, runtime.stratagem5.runtime.settings);
    request_stratagem5(runtime, config.enabled);
    set_stratagem5_settings(runtime, config.settings);
}

bool on_stratagem5_lifecycle(Runtime& runtime, std::uintptr_t module_base, bool battle_active,
                             bool safe_non_battle, const Stratagem5UiActions& ui) {
    struct Context {
        Runtime& runtime;
        std::uintptr_t module_base;
        const Stratagem5UiActions& ui;
    } context{runtime, module_base, ui};
    const Stratagem5LifecycleActions actions{
        &context,
        [](void* value) {
            const auto& context = *static_cast<Context*>(value);
            return context.ui.prepare_bridge(context.ui.context);
        },
        [](void* value, bool enabled) {
            auto& context = *static_cast<Context*>(value);
            return enabled ? enable_stratagem5_data(context.runtime, context.module_base)
                           : disable_stratagem5_data(context.runtime);
        },
        [](void* value, bool enabled) {
            const auto& context = *static_cast<Context*>(value);
            if (!enabled) {
                restore_stratagem5_model(context.runtime);
                restore_stratagem5_camp(context.runtime);
            }
            return context.ui.set_metadata(context.ui.context, enabled);
        },
        [](void* value, bool enabled) {
            auto& context = *static_cast<Context*>(value);
            set_stratagem5_count_request(context.runtime, enabled,
                context.runtime.stratagem5_attack_owner, context.runtime.stratagem5_defense_owner);
            if (enabled)
                context.ui.ensure_count_capture(context.ui.context);
            return true;
        },
    };
    return update_stratagem5_lifecycle(runtime.stratagem5_lifecycle, battle_active, safe_non_battle, actions);
}

void on_battle_end(Runtime& runtime) {
    runtime.battle_date = {};
    runtime.battle_weather = {};
    runtime.ship_weaponization = {};
    runtime.battle_skill_counts = {};
    runtime.terrain_bonus_state = {};
    runtime.siege = {};
    runtime.battle_mods_refreshed = false;
    runtime.battle_readiness = {};
    reset_stratagem5_battle(runtime.stratagem5_lifecycle);
    game::end_stratagem5_battle(runtime.stratagem5);
    game::reset_stratagem5_count(runtime.stratagem5_count);
    game::reset_stratagem5_camp(runtime.stratagem5_camp);
    game::reset_stratagem5_model(runtime.stratagem5_model);
    runtime.stratagem5_attack_owner = 0;
    runtime.stratagem5_defense_owner = 0;
}

void on_active_battle_observed(Runtime& runtime, std::uintptr_t module_base, std::uintptr_t game_base,
                               const game::BattleObservation& observation, std::uint64_t now_ms) {
    const bool records_ready = game::battle_records_ready(game_base, observation);
    const bool identity_changed = runtime.battle_readiness.unit_list != observation.unit_list ||
                                  runtime.battle_readiness.day_base != observation.day_base;
    if (identity_changed) {
        runtime.siege = {};
        runtime.battle_date.last_observed_day = 0;
        runtime.battle_skill_counts = {};
    }
    if (!records_ready || identity_changed) {
        runtime.battle_mods_refreshed = false;
    }
    if (game::update_battle_readiness(runtime.battle_readiness, observation, records_ready, now_ms)) {
        if (runtime.skill_counts) {
            game::update_ship_weaponization(runtime.ship_weaponization, module_base,
                                            *runtime.skill_counts, now_ms);
            game::update_battle_skill_counts(runtime.battle_skill_counts, observation, *runtime.skill_counts);
        }
        const bool day_changed = observation.day != runtime.battle_date.last_observed_day;
        game::update_battle_date(runtime.battle_date, observation, now_ms,
            runtime.features.get(domain::FeatureId::DateAlways15),
            runtime.features.get(domain::FeatureId::DateDynamic),
            runtime.features.get(domain::FeatureId::SkipDaysEnabled), runtime.skip_days, day_changed);
        game::update_battle_weather(runtime.battle_weather, module_base, now_ms,
            runtime.features.get(domain::FeatureId::WeatherSkillSimple),
            runtime.features.get(domain::FeatureId::WeatherSkillComplex), day_changed);
        if (day_changed) {
            std::uint8_t final_day = 0;
            if (platform::windows::read_memory(observation.day_base + 0x2c,
                    std::as_writable_bytes(std::span(&final_day, 1)))) {
                runtime.pending_battle_days_remaining = final_day >= observation.day
                    ? final_day - observation.day : 0;
                runtime.battle_date.last_observed_day = observation.day;
            }
        }
        game::update_terrain_bonus(runtime.terrain_bonus_state, observation, runtime.terrain_bonus, now_ms,
            runtime.features.get(domain::FeatureId::TerrainAbilityAtkDef),
            runtime.features.get(domain::FeatureId::TerrainAbilityAll));
        if (runtime.features.get(domain::FeatureId::SiegeWarfare)) {
            game::update_siege(runtime.siege[0], module_base, observation, now_ms, false, runtime.siege_heal_rate);
        }
        if (runtime.features.get(domain::FeatureId::SiegeWarfare2)) {
            game::update_siege(runtime.siege[1], module_base, observation, now_ms, true, runtime.siege_heal_rate);
        }
        on_battle_ready(runtime, module_base, observation.day, observation.unit_count);
        on_battle_update(runtime, module_base, observation.unit_count, observation.unit_list, false);
    }
}

void monitor_battle(Runtime& runtime, std::uintptr_t module_base, std::uintptr_t game_base,
                    std::uint64_t now_ms, const Stratagem5UiActions& ui) {
    const auto mode = game::sample_battle_state(runtime.battle_state, module_base, game_base, now_ms);
    if (runtime.battle_state.game_state == 5 || runtime.battle_state.game_state == 7) {
        runtime.last_relevant_phase = static_cast<std::uint8_t>(runtime.battle_state.game_state);
        const bool council = runtime.battle_state.game_state == 5;
        if (!council && runtime.specialty_last_council && mode == game::BattleMode::NonBattle &&
            runtime.features.get(domain::FeatureId::AutoFillSpecialties) && runtime.named_specialties) {
            game::assign_specialties(module_base, *runtime.named_specialties, runtime.specialty_random);
        }
        if (council || mode != game::BattleMode::Unknown) {
            runtime.specialty_last_council = council;
        }
    }
    if (runtime.last_relevant_phase) {
        game::update_battle_map_shuffle(runtime.battle_map_shuffle, module_base, now_ms,
            runtime.features.get(domain::FeatureId::BattleMapShuffle), runtime.last_relevant_phase == 5,
            runtime.specialty_random);
    }
    if (mode == game::BattleMode::Battle) {
        runtime.battle_seen = true;
        on_stratagem5_lifecycle(runtime, module_base, true, false, ui);
        if (const auto observation = game::read_battle_observation(module_base)) {
            on_active_battle_observed(runtime, module_base, game_base, *observation, now_ms);
        } else {
            runtime.battle_readiness = {};
            runtime.battle_mods_refreshed = false;
        }
    } else if (mode == game::BattleMode::NonBattle) {
        if (runtime.battle_seen) {
            on_battle_end(runtime);
            runtime.battle_seen = false;
        }
        if (!runtime.battle_state.native_present &&
            (runtime.battle_state.game_state == 5 || runtime.battle_state.game_state == 7)) {
            on_stratagem5_lifecycle(runtime, module_base, false, true, ui);
        }
    } else if (mode == game::BattleMode::Unknown) {
        runtime.battle_readiness.ready = false;
        runtime.battle_readiness.stable_since_ms = 0;
    }
}

void on_battle_ready(Runtime& runtime, std::uintptr_t module_base, int day, int unit_count) {
    if (runtime.battle_mods_refreshed || unit_count <= 0 || day < 1 || day > 30) {
        return;
    }
    if (runtime.features.get(domain::FeatureId::SelfHeal)) {
        game::set_self_heal(module_base, true, runtime.self_heal_settings);
    }
    if (runtime.features.get(domain::FeatureId::Dongto)) {
        game::set_dongto(module_base, true, runtime.dongto_settings);
    }
    if (runtime.features.get(domain::FeatureId::Celestial)) {
        game::set_celestial(module_base, true, runtime.celestial_settings);
    }
    if (runtime.features.get(domain::FeatureId::Catapult)) {
        game::set_catapult(module_base, true, runtime.catapult_settings);
    }
    if (runtime.features.get(domain::FeatureId::TerrainIgnore)) {
        game::set_terrain_tactics(module_base, true, runtime.terrain_tactics_settings);
    }
    if (runtime.features.get(domain::FeatureId::DefBuilding)) {
        game::set_def_building(module_base, true, runtime.def_building_settings);
    }
    runtime.battle_mods_refreshed = true;
}

void on_battle_update(Runtime& runtime, std::uintptr_t module_base, int unit_count,
                      std::uintptr_t unit_list, bool skip_stratagem5) {
    if (skip_stratagem5 || should_skip_stratagem5(runtime.stratagem5_lifecycle))
        return;
    game::update_stratagem5_healing(runtime.stratagem5, unit_count, unit_list);
    game::refresh_stratagem5(runtime.stratagem5, runtime.epoch, module_base);
}

bool on_stratagem_metadata(Runtime& runtime, std::uintptr_t metadata_table) {
    return game::apply_stratagem5_metadata(runtime.stratagem5, runtime.epoch, metadata_table);
}

bool on_stratagem5_context(Runtime& runtime, std::uintptr_t metadata_table, std::uintptr_t dialog,
                            std::uintptr_t attack_owner, std::uintptr_t defense_owner) {
    runtime.stratagem5_attack_owner = attack_owner;
    runtime.stratagem5_defense_owner = defense_owner;
    if (should_skip_stratagem5(runtime.stratagem5_lifecycle))
        return false;
    if (!update_stratagem5_count(runtime, metadata_table, dialog, attack_owner, defense_owner))
        return false;
    if (!attach_stratagem5_camp(runtime, metadata_table))
        return false;
    return extend_stratagem5_model(runtime, dialog, metadata_table);
}

std::optional<std::uintptr_t> find_stratagem5_metadata(std::span<const std::byte> image,
                                                        std::uintptr_t game_base) {
    return game::resolve_stratagem5_metadata(image, game_base);
}

bool update_stratagem5_count(Runtime& runtime, std::uintptr_t metadata_table,
    std::uintptr_t dialog_address, std::uintptr_t attack_owner, std::uintptr_t defense_owner) {
    if (!runtime.stratagem5_count.requested || !metadata_table)
        return false;
    const auto valid_attack = game::is_stratagem5_owner(attack_owner, 0)
        ? std::optional{attack_owner} : std::nullopt;
    const auto valid_defense = game::is_stratagem5_owner(defense_owner, 1)
        ? std::optional{defense_owner} : std::nullopt;
    const auto dialog = game::read_stratagem5_dialog_candidate(dialog_address, metadata_table);
    const auto attack = valid_attack ? game::read_stratagem5_candidate(*valid_attack, metadata_table) : std::nullopt;
    const auto defense = valid_defense ? game::read_stratagem5_candidate(*valid_defense, metadata_table) : std::nullopt;
    return game::update_stratagem5_count(runtime.stratagem5_count, runtime.epoch,
                                          valid_attack, valid_defense, dialog, attack, defense);
}

void set_stratagem5_count_request(Runtime& runtime, bool enabled,
                                  std::uintptr_t attack_owner, std::uintptr_t defense_owner) {
    if (enabled) {
        runtime.stratagem5_count.requested = true;
        return;
    }
    const auto valid_attack = game::is_stratagem5_owner(attack_owner, 0)
        ? std::optional{attack_owner} : std::nullopt;
    const auto valid_defense = game::is_stratagem5_owner(defense_owner, 1)
        ? std::optional{defense_owner} : std::nullopt;
    game::disable_stratagem5_count(runtime.stratagem5_count, runtime.epoch, valid_attack, valid_defense);
}

bool attach_stratagem5_camp(Runtime& runtime, std::uintptr_t metadata_table) {
    if (!runtime.stratagem5_count.applied || runtime.stratagem5_count.applied->epoch != runtime.epoch)
        return false;
    const auto owner = runtime.stratagem5_count.applied->owner;
    const auto candidate = game::read_stratagem5_candidate(owner, metadata_table);
    return candidate && game::attach_stratagem5_camp(runtime.stratagem5_camp, runtime.epoch,
                                                      owner, metadata_table, candidate->plan);
}

void restore_stratagem5_camp(Runtime& runtime) {
    game::restore_stratagem5_camp(runtime.stratagem5_camp, runtime.epoch);
}

bool extend_stratagem5_model(Runtime& runtime, std::uintptr_t dialog, std::uintptr_t metadata_table) {
    if (should_skip_stratagem5(runtime.stratagem5_lifecycle) || !runtime.stratagem5_camp.slot)
        return false;
    const auto candidate = game::read_stratagem5_dialog_candidate(dialog, metadata_table);
    if (!candidate || candidate->owner != runtime.stratagem5_camp.slot->owner)
        return false;
    return game::extend_stratagem5_model(runtime.stratagem5_model, runtime.epoch,
                                          candidate->owner, candidate->plan);
}

void restore_stratagem5_model(Runtime& runtime) {
    game::restore_stratagem5_model(runtime.stratagem5_model, runtime.epoch);
}

bool set_short_battle_cooldown(Runtime& runtime, std::uintptr_t module_base, bool enabled, std::int32_t days) {
    return game::set_short_battle_cooldown(runtime.short_battle_cooldown, module_base, enabled, days);
}

bool set_troop_count_combat(Runtime& runtime, std::uintptr_t module_base, bool enabled) {
    const bool result = game::set_troop_count_combat(runtime.troop_count_combat, module_base, enabled);
    runtime.troop_count_combat_enabled = result ? enabled : runtime.troop_count_combat.applied;
    return result;
}

bool set_reinforcement_arrival(Runtime& runtime, std::uintptr_t module_base, bool enabled) {
    const bool result = game::set_reinforcement_arrival(runtime.reinforcement_arrival, module_base, enabled);
    runtime.reinforcement_arrival_enabled = result ? enabled : runtime.reinforcement_arrival.applied;
    return result;
}

bool set_tech_city_visibility(Runtime& runtime, std::uintptr_t module_base, bool enabled) {
    const bool result = game::set_tech_city_visibility(runtime.tech_city_visibility, module_base, enabled);
    runtime.tech_city_visibility_enabled = runtime.tech_city_visibility.applied;
    return result;
}

bool set_infinite_gift(Runtime& runtime, std::uintptr_t module_base,
                       std::span<const std::byte> image, bool enabled) {
    return game::set_infinite_gift(runtime.infinite_gift, module_base, image, enabled);
}

void load_short_battle_cooldown_config(Runtime& runtime, std::string_view text, std::uintptr_t module_base) {
    if (const auto days = game::read_legacy_int(text, "\"iShortBattleCooldownDays\""))
        runtime.short_battle_cooldown_days = *days;
    if (const auto enabled = game::read_legacy_bool(text, "\"bShortBattleCooldownEnabled\"")) {
        runtime.short_battle_cooldown_enabled = *enabled;
        if (!set_short_battle_cooldown(runtime, module_base, *enabled, runtime.short_battle_cooldown_days))
            runtime.short_battle_cooldown_enabled = runtime.short_battle_cooldown.applied;
    }
}

void apply_config(Runtime& runtime, services::ConfigLoad config, std::uintptr_t game_module) {
    runtime.config_baseline = std::move(config.contents);
    const auto& secondary_jewels = config.secondary_jewels;
    const auto& terrain_bonus = config.terrain_bonus;
    const auto& features = config.features;
    const auto& def_building = config.def_building;
    const auto& scalars = config.scalars;
    if (const auto value = scalars.notification_speed) {
        runtime.notification_speed = *value;
    }
    if (const auto value = scalars.domestics_player) {
        runtime.domestics.player = *value;
    }
    if (const auto value = scalars.domestics_force) {
        runtime.domestics.force = *value;
    }
    runtime.domestics_patch.multipliers = runtime.domestics;
    if (game::set_secondary_jewels(secondary_jewels, game_module, runtime.game_base_pointer)) {
        runtime.secondary_jewels = secondary_jewels;
    }
    if (const auto value = scalars.all_jewels_open) {
        runtime.jewel_open.preferred = *value;
    }
    if (const auto value = scalars.siege_heal_rate) {
        runtime.siege_heal_rate = *value;
    }
    for (std::size_t troop = 1; troop <= 12; ++troop) {
        for (std::size_t terrain = 1; terrain <= 17; ++terrain) {
            if (const auto value = terrain_bonus[troop][terrain]) {
                runtime.terrain_bonus.values[troop][terrain] = *value;
            }
        }
    }
    if (const auto value = scalars.skip_days) {
        runtime.skip_days = *value;
    }
    for (std::size_t i = 0; i < features.size(); ++i) {
        if (features[i]) {
            runtime.features.values[i] = *features[i];
        }
    }
    runtime.recurring_features.infinite_action_points = runtime.features.get(domain::FeatureId::InfiniteAp);
    runtime.recurring_features.infinite_jewel_changes = runtime.features.get(domain::FeatureId::FastJewel);
    runtime.recurring_features.infinite_banquet = runtime.features.get(domain::FeatureId::InfiniteBanquet);
    runtime.recurring_features.infinite_mediation = runtime.features.get(domain::FeatureId::InfiniteMediation);
    runtime.infinite_gift_enabled = runtime.features.get(domain::FeatureId::InfiniteGift);
    runtime.infinite_talk_enabled = runtime.features.get(domain::FeatureId::InfiniteTalk);
    runtime.infinite_duel_enabled = runtime.features.get(domain::FeatureId::InfiniteDuel);
    runtime.infinite_debate_enabled = runtime.features.get(domain::FeatureId::InfiniteDebate);
    runtime.instant_attitude_enabled = runtime.features.get(domain::FeatureId::AttitudeHack);
    runtime.fast_relationship_enabled = runtime.features.get(domain::FeatureId::FastRelationship);
    runtime.marriage_enabled = runtime.features.get(domain::FeatureId::MarriageApplied);
    game::set_tech_zero(runtime.tech_zero, runtime.features.get(domain::FeatureId::TechZero));
    runtime.self_heal_settings.self = {
        runtime.features.get(domain::FeatureId::HealLv1Self),
        runtime.features.get(domain::FeatureId::HealLv2Self),
        runtime.features.get(domain::FeatureId::HealLv3Self)};
    for (std::size_t i = 0; i < scalars.heal_amount.size(); ++i) {
        if (const auto value = scalars.heal_amount[i]) {
            runtime.self_heal_settings.amount[i] = *value;
        }
    }
    for (std::size_t i = 0; i < scalars.heal_range.size(); ++i) {
        if (const auto value = scalars.heal_range[i]) {
            runtime.self_heal_settings.range[i] = *value;
        }
    }
    for (std::size_t i = 0; i < scalars.dongto_probability.size(); ++i) {
        if (const auto value = scalars.dongto_probability[i]) {
            runtime.dongto_settings.probability[i] = *value;
        }
        if (const auto value = scalars.dongto_status_probability[i]) {
            runtime.dongto_settings.status_probability[i] = *value;
        }
    }
    for (std::size_t i = 0; i < scalars.celestial_probability.size(); ++i) {
        if (const auto value = scalars.celestial_probability[i]) {
            runtime.celestial_settings.probability[i] = *value;
        }
        if (const auto value = scalars.celestial_amount[i]) {
            runtime.celestial_settings.amount[i] = *value;
        }
        if (const auto value = scalars.celestial_range[i]) {
            runtime.celestial_settings.range[i] = *value;
        }
    }
    if (const auto value = scalars.catapult_min_range) {
        runtime.catapult_settings.min_range = *value;
    }
    if (const auto value = scalars.catapult_max_range) {
        runtime.catapult_settings.max_range = *value;
    }
    if (const auto value = scalars.catapult_amount) {
        runtime.catapult_settings.amount = *value;
    }
    if (const auto value = scalars.catapult_range) {
        runtime.catapult_settings.range = *value;
    }
    for (std::size_t i = 0; i < scalars.water_amount.size(); ++i) {
        if (const auto value = scalars.water_amount[i]) {
            runtime.terrain_tactics_settings.water_amount[i] = *value;
        }
        if (const auto value = scalars.water_range[i]) {
            runtime.terrain_tactics_settings.water_range[i] = *value;
        }
        if (const auto value = scalars.stone_amount[i]) {
            runtime.terrain_tactics_settings.stone_amount[i] = *value;
        }
        if (const auto value = scalars.stone_range[i]) {
            runtime.terrain_tactics_settings.stone_range[i] = *value;
        }
    }
    for (std::size_t i = 0; i < domain::DEF_BUILDING_KEYS.size(); ++i) {
        if (const auto value = def_building[i]) {
            runtime.def_building_settings.values[i] = *value;
        }
    }
    if (const auto enabled = scalars.tech_city_visibility)
        set_tech_city_visibility(runtime, game_module, *enabled);
    auto stratagem5 = runtime.stratagem5.runtime.settings;
    constexpr std::array stratagem5_fields{
        &domain::Stratagem5Settings::target, &domain::Stratagem5Settings::effect1,
        &domain::Stratagem5Settings::power1, &domain::Stratagem5Settings::duration1,
        &domain::Stratagem5Settings::effect2, &domain::Stratagem5Settings::power2,
        &domain::Stratagem5Settings::duration2, &domain::Stratagem5Settings::range,
        &domain::Stratagem5Settings::heal_amount};
    for (std::size_t i = 0; i < stratagem5_fields.size(); ++i) {
        if (scalars.stratagem5[i]) {
            stratagem5.*stratagem5_fields[i] = *scalars.stratagem5[i];
        }
    }
    request_stratagem5(runtime, scalars.stratagem5_enabled);
    set_stratagem5_settings(runtime, stratagem5);
    if (const auto enabled = scalars.reinforcement_arrival)
        set_reinforcement_arrival(runtime, game_module, *enabled);
    if (const auto days = scalars.short_battle_cooldown_days) {
        runtime.short_battle_cooldown_days = *days;
    }
    if (const auto enabled = scalars.short_battle_cooldown_enabled) {
        runtime.short_battle_cooldown_enabled = *enabled;
        if (!set_short_battle_cooldown(runtime, game_module, *enabled, runtime.short_battle_cooldown_days)) {
            runtime.short_battle_cooldown_enabled = runtime.short_battle_cooldown.applied;
        }
    }
    if (const auto enabled = scalars.troop_count_combat)
        set_troop_count_combat(runtime, game_module, *enabled);
}

void tick_recurring_features(Runtime& runtime, std::uintptr_t module_base,
                             std::uintptr_t game_base, bool game_base_ready,
                             std::uintptr_t player, bool player_ready) {
    game::tick_recurring_features(runtime.recurring_features, module_base, game_base, game_base_ready,
                                  player, player_ready);
}

void tick_game(Runtime& runtime, std::uintptr_t game_module, std::uint64_t now_ms) {
    auto game_base = game::read_game_base(runtime.game_base_pointer);
    if (!(game_base && game_base == runtime.cached_game_base && now_ms - runtime.game_base_probe_ms < 300)) {
        runtime.game_base_probe_ms = now_ms;
        if (!game_base || !platform::windows::is_committed_memory(game_base, sizeof(std::uintptr_t)))
            game_base = 0;
        runtime.cached_game_base = game_base;
    }
    const auto player = game::read_player(game_base);
    if (game_base != runtime.loop_game_base || player != runtime.loop_player ||
        now_ms - runtime.loop_probe_ms >= 200) {
        runtime.loop_probe_ms = now_ms;
        runtime.loop_game_base = game_base;
        runtime.loop_player = player;
        runtime.loop_player_ready = player > 0x10000 && platform::windows::is_committed_memory(player, 0x200);
        runtime.loop_game_base_ready = game_base > 0x10000 && platform::windows::is_committed_memory(game_base, 0x6000);
    }

    if (runtime.session_player &&
        (player != runtime.session_player || game_base != runtime.session_game_base || !runtime.loop_player_ready)) {
        change_game_session(runtime);
        runtime.session_player = runtime.loop_player_ready ? player : 0;
        runtime.session_game_base = game_base;
    } else if (!runtime.session_player && runtime.loop_player_ready) {
        runtime.session_player = player;
        runtime.session_game_base = game_base;
    }

    tick_recurring_features(runtime, game_module, game_base, runtime.loop_game_base_ready,
                            player, runtime.loop_player_ready);
    game::update_officer_roster(runtime.officer_roster, game_module);
    game::tick_auto_affinity(runtime.auto_affinity, game_module, game_base, player, now_ms,
                             runtime.features.get(domain::FeatureId::AutoAffinityGrowth));
    game::tick_tech_zero(runtime.tech_zero, game_module, now_ms);
    game::update_officer_stats(runtime.officer_stats, game_module, game_base,
                               runtime.features.get(domain::FeatureId::AutoStatUp99));
    game::update_all_aggressive(runtime.all_aggressive, game_module, player, now_ms,
                                runtime.features.get(domain::FeatureId::AllAggressive));
    game::update_tavern_requests(runtime.tavern_requests, game_module, game_base, now_ms,
                                 runtime.features.get(domain::FeatureId::InfiniteTavernRequests));
    game::update_undiscovered_ronin(runtime.undiscovered_ronin, game_module, game_base, player, now_ms,
                                    runtime.features.get(domain::FeatureId::UndiscoveredToRonin));
    game::update_jewel_open(runtime.jewel_open, game_base, now_ms);
    if (runtime.features.get(domain::FeatureId::Loyalty) != runtime.instant_loyalty.applied) {
        const bool enabled = runtime.features.get(domain::FeatureId::Loyalty);
        if (!enabled || now_ms - runtime.loyalty_last_attempt_ms >= 1000) {
            runtime.loyalty_last_attempt_ms = now_ms;
            if (runtime.instant_loyalty.address || !enabled) {
                game::set_instant_loyalty(runtime.instant_loyalty, game_module, {}, enabled);
            } else if (const auto image = platform::windows::read_module_image(game_module)) {
                game::set_instant_loyalty(runtime.instant_loyalty, game_module, *image, enabled);
            }
        }
    }
    const bool love_enabled = runtime.features.get(domain::FeatureId::HateCave);
    if ((!love_enabled && (runtime.love_hate.affinity_applied || runtime.love_hate.relationship_applied)) ||
        (love_enabled && (!runtime.love_hate.affinity_applied || !runtime.love_hate.relationship_applied) &&
         now_ms - runtime.love_hate_last_attempt_ms >= 1000)) {
        runtime.love_hate_last_attempt_ms = now_ms;
        if (!love_enabled) {
            game::set_love_hate(runtime.love_hate, game_module, {}, false);
        } else if (const auto image = platform::windows::read_module_image(game_module)) {
            game::set_love_hate(runtime.love_hate, game_module, *image, true);
        }
    }
    const bool domestics_enabled = runtime.features.get(domain::FeatureId::Domestics);
    const bool domestics_config_ready = runtime.loop_player_ready && runtime.config_first_player_ms &&
        now_ms - runtime.config_first_player_ms >= 3000;
    if (domestics_enabled != runtime.domestics_patch.applied &&
        (!domestics_enabled || ((runtime.domestics_manual_request || domestics_config_ready) &&
         now_ms - runtime.domestics_last_attempt_ms >= 1000))) {
        runtime.domestics_last_attempt_ms = now_ms;
        if (!domestics_enabled) {
            game::set_domestics(runtime.domestics_patch, game_module, {}, false);
        } else if (const auto image = platform::windows::read_module_image(game_module)) {
            if (game::set_domestics(runtime.domestics_patch, game_module, *image, true)) {
                runtime.domestics_manual_request = false;
            }
        }
    }
    const bool resonance_enabled = runtime.features.get(domain::FeatureId::Resonance);
    if ((!resonance_enabled && (runtime.instant_resonance.select_applied || runtime.instant_resonance.getter_applied)) ||
        (resonance_enabled && (!runtime.instant_resonance.select_applied || !runtime.instant_resonance.getter_applied) &&
         now_ms - runtime.resonance_last_attempt_ms >= 1000)) {
        runtime.resonance_last_attempt_ms = now_ms;
        if (!resonance_enabled) {
            game::set_instant_resonance(runtime.instant_resonance, game_module, {}, false);
        } else if (const auto image = platform::windows::read_module_image(game_module)) {
            game::set_instant_resonance(runtime.instant_resonance, game_module, *image, true);
        }
    }
    if (runtime.features.get(domain::FeatureId::FactionLordBonus) && !runtime.faction_lord_bonus.enabled &&
        runtime.config_first_player_ms && now_ms - runtime.config_first_player_ms >= 3000) {
        game::set_faction_lord_bonus(runtime.faction_lord_bonus, game_module, true);
    }
    game::update_faction_lord_bonus(runtime.faction_lord_bonus, game_module, now_ms);
    for (std::size_t i = 0; i < runtime.road_blocks.size(); ++i) {
        game::tick_road_block(runtime.road_blocks[i], game_module, i == 1, now_ms);
    }
    for (auto& conversion : runtime.city_conversions)
        game::tick_city_conversion(conversion, runtime.epoch, now_ms);

    if (!game_base) {
        if (runtime.config_first_game_base_ms || runtime.config_first_player_ms) {
            runtime.infinite_gift_config_applied = false;
            runtime.infinite_talk_config_applied = false;
            runtime.infinite_duel_config_applied = false;
            runtime.infinite_debate_config_applied = false;
            runtime.instant_attitude_config_applied = false;
            runtime.fast_relationship_config_applied = false;
            runtime.city_config_applied.fill(false);
            runtime.marriage_config_applied = false;
            runtime.config_first_game_base_ms = 0;
            runtime.config_first_player_ms = 0;
        }
        return;
    }
    if (now_ms - runtime.config_last_check_ms < 1000)
        return;
    runtime.config_last_check_ms = now_ms;
    if (!runtime.config_first_game_base_ms) {
        runtime.config_first_game_base_ms = now_ms;
        return;
    }
    const bool player_valid = player > 0x10000 && platform::windows::is_committed_memory(player, 0x100);
    if (player_valid) {
        if (!runtime.config_first_player_ms)
            runtime.config_first_player_ms = now_ms;
    } else if (runtime.config_first_player_ms) {
        runtime.infinite_gift_config_applied = false;
        runtime.infinite_talk_config_applied = false;
        runtime.infinite_duel_config_applied = false;
        runtime.infinite_debate_config_applied = false;
        runtime.instant_attitude_config_applied = false;
        runtime.fast_relationship_config_applied = false;
        runtime.city_config_applied.fill(false);
        runtime.marriage_config_applied = false;
        runtime.config_first_player_ms = 0;
    }
    if (!player_valid || !runtime.config_first_player_ms || now_ms - runtime.config_first_player_ms < 3000)
        return;
    constexpr std::array ROAD_FEATURES{domain::FeatureId::RoadBlock, domain::FeatureId::RoadBlock2};
    for (std::size_t i = 0; i < ROAD_FEATURES.size(); ++i) {
        if (runtime.features.get(ROAD_FEATURES[i]) && !runtime.road_blocks[i].applied) {
            game::set_road_block(runtime.road_blocks[i], game_module, i == 1, true);
        }
    }
    constexpr std::array SKILL_FEATURES{domain::FeatureId::SkillCondition, domain::FeatureId::YumokCondition,
                                        domain::FeatureId::SangbyeongCondition};
    for (std::size_t i = 0; i < SKILL_FEATURES.size(); ++i) {
        if (runtime.features.get(SKILL_FEATURES[i]) && !runtime.skill_condition_applied[i] &&
            game::set_skill_condition(game_module, static_cast<game::SkillConditionKind>(i), true)) {
            runtime.skill_condition_applied[i] = true;
        }
    }
    constexpr std::array CITY_FEATURES{
        domain::FeatureId::BigCity, domain::FeatureId::BangmokCity,
        domain::FeatureId::NonggyeongCity, domain::FeatureId::SangeopCity};
    constexpr std::array CITY_KINDS{
        game::CityConversionKind::BigCity, game::CityConversionKind::Grazing,
        game::CityConversionKind::Farming, game::CityConversionKind::Commerce};
    for (std::size_t i = 0; i < CITY_FEATURES.size(); ++i) {
        const bool enabled = runtime.features.get(CITY_FEATURES[i]);
        if (enabled != runtime.city_config_applied[i]) {
            if (game::request_city_conversion(runtime.city_conversions[i], CITY_KINDS[i], enabled,
                                               runtime.epoch, game_module, now_ms)) {
                runtime.city_config_applied[i] = enabled;
            }
        }
    }
    if (runtime.instant_attitude_enabled != runtime.instant_attitude_config_applied) {
        bool applied = false;
        if (runtime.instant_attitude.address) {
            applied = game::set_instant_attitude(runtime.instant_attitude, game_module, {}, runtime.instant_attitude_enabled);
        } else if (const auto image = platform::windows::read_module_image(game_module)) {
            applied = game::set_instant_attitude(runtime.instant_attitude, game_module, *image, runtime.instant_attitude_enabled);
        }
        if (applied) {
            runtime.instant_attitude_config_applied = runtime.instant_attitude_enabled;
        }
    }
    if (runtime.fast_relationship_enabled != runtime.fast_relationship_config_applied) {
        bool applied = false;
        if (runtime.fast_relationship.address) {
            applied = game::set_fast_relationship(runtime.fast_relationship, game_module, {}, runtime.fast_relationship_enabled);
        } else if (const auto image = platform::windows::read_module_image(game_module)) {
            applied = game::set_fast_relationship(runtime.fast_relationship, game_module, *image, runtime.fast_relationship_enabled);
        }
        if (applied) {
            runtime.fast_relationship_config_applied = runtime.fast_relationship_enabled;
        }
    }
    if (runtime.infinite_gift_enabled != runtime.infinite_gift_config_applied) {
        bool applied = false;
        if (runtime.infinite_gift.code_address) {
            applied = set_infinite_gift(runtime, game_module, {}, runtime.infinite_gift_enabled);
        } else if (const auto image = platform::windows::read_module_image(game_module)) {
            applied = set_infinite_gift(runtime, game_module, *image, runtime.infinite_gift_enabled);
        }
        if (applied) {
            runtime.infinite_gift_config_applied = runtime.infinite_gift_enabled;
        }
    }
    if (runtime.infinite_talk_enabled != runtime.infinite_talk_config_applied) {
        bool applied = false;
        if (runtime.infinite_talk.hook) {
            applied = game::set_infinite_talk(runtime.infinite_talk, game_module, {}, runtime.infinite_talk_enabled);
        } else if (const auto image = platform::windows::read_module_image(game_module)) {
            applied = game::set_infinite_talk(runtime.infinite_talk, game_module, *image, runtime.infinite_talk_enabled);
        }
        if (applied) {
            runtime.infinite_talk_config_applied = runtime.infinite_talk_enabled;
        }
    }
    if (runtime.infinite_duel_enabled != runtime.infinite_duel_config_applied) {
        bool applied = false;
        if (runtime.infinite_duel_debate.hook) {
            applied = game::set_infinite_duel_debate(runtime.infinite_duel_debate, game_module, {},
                game::DuelDebateFeature::Duel, runtime.infinite_duel_enabled);
        } else if (const auto image = platform::windows::read_module_image(game_module)) {
            applied = game::set_infinite_duel_debate(runtime.infinite_duel_debate, game_module, *image,
                game::DuelDebateFeature::Duel, runtime.infinite_duel_enabled);
        }
        if (applied) {
            runtime.infinite_duel_config_applied = runtime.infinite_duel_enabled;
        }
    }
    if (runtime.infinite_debate_enabled != runtime.infinite_debate_config_applied) {
        bool applied = false;
        if (runtime.infinite_duel_debate.hook) {
            applied = game::set_infinite_duel_debate(runtime.infinite_duel_debate, game_module, {},
                game::DuelDebateFeature::Debate, runtime.infinite_debate_enabled);
        } else if (const auto image = platform::windows::read_module_image(game_module)) {
            applied = game::set_infinite_duel_debate(runtime.infinite_duel_debate, game_module, *image,
                game::DuelDebateFeature::Debate, runtime.infinite_debate_enabled);
        }
        if (applied) {
            runtime.infinite_debate_config_applied = runtime.infinite_debate_enabled;
        }
    }
    if (runtime.marriage_enabled != runtime.marriage_config_applied) {
        bool applied = false;
        if (runtime.marriage_condition.address) {
            applied = game::set_marriage_condition(runtime.marriage_condition, game_module, {}, runtime.marriage_enabled);
        } else if (const auto image = platform::windows::read_module_image(game_module)) {
            applied = game::set_marriage_condition(runtime.marriage_condition, game_module, *image, runtime.marriage_enabled);
        }
        if (applied) {
            runtime.marriage_config_applied = runtime.marriage_enabled;
        }
    }
}

} // namespace s8::runtime
