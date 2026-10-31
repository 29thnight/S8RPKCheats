#include "runtime.h"
#include "../game/stratagem5_config.h"
#include "../game/stratagem5_owner.h"
#include "../game/legacy_config.h"
#include "../game/game_base.h"
#include "../game/player_resources.h"
#include "../game/stratagem5_metadata.h"
#include "../platform/windows/process_memory.h"
#include "../platform/windows/module_image.h"
#include "../platform/windows/speed_clock.h"
#include "../services/ui_bridge.h"

#include <utility>
#include <algorithm>
#include <cmath>

namespace s8::runtime {

domain::ConfigSnapshot config_snapshot(const Runtime& runtime) {
    if (runtime.stop_snapshot) {
        return *runtime.stop_snapshot;
    }

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
        runtime.speed_multiplier,
        runtime.tengi_settings,
        runtime.tengi.layout_supported,
        runtime.tengi.snapshot_ready,
        runtime.tengi.momentum,
        runtime.tengi.filter_error,
        runtime.ai_growth_settings,
        runtime.ai_growth.running,
        runtime.ai_growth.cursor,
        runtime.ai_growth.game_setting_failed,
        runtime.ai_growth_last_report,
        runtime.ai_growth_preview,
        runtime.special_assignment.summary,
        runtime.special_assignment_results,
        runtime.special_assignment_generation,
        runtime.native_features.statuses,
        runtime.traits.view,
        runtime.city_management.snapshot.settings,
        runtime.ui_scale,
        runtime.player_resources,
        runtime.legacy_features.statuses,
    };
}

bool initialize_game_base(Runtime& runtime, std::span<const std::byte> image, std::uintptr_t module_base) {
    if (const auto address = game::find_game_base_pointer(image, module_base))
        runtime.game_base_pointer = *address;
    return runtime.game_base_pointer != 0;
}

bool initialize_game_base_from_module(Runtime& runtime, std::uintptr_t module_base) {
    if (runtime.image_module != module_base || runtime.module_image.empty()) {
        std::optional<std::vector<std::byte>> image = platform::windows::read_module_image(module_base);

        if (!image) {
            return false;
        }

        runtime.module_image = std::move(*image);
        runtime.image_module = module_base;
    }

    return initialize_game_base(runtime, runtime.module_image, module_base);
}

