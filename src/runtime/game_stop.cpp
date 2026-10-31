#include "runtime.h"

#include "../game/game_base.h"
#include "../game/stratagem5_owner.h"
#include "../platform/windows/speed_clock.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <cstddef>
#include <cstdint>
#include <optional>

namespace s8::runtime {
namespace {

enum class StopStep : std::size_t {
    Traits,
    CityManagement,
    CorpsDeployment,
    Native,
    Tengi,
    AiGrowth,
    Gift,
    Talk,
    Duel,
    Debate,
    Relationship,
    Marriage,
    Attitude,
    Loyalty,
    LoveHate,
    Resonance,
    Domestics,
    DomesticReward,
    Cooldown,
    TroopCount,
    Reinforcement,
    TechVisibility,
    SelfHeal,
    Dongto,
    Celestial,
    Catapult,
    Terrain,
    Building,
    RoadFirst,
    RoadSecond,
    Manbyeong,
    Yumok,
    Sangbyeong,
    SiegeFirst,
    SiegeSecond,
    LordBonus,
    BattleMap,
    Jewels,
    SecondaryJewels,
    AppointmentLimit,
    Weather,
    BigCity,
    GrazingCity,
    FarmingCity,
    CommerceCity,
    StratagemModel,
    StratagemCamp,
    StratagemCount,
    StratagemData,
    Count
};

constexpr std::size_t STOP_STEP_COUNT = static_cast<std::size_t>(StopStep::Count);
static_assert(STOP_STEP_COUNT < 64);
constexpr std::uint64_t ALL_STOPPED = (std::uint64_t{1} << STOP_STEP_COUNT) - 1;

domain::Result<void> stop_result(bool succeeded, StopStep step) {

    if (succeeded) {
        return {};
    }

    return std::unexpected(domain::Error{domain::ErrorCode::PlatformFailure,
        0x1000u + static_cast<std::uint32_t>(step)});
}

bool needed(const Runtime& runtime, StopStep step) {

    const domain::FeatureFlags& flags = runtime.features;

    switch (step) {
    case StopStep::Traits: return true;
    case StopStep::CityManagement: return true;
    case StopStep::CorpsDeployment: return true;
    case StopStep::Native: return true;
    case StopStep::Tengi: return runtime.tengi.module_base != 0;
    case StopStep::AiGrowth: return runtime.ai_growth_settings.enabled;
    case StopStep::Gift: return runtime.infinite_gift.applied;
    case StopStep::Talk: return runtime.infinite_talk.applied;
    case StopStep::Duel: return runtime.infinite_duel_debate.duel;
    case StopStep::Debate: return runtime.infinite_duel_debate.debate;
    case StopStep::Relationship: return runtime.fast_relationship.applied;
    case StopStep::Marriage: return runtime.marriage_condition.applied;
    case StopStep::Attitude: return runtime.instant_attitude.address != 0;
    case StopStep::Loyalty: return runtime.instant_loyalty.applied;
    case StopStep::LoveHate: return runtime.love_hate.affinity_applied || runtime.love_hate.relationship_applied;
    case StopStep::Resonance: return runtime.instant_resonance.select_applied || runtime.instant_resonance.getter_applied;
    case StopStep::Domestics: return runtime.domestics_patch.applied;
    case StopStep::DomesticReward: return runtime.domestic_reward.applied;
    case StopStep::Cooldown: return runtime.short_battle_cooldown.applied;
    case StopStep::TroopCount: return runtime.troop_count_combat.applied;
    case StopStep::Reinforcement: return runtime.reinforcement_arrival.applied;
    case StopStep::TechVisibility: return runtime.tech_city_visibility.applied;
    case StopStep::SelfHeal: return flags.get(domain::FeatureId::SelfHeal);
    case StopStep::Dongto: return flags.get(domain::FeatureId::Dongto);
    case StopStep::Celestial: return flags.get(domain::FeatureId::Celestial);
    case StopStep::Catapult: return flags.get(domain::FeatureId::Catapult);
    case StopStep::Terrain: return flags.get(domain::FeatureId::TerrainIgnore);
    case StopStep::Building: return flags.get(domain::FeatureId::DefBuilding);
    case StopStep::RoadFirst: return runtime.road_blocks[0].applied || runtime.road_blocks[0].requested;
    case StopStep::RoadSecond: return runtime.road_blocks[1].applied || runtime.road_blocks[1].requested;
    case StopStep::Manbyeong: return runtime.skill_condition_applied[0];
    case StopStep::Yumok: return runtime.skill_condition_applied[1];
    case StopStep::Sangbyeong: return runtime.skill_condition_applied[2];
    case StopStep::SiegeFirst: return runtime.siege[0].move_applied || runtime.siege[0].terrain_applied;
    case StopStep::SiegeSecond: return runtime.siege[1].move_applied || runtime.siege[1].terrain_applied;
    case StopStep::LordBonus: return runtime.faction_lord_bonus.enabled || runtime.faction_lord_bonus.names_applied ||
        !runtime.faction_lord_bonus.assigned.empty();
    case StopStep::BattleMap: return runtime.battle_map_shuffle.active;
    case StopStep::Jewels: return runtime.jewel_open.preferred;
    case StopStep::SecondaryJewels: return runtime.secondary_jewels.has_value();
    case StopStep::AppointmentLimit: return runtime.mission_appointment_limit.managed;
    case StopStep::Weather: return flags.get(domain::FeatureId::WeatherSkillSimple) ||
        flags.get(domain::FeatureId::WeatherSkillComplex);
    case StopStep::BigCity:
    case StopStep::GrazingCity:
    case StopStep::FarmingCity:
    case StopStep::CommerceCity: {

        const game::CityConversion& city = runtime.city_conversions[
            static_cast<std::size_t>(step) - static_cast<std::size_t>(StopStep::BigCity)];

        return city.applied || city.phase != game::CityConversionPhase::Idle;
    }
    case StopStep::StratagemCount: return runtime.stratagem5_count.applied.has_value();
    case StopStep::StratagemModel: return runtime.stratagem5_model.applied.has_value();
    case StopStep::StratagemCamp: return runtime.stratagem5_camp.slot.has_value();
    case StopStep::StratagemData: return runtime.stratagem5.runtime.session.applied_table.has_value() ||
        runtime.stratagem5.runtime.session.last_restore == game::Stratagem5RestoreStatus::Failed;
    default: return false;
    }
}

domain::Result<void> perform_stop(Runtime& runtime, std::uintptr_t module_base, StopStep step) {

    switch (step) {
    case StopStep::CityManagement:
        return stop_result(game::stop_city_management(runtime.city_management, module_base, runtime.epoch), step);
    case StopStep::CorpsDeployment:
        return stop_result(game::stop_corps_deployment(*runtime.corps_deployment, module_base, runtime.epoch), step);
    case StopStep::Traits:
        return stop_trait_control(runtime.traits, module_base, runtime.loop_game_base, runtime.epoch.value);
    case StopStep::Native:
        return stop_native_features(runtime, module_base);
    case StopStep::Tengi: {

        const int result = game::stop_tengi(runtime.tengi);

        return result == 0 ? domain::Result<void>{} : std::unexpected(domain::Error{
            domain::ErrorCode::PlatformFailure, static_cast<std::uint32_t>(result)});
    }
    case StopStep::AiGrowth: {

        domain::AiGrowthSettings settings = runtime.ai_growth_settings;

        return stop_result(game::set_ai_growth_enabled(module_base, settings, false), step);
    }
    case StopStep::Gift:
        return stop_result(game::set_infinite_gift(runtime.infinite_gift, module_base, {}, false), step);
    case StopStep::Talk:
        return stop_result(game::set_infinite_talk(runtime.infinite_talk, module_base, {}, false), step);
    case StopStep::Duel:
        return stop_result(game::set_infinite_duel_debate(runtime.infinite_duel_debate, module_base, {},
            game::DuelDebateFeature::Duel, false), step);
    case StopStep::Debate:
        return stop_result(game::set_infinite_duel_debate(runtime.infinite_duel_debate, module_base, {},
            game::DuelDebateFeature::Debate, false), step);
    case StopStep::Relationship:
        return stop_result(game::set_fast_relationship(runtime.fast_relationship, module_base, {}, false), step);
    case StopStep::Marriage:
        return stop_result(game::set_marriage_condition(runtime.marriage_condition, module_base, {}, false), step);
    case StopStep::Attitude:
        return stop_result(game::set_instant_attitude(runtime.instant_attitude, module_base, {}, false), step);
    case StopStep::Loyalty:
        return stop_result(game::set_instant_loyalty(runtime.instant_loyalty, module_base, {}, false), step);
    case StopStep::LoveHate:
        return stop_result(game::set_love_hate(runtime.love_hate, module_base, {}, false), step);
    case StopStep::Resonance:
        return stop_result(game::set_instant_resonance(runtime.instant_resonance, module_base, {}, false), step);
    case StopStep::Domestics:
        return stop_result(game::set_domestics(runtime.domestics_patch, module_base, {}, false), step);
    case StopStep::DomesticReward:
        return stop_result(game::set_domestic_reward(runtime.domestic_reward, module_base, domain::DomesticRewardMode::Off), step);
    case StopStep::Cooldown:
        return stop_result(game::set_short_battle_cooldown(runtime.short_battle_cooldown, module_base, false,
            runtime.short_battle_cooldown_days), step);
    case StopStep::TroopCount:
        return stop_result(game::set_troop_count_combat(runtime.troop_count_combat, module_base, false), step);
    case StopStep::Reinforcement:
        return stop_result(game::set_reinforcement_arrival(runtime.reinforcement_arrival, module_base, false), step);
    case StopStep::TechVisibility:
        return stop_result(game::set_tech_city_visibility(runtime.tech_city_visibility, module_base, false), step);
    case StopStep::SelfHeal:
        return stop_result(game::set_self_heal(module_base, false, runtime.self_heal_settings), step);
    case StopStep::Dongto:
        return stop_result(game::set_dongto(module_base, false, runtime.dongto_settings), step);
    case StopStep::Celestial:
        return stop_result(game::set_celestial(module_base, false, runtime.celestial_settings), step);
    case StopStep::Catapult:
        return stop_result(game::set_catapult(module_base, false, runtime.catapult_settings), step);
    case StopStep::Terrain:
        return stop_result(game::set_terrain_tactics(module_base, false, runtime.terrain_tactics_settings), step);
    case StopStep::Building:
        return stop_result(game::set_def_building(module_base, false, runtime.def_building_settings), step);
    case StopStep::RoadFirst:
    case StopStep::RoadSecond: {

        const bool second = step == StopStep::RoadSecond;

        return stop_result(game::set_road_block(runtime.road_blocks[second], module_base, second, false, runtime.epoch), step);
    }
    case StopStep::Manbyeong:
    case StopStep::Yumok:
    case StopStep::Sangbyeong: {

        const std::size_t index = static_cast<std::size_t>(step) - static_cast<std::size_t>(StopStep::Manbyeong);

        return stop_result(game::set_skill_condition(module_base, static_cast<game::SkillConditionKind>(index), false), step);
    }
    case StopStep::SiegeFirst:
    case StopStep::SiegeSecond:
        return stop_result(game::disable_siege(runtime.siege[step == StopStep::SiegeSecond], module_base), step);
    case StopStep::LordBonus:
        return stop_result(game::set_faction_lord_bonus(runtime.faction_lord_bonus, module_base, false), step);
    case StopStep::BattleMap:
        return stop_result(game::disable_battle_map_shuffle(runtime.battle_map_shuffle), step);
    case StopStep::Jewels: {

        const std::uintptr_t root = game::read_game_base(runtime.game_base_pointer);

        if (!root || root != runtime.loop_game_base) {
            return {};
        }

        return stop_result(game::set_all_jewels_open(runtime.jewel_open, root, false), step);
    }
    case StopStep::SecondaryJewels:
        return stop_result(game::set_secondary_jewels(domain::SecondaryJewelMask{}, module_base,
            runtime.game_base_pointer), step);
    case StopStep::AppointmentLimit:
        return stop_result(game::restore_mission_appointment_limit(runtime.mission_appointment_limit, module_base), step);
    case StopStep::Weather:
        return stop_result(game::update_battle_weather(runtime.battle_weather, module_base, GetTickCount64(),
            false, false, true, false), step);
    case StopStep::BigCity:
    case StopStep::GrazingCity:
    case StopStep::FarmingCity:
    case StopStep::CommerceCity: {

        const std::size_t index = static_cast<std::size_t>(step) - static_cast<std::size_t>(StopStep::BigCity);

        return stop_result(game::stop_city_conversion(runtime.city_conversions[index], module_base, runtime.epoch), step);
    }
    case StopStep::StratagemCount: {

        const std::optional<std::uintptr_t> attack = game::is_stratagem5_owner(runtime.stratagem5_attack_owner, 0)
            ? std::optional<std::uintptr_t>{runtime.stratagem5_attack_owner} : std::nullopt;
        const std::optional<std::uintptr_t> defense = game::is_stratagem5_owner(runtime.stratagem5_defense_owner, 1)
            ? std::optional<std::uintptr_t>{runtime.stratagem5_defense_owner} : std::nullopt;

        return stop_result(game::disable_stratagem5_count(runtime.stratagem5_count, runtime.epoch, attack, defense), step);
    }
    case StopStep::StratagemModel:
        return stop_result(game::restore_stratagem5_model(runtime.stratagem5_model, runtime.epoch), step);
    case StopStep::StratagemCamp:
        return stop_result(game::restore_stratagem5_camp(runtime.stratagem5_camp, runtime.epoch), step);
    case StopStep::StratagemData:
        return stop_result(game::stop_stratagem5(runtime.stratagem5, runtime.epoch), step);
    default:
        return {};
    }
}

} // namespace

domain::Result<void> stop_game(Runtime& runtime, std::uintptr_t game_module) {

    const platform::windows::RealClockScope real_clock;

    if (runtime.stopped) {
        return {};
    }

    const std::uintptr_t current_root = game::read_game_base(runtime.game_base_pointer);

    if (runtime.session_game_base != 0 && current_root != runtime.session_game_base) {
        // Shutdown can span several window messages while a scenario unloads.
        // Invalidate object-bound backups before any later restore attempt.
        change_game_session(runtime);
        runtime.session_game_base = current_root;
        runtime.session_player = 0;
    }

    if (!runtime.stopping) {
        runtime.stop_snapshot = config_snapshot(runtime);
        runtime.stopping = true;
        runtime.pending_trait_change.reset();
        runtime.pending_annual_assignment = 0;
        runtime.ai_growth.running = false;
        runtime.auto_affinity = {};
        runtime.officer_roster.requested = false;
        runtime.undiscovered_ronin = {};
        runtime.special_assignment_notices.reset();
        game::stop_officer_tools(*runtime.officer_tools);
        game::reset_officer_notifications(*runtime.officer_notices, runtime.epoch);
        runtime.specialty_assignment_requested = false;
        runtime.stratagem5_lifecycle.effective.store(false);
        runtime.stratagem5_lifecycle.skip_current_battle.store(true);
        runtime.stratagem5_count.requested = false;
        game::set_tech_zero(runtime.tech_zero, false);

        if (runtime.special_assignment.summary.phase == domain::SpecialAssignmentPhase::Scanning ||
            runtime.special_assignment.summary.phase == domain::SpecialAssignmentPhase::Committing) {
            runtime.special_assignment.summary.phase = domain::SpecialAssignmentPhase::Cancelled;
        }

        // Acceleration ends immediately; retained 1x detours preserve the game
        // clock's accumulated offset while the remaining effects are restored.
        platform::windows::stop_speed_clock();
    }

    for (std::size_t offset = 0; offset < STOP_STEP_COUNT; ++offset) {

        const std::size_t index = (runtime.next_stop_step + offset) % STOP_STEP_COUNT;
        const std::uint64_t bit = std::uint64_t{1} << index;
        const StopStep step = static_cast<StopStep>(index);

        if ((runtime.completed_stop_steps & bit) != 0) {
            continue;
        }

        if ((runtime.failed_stop_steps & bit) == 0 && !needed(runtime, step)) {
            runtime.completed_stop_steps |= bit;
            continue;
        }

        const domain::Result<void> result = perform_stop(runtime, game_module, step);

        if (!result) {
            // Layered backups must unwind in order. In particular, restoring
            // COUNT before a failed MODEL retry would discard the real value.
            runtime.next_stop_step = index;
            runtime.failed_stop_steps |= bit;
            return result;
        }

        runtime.next_stop_step = (index + 1) % STOP_STEP_COUNT;
        runtime.failed_stop_steps &= ~bit;
        runtime.completed_stop_steps |= bit;

        if (runtime.completed_stop_steps != ALL_STOPPED) {
            return std::unexpected(domain::Error{domain::ErrorCode::QueueFull});
        }

        break;
    }

    runtime.stopped = runtime.completed_stop_steps == ALL_STOPPED;
    return runtime.stopped ? domain::Result<void>{} : std::unexpected(domain::Error{domain::ErrorCode::QueueFull});
}

} // namespace s8::runtime
