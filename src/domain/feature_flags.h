#pragma once

#include <array>
#include <cstddef>
#include <string_view>
#include <optional>

namespace s8::domain {

enum class FeatureId {
    InfiniteAp,
    FastJewel,
    AutoFillSpecialties,
    InfiniteTavernRequests,
    BigCity,
    BangmokCity,
    NonggyeongCity,
    SangeopCity,
    AttitudeHack,
    InfiniteBanquet,
    HateCave,
    Loyalty,
    Resonance,
    InfiniteGift,
    InfiniteTalk,
    InfiniteDuel,
    InfiniteDebate,
    InfiniteMediation,
    FastRelationship,
    AutoAffinityGrowth,
    MarriageApplied,
    SelfHeal,
    Dongto,
    TerrainIgnore,
    DefBuilding,
    Catapult,
    Celestial,
    SiegeWarfare,
    SiegeWarfare2,
    WeatherSkillSimple,
    WeatherSkillComplex,
    DateAlways15,
    DateDynamic,
    SkipDaysEnabled,
    TerrainAbilityAtkDef,
    TerrainAbilityAll,
    AllAggressive,
    BattleUnit,
    BattleMapShuffle,
    RoadBlock,
    RoadBlock2,
    StartSetting,
    TechZero,
    FileLog,
    SkillCondition,
    YumokCondition,
    SangbyeongCondition,
    FactionLordBonus,
    MonitorRonin,
    AutoStatUp99,
    AutoLoadMenu,
    ZeroInfamy,
    SpeedHack,
    InfTengi,
    ShowWidgetNotif,
    ShowWidgetHero,
    ShowWidgetAllOfficers,
    Domestics,
    UndiscoveredToRonin,
    HealLv1Self,
    HealLv2Self,
    HealLv3Self,
    AiWarImprove,
    PrisonerCaptureManagement,
    GovernorPrisonerDisposal,
    GovernorPrisonerConsumePrivilege,
    MaxAttackStratagemGauge,
    MaxDefenseStratagemGauge,
    IsolatedTerritoryMovement,
    ReinforcementDefenderPlacement,
    CouncilContinueAfterMove,
    MissionCpuHeroExclusion,
    CouncilExecuteFreeOfficers,
    AffinityDisplay,
    Count,
};

struct FeatureFlags {
    std::array<bool, static_cast<std::size_t>(FeatureId::Count)> values{};

    FeatureFlags();
    bool get(FeatureId id) const;
    void set(FeatureId id, bool enabled);
};

void load_feature_flags(FeatureFlags& flags, std::string_view text);
using FeatureFlagChanges = std::array<std::optional<bool>, static_cast<std::size_t>(FeatureId::Count)>;
FeatureFlagChanges read_feature_flag_changes(std::string_view text);

} // namespace s8::domain
