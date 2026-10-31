#pragma once

#include "officer_basic.h"
#include "trait.h"

#include <array>
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
