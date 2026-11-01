#pragma once

#include "feature_flags.h"
#include "ids.h"
#include "stratagem5.h"
#include "self_heal.h"
#include "dongto.h"
#include "celestial.h"
#include "catapult.h"
#include "terrain_tactics.h"
#include "def_building.h"
#include "terrain_bonus.h"
#include "secondary_jewel.h"
#include "domestics.h"
#include "domestic_reward.h"
#include "officer_roster.h"
#include "tengi.h"
#include "ai_officer_growth.h"
#include "trait_control.h"
#include "officer_tools.h"
#include "city_management.h"
#include "corps_deployment.h"
#include "player_resources.h"

#include <cstdint>
#include <optional>
#include <variant>

namespace s8::domain {

struct SetFeature {
    FeatureId feature;
    bool enabled;
};

struct SetShortBattleCooldown {
    bool enabled;
    std::int32_t days;
};

struct SetTroopCountCombat {
    bool enabled;
};

struct SetReinforcementArrival {
    bool enabled;
};

struct SetTechCityVisibility {
    bool enabled;
};

struct SetStratagem5Enabled {
    bool enabled;
};

struct SetStratagem5Settings {
    Stratagem5Settings settings;
};

struct SetSelfHealSettings {
    SelfHealSettings settings;
};

struct SetDongtoSettings {
    DongtoSettings settings;
};

struct SetCelestialSettings {
    CelestialSettings settings;
};

struct SetCatapultSettings {
    CatapultSettings settings;
};

struct SetTerrainTacticsSettings {
    TerrainTacticsSettings settings;
};

struct SetDefBuildingSettings {
    DefBuildingSettings settings;
};

struct SetSkipDays {
    int days;
};

struct SetTerrainBonus {
    TerrainBonus table;
};

struct SetSiegeHealRate { int rate; };

struct SetAllJewelsOpen { bool enabled; };

struct SetSecondaryJewels { SecondaryJewelMask mask; };
struct SetDomesticsMultipliers { DomesticsMultipliers multipliers; };
struct SetMissionAppointmentLimit { std::int32_t value; };
struct RestoreMissionAppointmentLimit {};
struct SetNotificationSpeed { float speed; };
struct SetUiScale { float scale; };
struct SetDomesticRewardMode { DomesticRewardMode mode; };
struct SetSkillCount { int officer_id; std::uintptr_t offset; int count; };
struct SetProtagonistTacticLevel { std::uint16_t officer_id; std::uint8_t index; std::uint8_t level; };
struct SetProtagonistTraining { std::uint16_t officer_id; bool aptitude; std::uint8_t index; std::uint8_t value; };
struct SetProtagonistBasic { std::uint16_t officer_id; std::uint8_t index; std::uint32_t value; };
struct SetProtagonistGender { std::uint16_t officer_id; std::uint8_t gender; };
struct RefreshOfficerRoster {};
struct SelectRosterOfficer { std::uint16_t officer_id; };
struct RefreshOfficerRelations { std::uint16_t officer_id; bool protagonist = false; };
struct SetOfficerFactionColor {
    std::uint16_t officer_id;
    std::uint64_t token;
    std::uint8_t expected_color;
    std::uint8_t color;
    bool protagonist = false;
};
struct SetRosterOfficerBasic { std::uint16_t officer_id; std::uint8_t index; std::uint32_t value; };
struct SetRosterOfficerGender { std::uint16_t officer_id; std::uint8_t gender; };
struct SetRosterOfficerVoice { std::uint16_t officer_id; std::uint8_t voice; };
struct SetRosterOfficerRonin { std::uint16_t officer_id; bool undiscovered_only = true; };
struct MoveRosterOfficer { std::uint16_t officer_id; std::uint8_t city; };
struct ReviveRosterOfficer { std::uint16_t officer_id; std::optional<std::uint8_t> city; };
struct SetRosterOfficerTrait { std::uint16_t officer_id; std::uint8_t slot; std::uint16_t trait_id; bool protagonist = false; };
struct SetRosterOfficerTraining { std::uint16_t officer_id; OfficerTrainingKind kind; std::uint8_t index; std::uint8_t value; };

struct SetSpeedMultiplier { float multiplier; };
struct SetTengiSettings { TengiSettings settings; };
struct SetAiGrowthEnabled { bool enabled; };
struct SetAiGrowthSpeed { int speed; };
struct SetAnnualSpecialAbilities { bool enabled; };
struct StartSpecialAbilityAssignment {};
struct CancelSpecialAbilityAssignment {};

using CommandPayload = std::variant<SetFeature, SetSpeedMultiplier, SetTengiSettings,
                                    SetAiGrowthEnabled, SetAiGrowthSpeed, SetAnnualSpecialAbilities,
                                    StartSpecialAbilityAssignment, CancelSpecialAbilityAssignment,
                                    SetShortBattleCooldown, SetTroopCountCombat,
                                    SetReinforcementArrival, SetTechCityVisibility,
                                    SetStratagem5Enabled, SetStratagem5Settings, SetSelfHealSettings,
                                    SetDongtoSettings, SetCelestialSettings, SetCatapultSettings,
                                    SetTerrainTacticsSettings, SetDefBuildingSettings, SetSkipDays, SetTerrainBonus,
                                    SetSiegeHealRate, SetAllJewelsOpen, SetSecondaryJewels,
                                    SetDomesticsMultipliers, SetMissionAppointmentLimit,
                                    RestoreMissionAppointmentLimit, SetNotificationSpeed, SetDomesticRewardMode,
                                    SetSkillCount, SetProtagonistTacticLevel, SetProtagonistTraining,
                                    SetProtagonistBasic, SetProtagonistGender, RefreshOfficerRoster, SelectRosterOfficer,
                                    RefreshOfficerRelations, SetOfficerFactionColor,
                                    SetRosterOfficerBasic, SetRosterOfficerGender, SetRosterOfficerVoice,
                                    SetRosterOfficerRonin, MoveRosterOfficer, ReviveRosterOfficer,
                                    SetRosterOfficerTrait, SetRosterOfficerTraining, ApplyTraitTextEdits,
                                    OfficerToolsRequest, CityRequest, SetCitySettings, SetCitySupportRoutes, CorpsRequest,
                                    SetUiScale, SetPlayerResource>;

struct Command {
    RequestId request;
    GameEpoch epoch;
    CommandPayload payload;
};

} // namespace s8::domain
