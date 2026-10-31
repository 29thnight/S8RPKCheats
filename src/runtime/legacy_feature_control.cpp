#include "legacy_feature_control.h"
#include "runtime.h"
#include "../platform/windows/speed_clock.h"

namespace s8::runtime {
namespace {

using Group = domain::LegacyFeatureGroup;
using Phase = domain::LegacyFeaturePhase;
constexpr std::uint64_t RETRY_INTERVAL_MS = 1000;
constexpr std::uint64_t PLAYER_WARMUP_MS = 3000;
constexpr std::array<domain::FeatureId, domain::LEGACY_FEATURE_GROUP_COUNT> GROUP_FEATURES{
    domain::FeatureId::Count, domain::FeatureId::Count, domain::FeatureId::Count,
    domain::FeatureId::Count, domain::FeatureId::Count,
    domain::FeatureId::Loyalty, domain::FeatureId::HateCave, domain::FeatureId::Domestics,
    domain::FeatureId::Resonance, domain::FeatureId::FactionLordBonus,
    domain::FeatureId::RoadBlock, domain::FeatureId::RoadBlock2,
    domain::FeatureId::SkillCondition, domain::FeatureId::YumokCondition, domain::FeatureId::SangbyeongCondition,
    domain::FeatureId::BigCity, domain::FeatureId::BangmokCity,
    domain::FeatureId::NonggyeongCity, domain::FeatureId::SangeopCity,
    domain::FeatureId::AttitudeHack, domain::FeatureId::FastRelationship,
    domain::FeatureId::InfiniteGift, domain::FeatureId::InfiniteTalk,
    domain::FeatureId::InfiniteDuel, domain::FeatureId::InfiniteDebate, domain::FeatureId::MarriageApplied,
    domain::FeatureId::SpeedHack};

constexpr std::size_t index_of(Group group) {
    return static_cast<std::size_t>(group);
}

constexpr bool city_group(Group group) {
    return group >= Group::BigCity && group <= Group::CommerceCity;
}

bool any_jewels(const std::optional<domain::SecondaryJewelMask>& mask) {

    if (mask) {
        for (const std::uint64_t word : mask->words) {
            if (word != 0) {
                return true;
            }
        }
    }
    return false;
}

bool desired(const Runtime& runtime, Group group) {

    switch (group) {
    case Group::SecondaryJewels: return any_jewels(runtime.secondary_jewels);
    case Group::TechCityVisibility: return runtime.tech_city_visibility_enabled;
    case Group::ReinforcementArrival: return runtime.reinforcement_arrival_enabled;
    case Group::ShortBattleCooldown: return runtime.short_battle_cooldown_enabled;
    case Group::TroopCountCombat: return runtime.troop_count_combat_enabled;
    default: return runtime.features.get(GROUP_FEATURES[index_of(group)]);
    }
}

bool applied(const Runtime& runtime, Group group) {

    if (group >= Group::RoadBlock && group <= Group::RoadBlock2) {
        return runtime.road_blocks[index_of(group) - index_of(Group::RoadBlock)].applied;
    }
    if (group >= Group::SkillCondition && group <= Group::SangbyeongCondition) {
        return runtime.skill_condition_applied[index_of(group) - index_of(Group::SkillCondition)];
    }
    if (city_group(group)) {
        return runtime.city_conversions[index_of(group) - index_of(Group::BigCity)].applied;
    }
    switch (group) {
    case Group::SecondaryJewels: return any_jewels(runtime.legacy_features.applied_secondary_jewels);
    case Group::TechCityVisibility: return runtime.tech_city_visibility.applied;
    case Group::ReinforcementArrival: return runtime.reinforcement_arrival.applied;
    case Group::ShortBattleCooldown: return runtime.short_battle_cooldown.applied;
    case Group::TroopCountCombat: return runtime.troop_count_combat.applied;
    case Group::Loyalty: return runtime.instant_loyalty.applied;
    case Group::LoveHate: return runtime.love_hate.affinity_applied || runtime.love_hate.relationship_applied;
    case Group::Domestics: return runtime.domestics_patch.applied;
    case Group::Resonance: return runtime.instant_resonance.select_applied || runtime.instant_resonance.getter_applied;
    case Group::FactionLordBonus:
        return runtime.faction_lord_bonus.enabled || runtime.faction_lord_bonus.names_applied ||
            !runtime.faction_lord_bonus.assigned.empty();
    case Group::Attitude: return runtime.instant_attitude_config_applied;
    case Group::Relationship: return runtime.fast_relationship.applied;
    case Group::Gift: return runtime.infinite_gift.applied;
    case Group::Talk: return runtime.infinite_talk.applied;
    case Group::Duel: return runtime.infinite_duel_debate.applied && runtime.infinite_duel_debate.duel;
    case Group::Debate: return runtime.infinite_duel_debate.applied && runtime.infinite_duel_debate.debate;
    case Group::Marriage: return runtime.marriage_condition.applied;
    case Group::SpeedClock: return runtime.legacy_features.applied_speed_enabled;
    default: return false;
    }
}

constexpr bool requires_player_warmup(Group group) {
    return group == Group::Domestics || (group >= Group::FactionLordBonus && group <= Group::Marriage);
}

bool apply_group(Runtime& runtime, std::uintptr_t module_base, Group group) {

    const bool enabled = desired(runtime, group);
    const std::span<const std::byte> image = runtime.module_image;

    if (group >= Group::RoadBlock && group <= Group::RoadBlock2) {

        const std::size_t index = index_of(group) - index_of(Group::RoadBlock);

        return game::set_road_block(runtime.road_blocks[index], module_base, index == 1, enabled, runtime.epoch);
    }
    if (group >= Group::SkillCondition && group <= Group::SangbyeongCondition) {

        const std::size_t index = index_of(group) - index_of(Group::SkillCondition);
        const bool succeeded = game::set_skill_condition(module_base, static_cast<game::SkillConditionKind>(index), enabled);

        if (succeeded) {
            runtime.skill_condition_applied[index] = enabled;
        }
        return succeeded;
    }
    switch (group) {
    case Group::SecondaryJewels:
        if (game::set_secondary_jewels(runtime.secondary_jewels.value_or(domain::SecondaryJewelMask{}),
                module_base, runtime.game_base_pointer)) {
            runtime.legacy_features.applied_secondary_jewels = runtime.secondary_jewels;
            return true;
        }
        return false;
    case Group::TechCityVisibility:
        return game::set_tech_city_visibility(runtime.tech_city_visibility, module_base, enabled);
    case Group::ReinforcementArrival:
        return game::set_reinforcement_arrival(runtime.reinforcement_arrival, module_base, enabled);
    case Group::ShortBattleCooldown:
        return game::set_short_battle_cooldown(runtime.short_battle_cooldown, module_base, enabled,
            runtime.short_battle_cooldown_days);
    case Group::TroopCountCombat:
        return game::set_troop_count_combat(runtime.troop_count_combat, module_base, enabled);
    case Group::Loyalty:
        return game::set_instant_loyalty(runtime.instant_loyalty, module_base, image, enabled);
    case Group::LoveHate:
        return game::set_love_hate(runtime.love_hate, module_base, image, enabled);
    case Group::Domestics:
        runtime.domestics_patch.multipliers = runtime.domestics;
        return game::set_domestics(runtime.domestics_patch, module_base, image, enabled);
    case Group::Resonance:
        return game::set_instant_resonance(runtime.instant_resonance, module_base, image, enabled);
    case Group::FactionLordBonus:
        return game::set_faction_lord_bonus(runtime.faction_lord_bonus, module_base, enabled);
    case Group::Attitude: {
        const bool succeeded = game::set_instant_attitude(runtime.instant_attitude, module_base, image, enabled);

        if (succeeded) {
            runtime.instant_attitude_config_applied = enabled;
        }
        return succeeded;
    }
    case Group::Relationship:
        return game::set_fast_relationship(runtime.fast_relationship, module_base, image, enabled);
    case Group::Gift:
        return game::set_infinite_gift(runtime.infinite_gift, module_base, image, enabled);
    case Group::Talk:
        return game::set_infinite_talk(runtime.infinite_talk, module_base, image, enabled);
    case Group::Duel:
        return game::set_infinite_duel_debate(runtime.infinite_duel_debate, module_base, image,
            game::DuelDebateFeature::Duel, enabled);
    case Group::Debate:
        return game::set_infinite_duel_debate(runtime.infinite_duel_debate, module_base, image,
            game::DuelDebateFeature::Debate, enabled);
    case Group::Marriage:
        return game::set_marriage_condition(runtime.marriage_condition, module_base, image, enabled);
    case Group::SpeedClock: {
        const std::expected<void, int> configured = platform::windows::configure_speed_clock(
            {enabled, runtime.speed_multiplier});

        runtime.speed_clock_configured = true;
        runtime.speed_clock_error = configured ? 0 : configured.error();
        if (configured) {
            runtime.legacy_features.applied_speed_enabled = enabled;
        }
        return configured.has_value();
    }
    default: return false;
    }
}

enum class ApplyResult { Complete, Pending, Failed };

ApplyResult apply_city_group(Runtime& runtime, std::uintptr_t module_base, Group group, std::uint64_t now_ms) {

    const std::size_t index = index_of(group) - index_of(Group::BigCity);
    game::CityConversion& conversion = runtime.city_conversions[index];
    const bool enabled = desired(runtime, group);

    if (!enabled) {
        return game::stop_city_conversion(conversion, module_base, runtime.epoch)
            ? ApplyResult::Complete : ApplyResult::Failed;
    }
    if (conversion.phase != game::CityConversionPhase::Idle) {

        const game::CityConversionPhase previous = conversion.phase;

        game::tick_city_conversion(conversion, runtime.epoch, now_ms);
        if (conversion.phase == game::CityConversionPhase::Idle) {
            return conversion.applied ? ApplyResult::Complete : ApplyResult::Pending;
        }
        return conversion.phase != previous ? ApplyResult::Pending : ApplyResult::Failed;
    }
    // A preceding OFF may have restored only part of the table. Re-run the
    // two ON phases even when its restoration marker is still set.
    return game::request_city_conversion(conversion, static_cast<game::CityConversionKind>(index), true,
        runtime.epoch, module_base, now_ms) ? ApplyResult::Pending : ApplyResult::Failed;
}

void synchronize_desired_fields(Runtime& runtime, domain::FeatureId feature, bool enabled) {

    switch (feature) {
    case domain::FeatureId::InfiniteGift: runtime.infinite_gift_enabled = enabled; break;
    case domain::FeatureId::InfiniteTalk: runtime.infinite_talk_enabled = enabled; break;
    case domain::FeatureId::InfiniteDuel: runtime.infinite_duel_enabled = enabled; break;
    case domain::FeatureId::InfiniteDebate: runtime.infinite_debate_enabled = enabled; break;
    case domain::FeatureId::AttitudeHack: runtime.instant_attitude_enabled = enabled; break;
    case domain::FeatureId::FastRelationship: runtime.fast_relationship_enabled = enabled; break;
    case domain::FeatureId::MarriageApplied: runtime.marriage_enabled = enabled; break;
    default: break;
    }
}

} // namespace

bool request_legacy_feature(Runtime& runtime, domain::FeatureId feature, bool enabled) {

    const std::optional<Group> group = domain::legacy_feature_group(feature);

    if (!group) {
        return false;
    }
    runtime.features.set(feature, enabled);
    synchronize_desired_fields(runtime, feature, enabled);
    request_legacy_group(runtime, *group, true);
    return true;
}

void request_legacy_group(Runtime& runtime, Group group, bool manual) {

    const std::size_t index = index_of(group);
    LegacyFeatureControl& control = runtime.legacy_features;

    if (index >= domain::LEGACY_FEATURE_GROUP_COUNT) {
        return;
    }
    control.pending[index] = true;
    control.manual[index] = manual;
    control.announce[index] = manual;
    control.retry_after_ms[index] = 0;
    control.statuses[index] = {Phase::Pending, applied(runtime, group)};
}

void queue_legacy_configuration(Runtime& runtime) {

    for (std::size_t index = 0; index < domain::LEGACY_FEATURE_GROUP_COUNT; ++index) {

        const Group group = static_cast<Group>(index);

        if (desired(runtime, group) || applied(runtime, group) || runtime.legacy_features.pending[index] ||
            (group == Group::SecondaryJewels && runtime.secondary_jewels) ||
            (group == Group::SpeedClock && !runtime.speed_clock_configured)) {
            request_legacy_group(runtime, group, false);
        }
    }
}

void change_legacy_feature_epoch(Runtime& runtime) {

    runtime.config_first_game_base_ms = 0;
    runtime.config_first_player_ms = 0;
    runtime.legacy_features.manual.fill(false);
    runtime.city_conversions = {};
    runtime.city_config_applied.fill(false);

    for (std::size_t index = 0; index < domain::LEGACY_FEATURE_GROUP_COUNT; ++index) {

        const Group group = static_cast<Group>(index);
        const bool session_data = group == Group::SecondaryJewels ||
            (group >= Group::FactionLordBonus && group <= Group::CommerceCity);

        if (session_data && (desired(runtime, group) || applied(runtime, group) ||
            runtime.legacy_features.pending[index])) {
            request_legacy_group(runtime, group, false);
        }
    }
}

void observe_legacy_player(Runtime& runtime, std::uint64_t now_ms) {

    if (!runtime.loop_game_base_ready) {
        runtime.config_first_game_base_ms = 0;
        runtime.config_first_player_ms = 0;
        return;
    }
    if (!runtime.config_first_game_base_ms) {
        runtime.config_first_game_base_ms = now_ms;
    }
    if (!runtime.loop_player_ready) {
        runtime.config_first_player_ms = 0;
    } else if (!runtime.config_first_player_ms) {
        runtime.config_first_player_ms = now_ms;
    }
}

bool reconcile_legacy_features(Runtime& runtime, std::uintptr_t module_base, std::uint64_t now_ms) {

    LegacyFeatureControl& control = runtime.legacy_features;

    if (!runtime.config_loaded || runtime.stopping) {
        return false;
    }
    for (std::size_t offset = 0; offset < domain::LEGACY_FEATURE_GROUP_COUNT; ++offset) {

        const std::size_t index = (control.next_group + offset) % domain::LEGACY_FEATURE_GROUP_COUNT;
        const Group group = static_cast<Group>(index);
        domain::LegacyFeatureStatus& status = control.statuses[index];

        if (!control.pending[index] || now_ms < control.retry_after_ms[index]) {
            continue;
        }
        if (desired(runtime, group) && requires_player_warmup(group) && !control.manual[index] &&
            (!runtime.loop_player_ready || !runtime.config_first_player_ms ||
             now_ms - runtime.config_first_player_ms < PLAYER_WARMUP_MS)) {
            status.phase = Phase::WaitingForPlayer;
            continue;
        }
        if (city_group(group)) {

            const game::CityConversion& conversion = runtime.city_conversions[index - index_of(Group::BigCity)];

            if (desired(runtime, group) && conversion.phase != game::CityConversionPhase::Idle &&
                now_ms < conversion.deadline_ms) {
                continue;
            }
        }

        const Phase previous = status.phase;
        const ApplyResult result = city_group(group) ? apply_city_group(runtime, module_base, group, now_ms) :
            (apply_group(runtime, module_base, group) ? ApplyResult::Complete : ApplyResult::Failed);

        control.next_group = (index + 1) % domain::LEGACY_FEATURE_GROUP_COUNT;
        status.applied = applied(runtime, group);
        if (result == ApplyResult::Complete) {
            control.pending[index] = false;
            control.manual[index] = false;
            status.phase = desired(runtime, group) ? Phase::Applied : Phase::Disabled;
            if (control.announce[index] || previous == Phase::Retrying) {
                control.notice = LegacyFeatureNotice{group, true};
            }
            control.announce[index] = false;
        } else if (result == ApplyResult::Pending) {
            status.phase = Phase::Pending;
            control.retry_after_ms[index] = runtime.city_conversions[index - index_of(Group::BigCity)].deadline_ms;
        } else {
            status.phase = Phase::Retrying;
            control.retry_after_ms[index] = now_ms + RETRY_INTERVAL_MS;
            if (previous != Phase::Retrying || control.announce[index]) {
                control.notice = LegacyFeatureNotice{group, false};
            }
            control.announce[index] = false;
        }
        return true;
    }
    return false;
}

} // namespace s8::runtime
