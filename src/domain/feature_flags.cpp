#include "feature_flags.h"

namespace s8::domain {
namespace {

constexpr std::array FEATURE_KEYS{
    std::string_view{"\"bInfiniteAP\""},
    std::string_view{"\"bFastJewel\""},
    std::string_view{"\"bAutoFillSpecialties\""},
    std::string_view{"\"bInfiniteTavernRequests\""},
    std::string_view{"\"bBigCity\""},
    std::string_view{"\"bBangmokCity\""},
    std::string_view{"\"bNonggyeongCity\""},
    std::string_view{"\"bSangeopCity\""},
    std::string_view{"\"bAttitudeHack\""},
    std::string_view{"\"bInfiniteBanquet\""},
    std::string_view{"\"bHateCave\""},
    std::string_view{"\"bLoyalty\""},
    std::string_view{"\"bResonance\""},
    std::string_view{"\"bInfiniteGift\""},
    std::string_view{"\"bInfiniteTalk\""},
    std::string_view{"\"bInfiniteDuel\""},
    std::string_view{"\"bInfiniteDebate\""},
    std::string_view{"\"bInfiniteMediation\""},
    std::string_view{"\"bFastRelationship\""},
    std::string_view{"\"bAutoAffinityGrowth\""},
    std::string_view{"\"marriageApplied\""},
    std::string_view{"\"bSelfHeal\""},
    std::string_view{"\"bDongto\""},
    std::string_view{"\"bTerrainIgnore\""},
    std::string_view{"\"bDefBuilding\""},
    std::string_view{"\"bCatapult\""},
    std::string_view{"\"bCelestial\""},
    std::string_view{"\"bSiegeWarfare\""},
    std::string_view{"\"bSiegeWarfare2\""},
    std::string_view{"\"bWeatherSkillSimple\""},
    std::string_view{"\"bWeatherSkillComplex\""},
    std::string_view{"\"bDateAlways15\""},
    std::string_view{"\"bDateDynamic\""},
    std::string_view{"\"bSkipDaysEnabled\""},
    std::string_view{"\"bTerrainAbilityAtkDef\""},
    std::string_view{"\"bTerrainAbilityAll\""},
    std::string_view{"\"bAllAggressive\""},
    std::string_view{"\"bBattleUnit\""},
    std::string_view{"\"bBattleMapShuffle\""},
    std::string_view{"\"bRoadBlock\""},
    std::string_view{"\"bRoadBlock2\""},
    std::string_view{"\"bStartSetting\""},
    std::string_view{"\"bTechZero\""},
    std::string_view{"\"bFileLog\""},
    std::string_view{"\"bSkillCondition\""},
    std::string_view{"\"bYumokCondition\""},
    std::string_view{"\"bSangbyeongCondition\""},
    std::string_view{"\"bFactionLordBonus\""},
    std::string_view{"\"bMonitorRonin\""},
    std::string_view{"\"bAutoStatUp99\""},
    std::string_view{"\"bAutoLoadMenu\""},
    std::string_view{"\"bZeroInfamy\""},
    std::string_view{"\"bSpeedHack\""},
    std::string_view{"\"bInfTengi\""},
    std::string_view{"\"bShowWidgetNotif\""},
    std::string_view{"\"bShowWidgetHero\""},
    std::string_view{"\"bShowWidgetAllOfficers\""},
    std::string_view{"\"bDomestics\""},
    std::string_view{"\"bUndiscoveredToRonin\""},
    std::string_view{"\"bHealLv1_Self\""},
    std::string_view{"\"bHealLv2_Self\""},
    std::string_view{"\"bHealLv3_Self\""},
    std::string_view{"\"bAIWarImprove\""},
    std::string_view{"\"bPrisonerCaptureManagement\""},
    std::string_view{"\"bGovernorPrisonerDisposal\""},
    std::string_view{"\"bGovernorPrisonerConsumePrivilege\""},
    std::string_view{"\"bMaxAttackStratagemGauge\""},
    std::string_view{"\"bMaxDefenseStratagemGauge\""},
    std::string_view{"\"bIsolatedTerritoryMovement\""},
    std::string_view{"\"bReinforcementDefenderPlacement\""},
    std::string_view{"\"bCouncilContinueAfterMove\""},
    std::string_view{"\"bMissionCpuHeroExclusion\""},
    std::string_view{"\"bCouncilExecuteFreeOfficers\""},
    std::string_view{"\"bAffinityDisplay\""},
    std::string_view{"\"bTraitViewer\""},
};

static_assert(FEATURE_KEYS.size() == static_cast<std::size_t>(FeatureId::Count));

} // namespace

FeatureFlags::FeatureFlags() {
    set(FeatureId::ShowWidgetNotif, true);
    set(FeatureId::HealLv1Self, true);
    set(FeatureId::HealLv2Self, true);
    set(FeatureId::HealLv3Self, true);
    set(FeatureId::PrisonerCaptureManagement, true);
    set(FeatureId::IsolatedTerritoryMovement, true);
    set(FeatureId::TraitViewer, true);
}

bool FeatureFlags::get(FeatureId id) const {
    return values[static_cast<std::size_t>(id)];
}

void FeatureFlags::set(FeatureId id, bool enabled) {
    values[static_cast<std::size_t>(id)] = enabled;
}

FeatureFlagChanges read_feature_flag_changes(std::string_view text) {
    FeatureFlagChanges changes{};
    while (!text.empty()) {
        const auto end = text.find('\n');
        const auto line = text.substr(0, end);
        for (std::size_t i = 0; i < FEATURE_KEYS.size(); ++i) {
            if (line.find(FEATURE_KEYS[i]) != std::string_view::npos) {
                changes[i] = line.find("true") != std::string_view::npos;
                break;
            }
        }
        if (end == std::string_view::npos)
            break;
        text.remove_prefix(end + 1);
    }
    return changes;
}

void load_feature_flags(FeatureFlags& flags, std::string_view text) {
    const auto changes = read_feature_flag_changes(text);
    for (std::size_t i = 0; i < changes.size(); ++i) {
        if (changes[i]) {
            flags.values[i] = *changes[i];
        }
    }
}

} // namespace s8::domain
