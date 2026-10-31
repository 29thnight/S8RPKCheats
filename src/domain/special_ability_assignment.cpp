#include "special_ability_assignment.h"

#include <algorithm>

namespace s8::domain {
namespace {

constexpr int level(std::uint8_t value) {

    return value > 3 ? 3 : value;
}

template <std::size_t Count>
int sum_levels(const std::array<std::uint8_t, Count>& values) {

    int sum = 0;

    for (const std::uint8_t value : values) {
        sum += level(value);
    }

    return sum;
}

int max_level(const std::array<std::uint8_t, 5>& values) {

    int maximum = 0;

    for (const std::uint8_t value : values) {
        maximum = std::max(maximum, level(value));
    }

    return maximum;
}

} // namespace

std::uint16_t evaluate_special_abilities(const SpecialAbilityProfile& profile) {

    if (profile.officer_id == 0 || profile.officer_id > SPECIAL_ASSIGNMENT_OFFICER_LIMIT) {
        return 0;
    }

    const std::array<std::uint8_t, 5>& infantry = profile.tactics[0];
    const std::array<std::uint8_t, 5>& cavalry = profile.tactics[1];
    const std::array<std::uint8_t, 5>& archer = profile.tactics[2];
    const std::array<std::uint8_t, 5>& ship = profile.tactics[3];
    const std::array<std::uint8_t, 5>& strategy = profile.tactics[4];
    const std::array<std::uint8_t, 5>& support = profile.tactics[5];
    const int infantry_sum = sum_levels(infantry);
    const int cavalry_sum = sum_levels(cavalry);
    const int archer_sum = sum_levels(archer);
    const int military_sum = sum_levels(profile.military_skills);
    const int lead = profile.stats[0];
    const int war = profile.stats[1];
    const int intelligence = profile.stats[2];
    std::array<bool, SPECIAL_ABILITY_COUNT> matches{};

    matches[0] = (level(support[0]) >= 2 || level(support[1]) >= 2) &&
        level(support[0]) + level(support[1]) >= 4;
    matches[1] = war >= 80 && (level(infantry[0]) >= 2 || level(infantry[3]) >= 2) &&
        infantry_sum + level(profile.branch_skills[0]) * 2 >= 9;
    matches[2] = war >= 80 && (level(cavalry[0]) >= 2 || level(cavalry[3]) >= 2) &&
        cavalry_sum + level(profile.branch_skills[1]) * 2 >= 9;
    matches[3] = war >= 75 && archer_sum + level(profile.branch_skills[2]) * 2 >= 9 &&
        (level(archer[3]) >= 2 || level(archer[4]) >= 2 || archer_sum >= 8);

    const int strong_branches = static_cast<int>(infantry_sum >= 6) +
        static_cast<int>(cavalry_sum >= 6) + static_cast<int>(archer_sum >= 6);

    matches[5] = lead >= 90 && strong_branches >= 2 &&
        infantry_sum + cavalry_sum + archer_sum >= 16 && military_sum >= 6;

    const int disruption = level(infantry[2]);
    const int raid = level(cavalry[2]);
    const int intercept = level(strategy[3]);
    const int ambush_kinds = static_cast<int>(disruption > 0) +
        static_cast<int>(raid > 0) + static_cast<int>(intercept > 0);

    matches[6] = ambush_kinds >= 2 && disruption + raid + intercept >= 5;

    const int strategy_sum = level(strategy[0]) + level(strategy[1]) + level(strategy[2]) + level(strategy[3]);

    matches[7] = intelligence >= 85 && strategy_sum >= 7 &&
        (military_sum >= 5 || level(profile.branch_skills[5]) >= 2);
    matches[8] = war >= 95 && infantry_sum >= 5 && cavalry_sum >= 5 && archer_sum >= 5 &&
        infantry_sum + cavalry_sum + archer_sum >= 18 &&
        max_level(infantry) >= 2 && max_level(cavalry) >= 2 && max_level(archer) >= 2;
    matches[9] = sum_levels(ship) >= 7 && level(profile.branch_skills[3]) >= 2;

    std::uint16_t result = 0;

    for (std::size_t index = 0; index < SPECIAL_ABILITY_COUNT; ++index) {

        if (matches[index]) {
            result |= static_cast<std::uint16_t>(1u << index);
        }
    }

    return result;
}

} // namespace s8::domain
