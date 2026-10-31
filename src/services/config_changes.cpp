#include "config_changes.h"
#include "tengi_config_keys.h"

#include <array>
#include <string>

namespace s8::services {

std::vector<ConfigChange> config_changes(const domain::ConfigSnapshot& snapshot) {
    struct FeatureKey { domain::FeatureId id; std::string_view key; };
    constexpr std::array FEATURES{
        FeatureKey{domain::FeatureId::InfiniteAp, "bInfiniteAP"},
        FeatureKey{domain::FeatureId::FastJewel, "bFastJewel"},
        FeatureKey{domain::FeatureId::BigCity, "bBigCity"},
        FeatureKey{domain::FeatureId::BangmokCity, "bBangmokCity"},
        FeatureKey{domain::FeatureId::NonggyeongCity, "bNonggyeongCity"},
        FeatureKey{domain::FeatureId::SangeopCity, "bSangeopCity"},
        FeatureKey{domain::FeatureId::AttitudeHack, "bAttitudeHack"},
        FeatureKey{domain::FeatureId::InfiniteBanquet, "bInfiniteBanquet"},
        FeatureKey{domain::FeatureId::InfiniteGift, "bInfiniteGift"},
        FeatureKey{domain::FeatureId::InfiniteTalk, "bInfiniteTalk"},
        FeatureKey{domain::FeatureId::InfiniteDuel, "bInfiniteDuel"},
        FeatureKey{domain::FeatureId::InfiniteDebate, "bInfiniteDebate"},
        FeatureKey{domain::FeatureId::InfiniteMediation, "bInfiniteMediation"},
        FeatureKey{domain::FeatureId::FastRelationship, "bFastRelationship"},
        FeatureKey{domain::FeatureId::AutoAffinityGrowth, "bAutoAffinityGrowth"},
        FeatureKey{domain::FeatureId::MarriageApplied, "marriageApplied"},
        FeatureKey{domain::FeatureId::SelfHeal, "bSelfHeal"},
        FeatureKey{domain::FeatureId::Dongto, "bDongto"},
        FeatureKey{domain::FeatureId::Celestial, "bCelestial"},
        FeatureKey{domain::FeatureId::Catapult, "bCatapult"},
        FeatureKey{domain::FeatureId::TerrainIgnore, "bTerrainIgnore"},
        FeatureKey{domain::FeatureId::DefBuilding, "bDefBuilding"},
        FeatureKey{domain::FeatureId::RoadBlock, "bRoadBlock"},
        FeatureKey{domain::FeatureId::RoadBlock2, "bRoadBlock2"},
        FeatureKey{domain::FeatureId::SkillCondition, "bSkillCondition"},
        FeatureKey{domain::FeatureId::YumokCondition, "bYumokCondition"},
        FeatureKey{domain::FeatureId::SangbyeongCondition, "bSangbyeongCondition"},
        FeatureKey{domain::FeatureId::DateAlways15, "bDateAlways15"},
        FeatureKey{domain::FeatureId::DateDynamic, "bDateDynamic"},
        FeatureKey{domain::FeatureId::SkipDaysEnabled, "bSkipDaysEnabled"},
        FeatureKey{domain::FeatureId::WeatherSkillSimple, "bWeatherSkillSimple"},
        FeatureKey{domain::FeatureId::WeatherSkillComplex, "bWeatherSkillComplex"},
        FeatureKey{domain::FeatureId::TerrainAbilityAtkDef, "bTerrainAbilityAtkDef"},
        FeatureKey{domain::FeatureId::TerrainAbilityAll, "bTerrainAbilityAll"},
        FeatureKey{domain::FeatureId::AutoStatUp99, "bAutoStatUp99"},
        FeatureKey{domain::FeatureId::AllAggressive, "bAllAggressive"},
        FeatureKey{domain::FeatureId::ZeroInfamy, "bZeroInfamy"},
        FeatureKey{domain::FeatureId::SiegeWarfare, "bSiegeWarfare"},
        FeatureKey{domain::FeatureId::SiegeWarfare2, "bSiegeWarfare2"},
        FeatureKey{domain::FeatureId::InfiniteTavernRequests, "bInfiniteTavernRequests"},
        FeatureKey{domain::FeatureId::AutoFillSpecialties, "bAutoFillSpecialties"},
        FeatureKey{domain::FeatureId::BattleMapShuffle, "bBattleMapShuffle"},
        FeatureKey{domain::FeatureId::BattleUnit, "bBattleUnit"},
        FeatureKey{domain::FeatureId::FactionLordBonus, "bFactionLordBonus"},
        FeatureKey{domain::FeatureId::UndiscoveredToRonin, "bUndiscoveredToRonin"},
        FeatureKey{domain::FeatureId::Loyalty, "bLoyalty"},
        FeatureKey{domain::FeatureId::HateCave, "bHateCave"},
        FeatureKey{domain::FeatureId::Resonance, "bResonance"},
        FeatureKey{domain::FeatureId::TechZero, "bTechZero"},
        FeatureKey{domain::FeatureId::StartSetting, "bStartSetting"},
        FeatureKey{domain::FeatureId::FileLog, "bFileLog"},
        FeatureKey{domain::FeatureId::MonitorRonin, "bMonitorRonin"},
        FeatureKey{domain::FeatureId::SpeedHack, "bSpeedHack"},
        FeatureKey{domain::FeatureId::InfTengi, "bInfTengi"},
        FeatureKey{domain::FeatureId::AutoLoadMenu, "bAutoLoadMenu"},
        FeatureKey{domain::FeatureId::ShowWidgetNotif, "bShowWidgetNotif"},
        FeatureKey{domain::FeatureId::ShowWidgetHero, "bShowWidgetHero"},
        FeatureKey{domain::FeatureId::ShowWidgetAllOfficers, "bShowWidgetAllOfficers"},
        FeatureKey{domain::FeatureId::Domestics, "bDomestics"},
        FeatureKey{domain::FeatureId::HealLv1Self, "bHealLv1_Self"},
        FeatureKey{domain::FeatureId::HealLv2Self, "bHealLv2_Self"},
        FeatureKey{domain::FeatureId::HealLv3Self, "bHealLv3_Self"},
    };
    std::vector<ConfigChange> changes;
    changes.reserve(FEATURES.size() + 128 + 12 * 11 + domain::TENGI_EVENT_COUNT * 3);
    for (const auto& feature : FEATURES) {
        changes.push_back({feature.key, snapshot.features.get(feature.id)});
    }
    changes.push_back({"bShortBattleCooldownEnabled", snapshot.short_battle_cooldown_enabled});
    changes.push_back({"nSkipDays", snapshot.skip_days});
    changes.push_back({"iSiegeHealRate", snapshot.siege_heal_rate});
    changes.push_back({"bAllJewelsOpen", snapshot.all_jewels_open});
    changes.push_back({"fDomesticsPlayer", snapshot.domestics.player});
    changes.push_back({"fDomesticsForce", snapshot.domestics.force});
    changes.push_back({"g_notificationSpeed", snapshot.notification_speed});
    changes.push_back({"g_speedMultiplier", snapshot.speed_multiplier});
    changes.push_back({"bAIOfficerAutoGrowth", snapshot.ai_growth_settings.enabled});
    changes.push_back({"iAIOfficerGrowthSpeed", snapshot.ai_growth_settings.speed});
    changes.push_back({"bAIOfficerGrowthRestoreNone", snapshot.ai_growth_settings.restore_none});
    changes.push_back({"bAnnualSpecialAbilityAutoAssign", snapshot.ai_growth_settings.annual_special_abilities});
    for (std::size_t index = 0; index < domain::TENGI_EVENT_COUNT; ++index) {
        const TengiConfigKeys& keys = TENGI_CONFIG_KEYS[index];
        const domain::TengiEventSettings& event = snapshot.tengi_settings.events[index];

        changes.push_back({keys.allowed.data(), event.allowed});
        changes.push_back({keys.duration.data(), event.duration_months});
        changes.push_back({keys.cooldown.data(), event.cooldown_years});
    }
    if (snapshot.secondary_jewels) {
        const auto all = domain::all_secondary_jewels();
        changes.push_back({"bAllSecondaryJewels", snapshot.secondary_jewels->words == all.words});
        changes.push_back({"secondaryJewelForceMask", domain::secondary_jewel_mask_hex(*snapshot.secondary_jewels)});
    }
    changes.push_back({"iShortBattleCooldownDays", snapshot.short_battle_cooldown_days});
    changes.push_back({"bTroopCountCombatScaling", snapshot.troop_count_combat_enabled});
    changes.push_back({"bReinforcementArrivalAction", snapshot.reinforcement_arrival_enabled});
    changes.push_back({"bTechCityEditorVisible", snapshot.tech_city_visibility_enabled});
    changes.push_back({"bStratagemFiveEnabled", snapshot.stratagem5_enabled});
    const auto& stratagem = snapshot.stratagem5_settings;
    changes.push_back({"iStratagem5Target", stratagem.target});
    changes.push_back({"iStratagem5Effect1", stratagem.effect1});
    changes.push_back({"iStratagem5Power1", stratagem.power1});
    changes.push_back({"iStratagem5Duration1", stratagem.duration1});
    changes.push_back({"iStratagem5Effect2", stratagem.effect2});
    changes.push_back({"iStratagem5Power2", stratagem.power2});
    changes.push_back({"iStratagem5Duration2", stratagem.duration2});
    changes.push_back({"iStratagem5Range", stratagem.range});
    changes.push_back({"iStratagem5HealAmount", stratagem.heal_amount});
    const auto& heal = snapshot.self_heal_settings;
    changes.push_back({"v_HealLv1_Amount", heal.amount[0]});
    changes.push_back({"v_HealLv2_Amount", heal.amount[1]});
    changes.push_back({"v_HealLv2_Range", heal.range[0]});
    changes.push_back({"v_HealLv3_Amount", heal.amount[2]});
    changes.push_back({"v_HealLv3_Range", heal.range[1]});
    const auto& dongto = snapshot.dongto_settings;
    changes.push_back({"v_DongtoLv1_Prob", dongto.probability[0]});
    changes.push_back({"v_DongtoLv1_StateProb", dongto.status_probability[0]});
    changes.push_back({"v_DongtoLv2_Prob", dongto.probability[1]});
    changes.push_back({"v_DongtoLv2_StateProb", dongto.status_probability[1]});
    changes.push_back({"v_DongtoLv3_Prob", dongto.probability[2]});
    changes.push_back({"v_DongtoLv3_StateProb", dongto.status_probability[2]});
    const auto& celestial = snapshot.celestial_settings;
    constexpr std::array<std::string_view, 3> PROB{"v_CelestiaLv1_Prob", "v_CelestiaLv2_Prob", "v_CelestiaLv3_Prob"};
    constexpr std::array<std::string_view, 3> AMOUNT{"v_CelestiaLv1_Amount", "v_CelestiaLv2_Amount", "v_CelestiaLv3_Amount"};
    constexpr std::array<std::string_view, 3> RANGE{"v_CelestiaLv1_Range", "v_CelestiaLv2_Range", "v_CelestiaLv3_Range"};
    for (std::size_t i = 0; i < PROB.size(); ++i) {
        changes.push_back({PROB[i], celestial.probability[i]});
        changes.push_back({AMOUNT[i], celestial.amount[i]});
        changes.push_back({RANGE[i], celestial.range[i]});
    }
    changes.push_back({"v_Catapult_MinRange", snapshot.catapult_settings.min_range});
    changes.push_back({"v_Catapult_MaxRange", snapshot.catapult_settings.max_range});
    changes.push_back({"v_Catapult_Amount", snapshot.catapult_settings.amount});
    changes.push_back({"v_Catapult_Range", snapshot.catapult_settings.range});
    const auto& terrain = snapshot.terrain_tactics_settings;
    constexpr std::array<std::string_view, 3> WATER_AMOUNT{"v_WaterLv1_Amount", "v_WaterLv2_Amount", "v_WaterLv3_Amount"};
    constexpr std::array<std::string_view, 3> WATER_RANGE{"v_WaterLv1_Range", "v_WaterLv2_Range", "v_WaterLv3_Range"};
    constexpr std::array<std::string_view, 3> STONE_AMOUNT{"v_StoneLv1_Amount", "v_StoneLv2_Amount", "v_StoneLv3_Amount"};
    constexpr std::array<std::string_view, 3> STONE_RANGE{"v_StoneLv1_Range", "v_StoneLv2_Range", "v_StoneLv3_Range"};
    for (std::size_t i = 0; i < WATER_AMOUNT.size(); ++i) {
        changes.push_back({WATER_AMOUNT[i], terrain.water_amount[i]});
        changes.push_back({WATER_RANGE[i], terrain.water_range[i]});
        changes.push_back({STONE_AMOUNT[i], terrain.stone_amount[i]});
        changes.push_back({STONE_RANGE[i], terrain.stone_range[i]});
    }
    for (std::size_t i = 0; i < domain::DEF_BUILDING_KEYS.size(); ++i) {
        changes.push_back({domain::DEF_BUILDING_KEYS[i], snapshot.def_building_settings.values[i]});
    }
    static const auto terrain_keys = [] {
        std::array<std::array<std::string, 11>, 12> keys;
        for (std::size_t troop = 0; troop < keys.size(); ++troop) {
            for (std::size_t column = 0; column < keys[troop].size(); ++column) {
                keys[troop][column] = "TB_" + std::to_string(troop + 1) + "_" +
                    std::to_string(domain::TERRAIN_BONUS_COLUMNS[column]);
            }
        }
        return keys;
    }();
    for (std::size_t troop = 0; troop < terrain_keys.size(); ++troop) {
        for (std::size_t column = 0; column < terrain_keys[troop].size(); ++column) {
            changes.push_back({terrain_keys[troop][column],
                static_cast<int>(snapshot.terrain_bonus.values[troop + 1][domain::TERRAIN_BONUS_COLUMNS[column]])});
        }
    }
    return changes;
}

} // namespace s8::services