void change_game_session(Runtime& runtime) {
    runtime.player_resources.reset();
    runtime.player_resources_next_ms = 0;
    runtime.skill_condition_applied.fill(false);
    runtime.battle_date = {};
    runtime.battle_weather = {};
    runtime.ship_weaponization = {};
    runtime.battle_skill_counts = {};
    runtime.terrain_bonus_state = {};
    runtime.officer_stats = {};
    game::reset_officer_roster(runtime.officer_roster);
    runtime.pending_trait_change.reset();
    runtime.auto_affinity = {};
    runtime.selected_officer.reset();
    runtime.ai_growth_preview.reset();
    runtime.ai_growth_last_report.reset();
    runtime.pending_annual_assignment = 0;
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
    runtime.specialty_assignment_requested = false;
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
    game::reset_officer_tools(*runtime.officer_tools, runtime.epoch);
    game::reset_officer_notifications(*runtime.officer_notices, runtime.epoch);
    runtime.officer_tools_publish_pending = true;
    runtime.officer_notices_publish_pending = true;
    change_native_feature_epoch(runtime);
    change_legacy_feature_epoch(runtime);
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
            const Context& context = *static_cast<Context*>(value);

            return context.ui.prepare_bridge(context.ui.context);
        },
        [](void* value, bool enabled) {
            Context& context = *static_cast<Context*>(value);

            return enabled ? enable_stratagem5_data(context.runtime, context.module_base)
                           : disable_stratagem5_data(context.runtime);
        },
        [](void* value, bool enabled) {
            const Context& context = *static_cast<Context*>(value);

            if (!enabled) {
                // The saved model includes the count/owner backing state.
                // Keep that state intact until model restoration succeeds.
                if (!restore_stratagem5_model(context.runtime) || !restore_stratagem5_camp(context.runtime)) {
                    return false;
                }
            }
            return context.ui.set_metadata(context.ui.context, enabled);
        },
        [](void* value, bool enabled) {
            Context& context = *static_cast<Context*>(value);
            const bool count_ok = set_stratagem5_count_request(context.runtime, enabled,
                context.runtime.stratagem5_attack_owner, context.runtime.stratagem5_defense_owner);

            return count_ok && (!enabled || context.ui.ensure_count_capture(context.ui.context));
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
            runtime.features.get(domain::FeatureId::WeatherSkillComplex), day_changed,
            runtime.features.get(domain::FeatureId::TerrainIgnore));
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
            runtime.specialty_assignment_requested = true;
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
    if (should_skip_stratagem5(runtime.stratagem5_lifecycle)) {
        return false;
    }
    const std::optional<game::Stratagem5CountCandidate> candidate =
        game::read_stratagem5_dialog_candidate(dialog, metadata_table);
    std::uintptr_t camp_data = 0;

    if (!candidate || !platform::windows::read_memory(candidate->owner + 0x10,
            std::as_writable_bytes(std::span(&camp_data, 1))) || !camp_data) {
        return false;
    }
    const std::optional<game::Stratagem5CountApplied>& count = runtime.stratagem5_count.applied;
    const std::optional<game::Stratagem5CampSlot>& camp = runtime.stratagem5_camp.slot;
    const std::optional<game::Stratagem5ModelApplied>& model = runtime.stratagem5_model.applied;
    const bool count_matches = !count || (count->epoch == runtime.epoch && count->owner == candidate->owner &&
        count->camp_data == camp_data && count->entry_index == candidate->plan.original_count &&
        count->metadata_row == candidate->plan.added.data);
    const bool camp_matches = !camp || (camp->epoch == runtime.epoch && camp->owner == candidate->owner &&
        camp->camp_data == camp_data && camp->metadata_table == metadata_table &&
        camp->original_count == candidate->plan.original_count &&
        camp->address == camp_data + 0x68 + static_cast<std::uintptr_t>(candidate->plan.camp_slot) * sizeof(std::uintptr_t));
    const bool model_matches = !model || (model->epoch == runtime.epoch && model->owner == candidate->owner &&
        model->camp_data == camp_data && model->original_count == candidate->plan.original_count &&
        model->replacement_entry.data == candidate->plan.added.data);
    const bool pending_recovery = (count && !count->verified) || (camp && camp->uncertain) ||
        (model && (model->uncertain || !candidate->plan.already_present));

    if (!count_matches || !camp_matches || !model_matches || pending_recovery) {
        // Finish the old owner's restoration before publishing another owner.
        // A failed read/write retains that layer's backup for the next event.
        if (!restore_stratagem5_model(runtime) || !restore_stratagem5_camp(runtime)) {
            return false;
        }
        const bool requested = runtime.stratagem5_count.requested;
        const bool restored = game::disable_stratagem5_count(runtime.stratagem5_count,
            runtime.epoch, std::nullopt, std::nullopt);

        runtime.stratagem5_count.requested = requested;
        if (!restored) {
            return false;
        }
    }
    if (!update_stratagem5_count(runtime, metadata_table, dialog, attack_owner, defense_owner)) {
        return false;
    }
    if (!attach_stratagem5_camp(runtime, metadata_table)) {
        return false;
    }
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

bool set_stratagem5_count_request(Runtime& runtime, bool enabled,
                                  std::uintptr_t attack_owner, std::uintptr_t defense_owner) {
    if (enabled) {
        runtime.stratagem5_count.requested = true;
        return true;
    }
    const std::optional<std::uintptr_t> valid_attack = game::is_stratagem5_owner(attack_owner, 0)
        ? std::optional<std::uintptr_t>{attack_owner} : std::nullopt;
    const std::optional<std::uintptr_t> valid_defense = game::is_stratagem5_owner(defense_owner, 1)
        ? std::optional<std::uintptr_t>{defense_owner} : std::nullopt;

    return game::disable_stratagem5_count(runtime.stratagem5_count, runtime.epoch, valid_attack, valid_defense);
}

