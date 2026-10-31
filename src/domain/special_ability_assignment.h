#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace s8::domain {

inline constexpr std::size_t SPECIAL_ABILITY_COUNT = 10;
inline constexpr std::size_t SPECIAL_ASSIGNMENT_OFFICER_LIMIT = 5102;
inline constexpr std::array<std::string_view, SPECIAL_ABILITY_COUNT> SPECIAL_ABILITY_NAMES{
    "군악대", "무쌍 보병", "불꽃 기병", "원격 궁병", "등갑군",
    "총사령관", "기습부대", "대군사", "무신", "함선 병기화"};

struct SpecialAbilityProfile {
    std::uint16_t officer_id = 0;
    std::array<std::uint8_t, 5> stats{};
    std::array<std::array<std::uint8_t, 5>, 6> tactics{};
    std::array<std::uint8_t, 6> branch_skills{};
    std::array<std::uint8_t, 6> military_skills{};
};

enum class SpecialAssignmentPhase { Idle, Scanning, Committing, Completed, Cancelled, SessionChanged };

struct SpecialAssignmentResult {
    std::uint16_t officer_id = 0;
    std::uint16_t abilities = 0;
};

struct SpecialAssignmentSummary {
    SpecialAssignmentPhase phase = SpecialAssignmentPhase::Idle;
    bool annual = false;
    std::uint32_t scanned = 0;
    std::uint32_t valid = 0;
    std::uint32_t excluded_existing = 0;
    std::uint32_t matched_officers = 0;
    std::uint32_t committed_officers = 0;
    std::uint32_t assigned = 0;
    std::uint32_t assigned_officers = 0;
    std::array<std::uint32_t, SPECIAL_ABILITY_COUNT> matched_counts{};
    std::array<std::uint32_t, SPECIAL_ABILITY_COUNT> assigned_counts{};
};

// A bit for each synthetic skill key 0x1000 + index. The vanguard armor key
// (index 4) is intentionally manual-only, as in the original feature.
std::uint16_t evaluate_special_abilities(const SpecialAbilityProfile& profile);

} // namespace s8::domain
