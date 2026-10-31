#pragma once

#include "feature_flags.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace s8::domain {

enum class LegacyFeatureGroup : std::uint8_t {
    SecondaryJewels, TechCityVisibility, ReinforcementArrival, ShortBattleCooldown, TroopCountCombat,
    Loyalty, LoveHate, Domestics, Resonance, FactionLordBonus,
    RoadBlock, RoadBlock2, SkillCondition, YumokCondition, SangbyeongCondition,
    BigCity, GrazingCity, FarmingCity, CommerceCity,
    Attitude, Relationship, Gift, Talk, Duel, Debate, Marriage, SpeedClock,
    Count
};

enum class LegacyFeaturePhase : std::uint8_t { Disabled, WaitingForPlayer, Pending, Applied, Retrying };

struct LegacyFeatureStatus {
    LegacyFeaturePhase phase = LegacyFeaturePhase::Disabled;
    bool applied = false;
};

inline constexpr std::size_t LEGACY_FEATURE_GROUP_COUNT = static_cast<std::size_t>(LegacyFeatureGroup::Count);
using LegacyFeatureStatuses = std::array<LegacyFeatureStatus, LEGACY_FEATURE_GROUP_COUNT>;

constexpr std::optional<LegacyFeatureGroup> legacy_feature_group(FeatureId feature) {

    switch (feature) {
    case FeatureId::Loyalty: return LegacyFeatureGroup::Loyalty;
    case FeatureId::HateCave: return LegacyFeatureGroup::LoveHate;
    case FeatureId::Domestics: return LegacyFeatureGroup::Domestics;
    case FeatureId::Resonance: return LegacyFeatureGroup::Resonance;
    case FeatureId::FactionLordBonus: return LegacyFeatureGroup::FactionLordBonus;
    case FeatureId::RoadBlock: return LegacyFeatureGroup::RoadBlock;
    case FeatureId::RoadBlock2: return LegacyFeatureGroup::RoadBlock2;
    case FeatureId::SkillCondition: return LegacyFeatureGroup::SkillCondition;
    case FeatureId::YumokCondition: return LegacyFeatureGroup::YumokCondition;
    case FeatureId::SangbyeongCondition: return LegacyFeatureGroup::SangbyeongCondition;
    case FeatureId::BigCity: return LegacyFeatureGroup::BigCity;
    case FeatureId::BangmokCity: return LegacyFeatureGroup::GrazingCity;
    case FeatureId::NonggyeongCity: return LegacyFeatureGroup::FarmingCity;
    case FeatureId::SangeopCity: return LegacyFeatureGroup::CommerceCity;
    case FeatureId::AttitudeHack: return LegacyFeatureGroup::Attitude;
    case FeatureId::FastRelationship: return LegacyFeatureGroup::Relationship;
    case FeatureId::InfiniteGift: return LegacyFeatureGroup::Gift;
    case FeatureId::InfiniteTalk: return LegacyFeatureGroup::Talk;
    case FeatureId::InfiniteDuel: return LegacyFeatureGroup::Duel;
    case FeatureId::InfiniteDebate: return LegacyFeatureGroup::Debate;
    case FeatureId::MarriageApplied: return LegacyFeatureGroup::Marriage;
    case FeatureId::SpeedHack: return LegacyFeatureGroup::SpeedClock;
    default: return std::nullopt;
    }
}

constexpr const char* legacy_feature_label(LegacyFeatureGroup group) {

    switch (group) {
    case LegacyFeatureGroup::SecondaryJewels: return "보조 보옥";
    case LegacyFeatureGroup::TechCityVisibility: return "기술도시 표시";
    case LegacyFeatureGroup::ReinforcementArrival: return "원군 즉시 행동";
    case LegacyFeatureGroup::ShortBattleCooldown: return "단기접전 대기일수";
    case LegacyFeatureGroup::TroopCountCombat: return "병력수 공방 반영";
    case LegacyFeatureGroup::Loyalty: return "충성도";
    case LegacyFeatureGroup::LoveHate: return "상극 무시";
    case LegacyFeatureGroup::Domestics: return "내정 배율";
    case LegacyFeatureGroup::Resonance: return "공명";
    case LegacyFeatureGroup::FactionLordBonus: return "세력 군주 보너스";
    case LegacyFeatureGroup::RoadBlock: return "교지 ↔ 건녕 도로 차단";
    case LegacyFeatureGroup::RoadBlock2: return "교지 ↔ 회계 도로 차단";
    case LegacyFeatureGroup::SkillCondition: return "만병 조건";
    case LegacyFeatureGroup::YumokCondition: return "유목 조건";
    case LegacyFeatureGroup::SangbyeongCondition: return "상병 조건";
    case LegacyFeatureGroup::BigCity: return "대도시 전환";
    case LegacyFeatureGroup::GrazingCity: return "방목도시 전환";
    case LegacyFeatureGroup::FarmingCity: return "농경도시 전환";
    case LegacyFeatureGroup::CommerceCity: return "상업도시 전환";
    case LegacyFeatureGroup::Attitude: return "견문 시 민심 최대";
    case LegacyFeatureGroup::Relationship: return "관계 단축";
    case LegacyFeatureGroup::Gift: return "선물 무제한";
    case LegacyFeatureGroup::Talk: return "담화 무제한";
    case LegacyFeatureGroup::Duel: return "대련 무제한";
    case LegacyFeatureGroup::Debate: return "토론 무제한";
    case LegacyFeatureGroup::Marriage: return "혼인 조건";
    case LegacyFeatureGroup::SpeedClock: return "게임 속도";
    default: return "게임 설정";
    }
}

} // namespace s8::domain