bool attach_stratagem5_camp(Runtime& runtime, std::uintptr_t metadata_table) {
    if (!runtime.stratagem5_count.applied || runtime.stratagem5_count.applied->epoch != runtime.epoch)
        return false;
    const auto owner = runtime.stratagem5_count.applied->owner;
    const auto candidate = game::read_stratagem5_candidate(owner, metadata_table);
    return candidate && game::attach_stratagem5_camp(runtime.stratagem5_camp, runtime.epoch,
                                                      owner, metadata_table, candidate->plan);
}

bool restore_stratagem5_camp(Runtime& runtime) {
    return game::restore_stratagem5_camp(runtime.stratagem5_camp, runtime.epoch);
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

bool restore_stratagem5_model(Runtime& runtime) {
    return game::restore_stratagem5_model(runtime.stratagem5_model, runtime.epoch);
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

void load_short_battle_cooldown_config(Runtime& runtime, std::string_view text, std::uintptr_t) {
    if (const auto days = game::read_legacy_int(text, "\"iShortBattleCooldownDays\""))
        runtime.short_battle_cooldown_days = *days;
    if (const auto enabled = game::read_legacy_bool(text, "\"bShortBattleCooldownEnabled\"")) {
        runtime.short_battle_cooldown_enabled = *enabled;
        request_legacy_group(runtime, domain::LegacyFeatureGroup::ShortBattleCooldown, false);
    }
}

void apply_config(Runtime& runtime, services::ConfigLoad config, std::uintptr_t) {
    runtime.config_baseline = std::move(config.contents);
    const auto& secondary_jewels = config.secondary_jewels;
    const auto& terrain_bonus = config.terrain_bonus;
    const auto& features = config.features;
    const auto& def_building = config.def_building;
    const auto& scalars = config.scalars;
    domain::CitySettings city_settings = runtime.city_management.snapshot.settings;

    if (scalars.city_grain_limit && *scalars.city_grain_limit >= 0) {
        city_settings.grain_limit = static_cast<std::uint32_t>(*scalars.city_grain_limit);
    }
    if (scalars.city_retained_grain && *scalars.city_retained_grain >= 0) {
        city_settings.retained_grain = static_cast<std::uint32_t>(*scalars.city_retained_grain);
    }
    if (scalars.city_exchange_rate && *scalars.city_exchange_rate > 0) {
        city_settings.exchange_rate = static_cast<std::uint32_t>(*scalars.city_exchange_rate);
    }
    if (scalars.city_auto_exchange) {
        city_settings.auto_exchange = *scalars.city_auto_exchange;
    }
    if (scalars.city_revolt_zero) {
        city_settings.revolt_always_zero = *scalars.city_revolt_zero;
    }
    game::set_city_settings(runtime.city_management, city_settings);
    if (scalars.ai_growth_enabled) {
        runtime.ai_growth_settings.enabled = *scalars.ai_growth_enabled;
    }
    if (scalars.ai_growth_speed) {
        runtime.ai_growth_settings.speed = *scalars.ai_growth_speed;
    }
    if (scalars.ai_growth_restore_none) {
        runtime.ai_growth_settings.restore_none = *scalars.ai_growth_restore_none;
    }
    if (scalars.annual_special_abilities) {
        runtime.ai_growth_settings.annual_special_abilities = *scalars.annual_special_abilities;
    }
    runtime.ai_growth_settings = domain::normalize_ai_growth_settings(runtime.ai_growth_settings);
    if (scalars.speed_multiplier) {
        runtime.speed_multiplier = domain::normalize_speed_multiplier(*scalars.speed_multiplier);
    }
    runtime.speed_clock_configured = false;
    for (std::size_t index = 0; index < domain::TENGI_EVENT_COUNT; ++index) {
        domain::TengiEventSettings& event = runtime.tengi_settings.events[index];

        if (scalars.tengi_allowed[index]) {
            event.allowed = *scalars.tengi_allowed[index];
        }
        if (scalars.tengi_duration[index]) {
            event.duration_months = *scalars.tengi_duration[index];
        }
        if (scalars.tengi_cooldown[index]) {
            event.cooldown_years = *scalars.tengi_cooldown[index];
        }
    }
    runtime.tengi_settings = domain::normalize_tengi_settings(runtime.tengi_settings);
    if (const auto value = scalars.notification_speed) {
        runtime.notification_speed = *value;
    }
    if (scalars.ui_scale && std::isfinite(*scalars.ui_scale)) {
        runtime.ui_scale = std::clamp(*scalars.ui_scale, 0.5f, 3.0f);
    }
    if (const auto value = scalars.domestics_player) {
        runtime.domestics.player = *value;
    }
    if (const auto value = scalars.domestics_force) {
        runtime.domestics.force = *value;
    }
    runtime.domestics_patch.multipliers = runtime.domestics;
    runtime.secondary_jewels = secondary_jewels;
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
    if (scalars.tech_city_visibility) {
        runtime.tech_city_visibility_enabled = *scalars.tech_city_visibility;
    }
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
    if (scalars.reinforcement_arrival) {
        runtime.reinforcement_arrival_enabled = *scalars.reinforcement_arrival;
    }
    if (const auto days = scalars.short_battle_cooldown_days) {
        runtime.short_battle_cooldown_days = *days;
    }
    if (const auto enabled = scalars.short_battle_cooldown_enabled) {
        runtime.short_battle_cooldown_enabled = *enabled;
    }
    if (scalars.troop_count_combat) {
        runtime.troop_count_combat_enabled = *scalars.troop_count_combat;
    }

    queue_legacy_configuration(runtime);
    request_trait_viewer(runtime.traits, runtime.features.get(domain::FeatureId::TraitViewer));
}

void tick_recurring_features(Runtime& runtime, std::uintptr_t module_base,
                             std::uintptr_t game_base, bool game_base_ready,
                             std::uintptr_t player, bool player_ready) {
    game::tick_recurring_features(runtime.recurring_features, module_base, game_base, game_base_ready,
                                  player, player_ready);
}

void tick_game(Runtime& runtime, std::uintptr_t game_module, std::uint64_t now_ms) {
    const platform::windows::RealClockScope real_clock;

    if (runtime.stopping) {
        return;
    }

    game::update_start_setting(runtime.start_setting, game_module, now_ms,
        runtime.features.get(domain::FeatureId::StartSetting));

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
    if (now_ms >= runtime.player_resources_next_ms) {
        runtime.player_resources_next_ms = now_ms + 250;
        runtime.player_resources = game::read_player_resources(runtime.game_base_pointer, game_base, player);
    }
    runtime.tengi_settings.force_quarterly = runtime.features.get(domain::FeatureId::InfTengi);
    if (runtime.config_loaded) {
        game::update_tengi(runtime.tengi, game_module, runtime.loop_player_ready ? player : 0,
                          now_ms, runtime.tengi_settings);
    }
    game::update_officer_roster(runtime.officer_roster, game_module);
    update_native_feature_candidates(runtime, game_module);
    if (runtime.config_loaded) {
        game::update_ai_officer_growth(runtime.ai_growth, game_module,
            runtime.loop_game_base_ready ? game_base : 0, runtime.loop_player_ready ? player : 0,
            runtime.epoch, now_ms, runtime.ai_growth_settings);
    }
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
    observe_legacy_player(runtime, now_ms);
    game::update_faction_lord_bonus(runtime.faction_lord_bonus, game_module, now_ms);
    constexpr std::array ROAD_FEATURES{domain::FeatureId::RoadBlock, domain::FeatureId::RoadBlock2};

    for (std::size_t index = 0; index < runtime.road_blocks.size(); ++index) {

        game::RoadBlock& road = runtime.road_blocks[index];

        // Routine zero maintenance is data-only. New-session capture and OFF
        // restoration belong to the one-group configuration scheduler.
        if (road.epoch == runtime.epoch && road.requested && runtime.features.get(ROAD_FEATURES[index])) {
            game::tick_road_block(road, game_module, index == 1, now_ms, runtime.epoch);
        }
    }
}

} // namespace s8::runtime
