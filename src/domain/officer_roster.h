#pragma once

#include "officer_basic.h"
#include "trait.h"
#include "ids.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace s8::domain {

struct OfficerRosterEntry {
    std::uint16_t id;
    std::uint8_t status;
};

using OfficerRoster = std::vector<OfficerRosterEntry>;
using OfficerNames = std::unordered_map<int, std::string>;

inline constexpr std::size_t OFFICER_RELATION_CAPACITY = 5102;
enum class OfficerRelationKind { Sworn, Spouse, Synergetic, Antipathetic, Enemy, Rival, Count };
enum class OfficerRelationsStatus { Loading, Ready, Partial, Unavailable, ReadFailed };

struct OfficerRelationMember {
    std::uint16_t id = 0;
    std::optional<std::uint8_t> affinity;
};

struct OfficerRelationGroup {
    std::array<OfficerRelationMember, OFFICER_RELATION_CAPACITY> members{};
    std::size_t count = 0;
    // A readable, completely scanned table with count == 0 means "none".
    // An unavailable group must not be presented as an empty relationship.
    bool available = false;
};

struct OfficerRelations {
    GameEpoch epoch{};
    std::uint16_t officer_id = 0;
    OfficerRelationsStatus status = OfficerRelationsStatus::Loading;
    std::array<OfficerRelationGroup, static_cast<std::size_t>(OfficerRelationKind::Count)> groups{};
};

struct OfficerFactionColor {
    std::uint16_t officer_id = 0;
    // Opaque observation generation; never a game address or faction index.
    std::uint64_t token = 0;
    std::uint8_t color = 0;
    bool available = false;
    bool no_faction = false;
    bool recovery_pending = false;
    bool operator==(const OfficerFactionColor&) const = default;
};

struct OfficerDetail {
    std::uint16_t id;
    std::uint8_t status;
    std::uint8_t gender;
    std::uint8_t strategic_tendency;
    std::uint8_t personality;
    std::uint8_t voice;
    std::uint8_t greed;
    std::uint8_t interests;
    std::uint8_t priority;
    std::array<std::uint16_t, 3> family;
    std::optional<std::uint8_t> city;
    std::optional<std::uint64_t> corps_number;
    std::uint16_t corps_governor = 0;
    OfficerTraits traits;
    OfficerBasicValues basic;
    std::array<std::uint8_t, 35> tactic_levels;
    std::array<std::uint8_t, 24> skill_levels;
    std::array<std::uint8_t, 11> aptitudes;
};

enum class OfficerTrainingKind { Tactic, Skill, Aptitude };

} // namespace s8::domain
