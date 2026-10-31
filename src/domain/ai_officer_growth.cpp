#include "ai_officer_growth.h"

#include <algorithm>

namespace s8::domain {
namespace {

struct CategoryRule {
    std::uint8_t stat_a;
    std::uint8_t stat_b;
    std::uint8_t skill_a;
    std::uint8_t skill_b;
    std::array<std::uint8_t, 3> normal_stat;
    std::array<std::uint8_t, 3> normal_skill;
    std::array<std::uint8_t, 3> final_sum;
    std::array<std::uint8_t, 3> final_stat;
    std::array<std::uint8_t, 3> final_skill;
};

// One-based indices and costs match the original GrowthM category table.
constexpr std::array<CategoryRule, AI_GROWTH_CATEGORY_COUNT> RULES{{
    {2, 1, 13, 18, {25, 50, 75}, {0, 0, 1}, {7, 9, 12}, {75, 85, 95}, {0, 1, 3}},
    {2, 1, 14, 18, {25, 50, 75}, {0, 0, 1}, {7, 9, 12}, {75, 85, 95}, {0, 1, 3}},
    {2, 1, 15, 18, {25, 50, 75}, {0, 0, 1}, {7, 9, 12}, {75, 85, 95}, {0, 1, 3}},
    {2, 1, 16, 18, {25, 50, 75}, {0, 0, 1}, {7, 9, 12}, {75, 85, 95}, {0, 1, 3}},
    {3, 1, 18, 12, {25, 50, 75}, {0, 0, 0}, {7, 9, 12}, {75, 85, 95}, {0, 0, 1}},
    {4, 5, 18, 12, {25, 50, 75}, {0, 0, 1}, {7, 9, 12}, {75, 85, 95}, {0, 0, 1}},
    {3, 5, 18, 12, {25, 50, 75}, {0, 0, 0}, {7, 9, 12}, {75, 85, 95}, {0, 0, 1}}}};
constexpr std::array<int, 3> NORMAL_COSTS{7, 15, 25};
constexpr std::array<int, 3> FINAL_COSTS{15, 25, 45};
constexpr std::array<std::array<std::size_t, 2>, AI_GROWTH_CATEGORY_COUNT> RESIDUAL_STATS{{
    {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 2}, {3, 4}, {2, 4}}};

std::uint32_t next_random(std::uint32_t& state) {

    if (state == 0) {
        state = 0x9e3779b9u;
    }

    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

int weighted_pick(const std::array<int, 5>& indices, const std::array<int, 5>& weights,
                   std::size_t count, std::uint32_t& random) {

    int total = 0;

    for (std::size_t index = 0; index < count; ++index) {
        total += weights[index];
    }

    if (total <= 0) {
        return -1;
    }

    int roll = static_cast<int>(next_random(random) % static_cast<std::uint32_t>(total));

    for (std::size_t index = 0; index < count; ++index) {

        if (roll < weights[index]) {
            return indices[index];
        }

        roll -= weights[index];
    }

    return indices[count - 1];
}

bool eligible_tactic(const CategoryRule& rule, const AiGrowthProfile& profile,
                     const std::array<std::uint8_t, 5>& levels, std::size_t tactic) {

    const std::size_t level = levels[tactic];

    if (level >= 3) {
        return false;
    }

    const int stat = std::max(profile.stats[rule.stat_a - 1], profile.stats[rule.stat_b - 1]);
    const int skill = std::min<int>(3, std::max(profile.skills[rule.skill_a - 1],
                                              profile.skills[rule.skill_b - 1]));

    if (tactic < 4) {
        return stat >= rule.normal_stat[level] && skill >= rule.normal_skill[level];
    }

    int normal_sum = 0;

    for (std::size_t index = 0; index < 4; ++index) {
        normal_sum += levels[index];
    }

    return normal_sum >= rule.final_sum[level] && stat >= rule.final_stat[level] &&
        skill >= rule.final_skill[level];
}

int category_base(std::size_t category, const std::array<int, 5>& cumulative,
                   const AiGrowthProfile& profile) {

    switch (category) {
    case 0:
    case 1:
    case 2:
    case 3:
        return ((cumulative[0] + cumulative[1]) *
                (std::min<int>(3, profile.skills[RULES[category].skill_a - 1]) + 2)) / 10;
    case 4:
        return (cumulative[0] + cumulative[2] * 2) / 3;
    case 5:
        return (cumulative[1] + cumulative[2] + cumulative[3] + cumulative[4]) / 4;
    case 6:
        return (cumulative[2] + cumulative[3] + cumulative[4]) / 3;
    default:
        return 0;
    }
}

void add_experience(std::array<std::uint16_t, 5>& experience, std::size_t stat, int amount) {

    if (amount <= 0) {
        return;
    }

    experience[stat] = static_cast<std::uint16_t>(
        std::min(65535, static_cast<int>(experience[stat]) + amount));
}

} // namespace

AiGrowthPreview calculate_ai_growth(const AiGrowthProfile& profile, int speed, std::uint32_t random_seed) {

    AiGrowthPreview preview{};

    preview.valid = true;
    preview.officer_id = profile.officer_id;
    preview.speed = normalize_ai_growth_speed(speed);
    preview.growth_factor = preview.speed == 1 ? 3 : preview.speed == 3 ? 7 : 5;

    if (!profile.eligible || profile.officer_id == 0 || profile.officer_id > AI_GROWTH_OFFICER_LIMIT) {
        return preview;
    }

    preview.eligible = true;
    preview.current_exp_before = profile.current_exp;
    preview.current_exp_after = profile.current_exp;
    preview.cumulative_exp_before = profile.cumulative_exp;
    preview.aptitude_before = profile.aptitudes;

    std::array<int, 5> cumulative{};

    for (std::size_t stat = 0; stat < cumulative.size(); ++stat) {
        cumulative[stat] = (static_cast<int>(profile.cumulative_exp[stat]) + 9) / 10;
    }

    for (std::size_t category = 0; category < AI_GROWTH_CATEGORY_COUNT; ++category) {

        std::array<std::uint8_t, 5>& levels = preview.tactics_after[category];
        int tactic_sum = 0;

        for (std::size_t tactic = 0; tactic < levels.size(); ++tactic) {
            levels[tactic] = std::min<std::uint8_t>(3, profile.tactics[category][tactic]);
            tactic_sum += levels[tactic];
        }

        preview.tactics_before[category] = levels;

        const int gain = (category_base(category, cumulative, profile) * preview.growth_factor + 6) / 7;
        int aptitude = static_cast<int>(profile.aptitudes[category]) + gain;
        std::uint32_t random = random_seed;

        preview.aptitude_gain[category] = gain;

        if (random == 0) {
            random = 0xa511e9b3u ^ (static_cast<std::uint32_t>(profile.officer_id) * 0x45d9f3bu) ^
                (static_cast<std::uint32_t>(category + 1) * 0x9e3779b9u);

            for (const std::uint16_t value : profile.cumulative_exp) {
                random = random * 1664525u + static_cast<std::uint32_t>(value) + 1013904223u;
            }
        } else {
            random ^= static_cast<std::uint32_t>(category + 1) * 0x9e3779b9u;
        }

        for (int attempt = 0; attempt < 15 && aptitude >= 45 && tactic_sum < 15; ++attempt) {

            std::array<int, 5> indices{};
            std::array<int, 5> weights{};
            std::size_t count = 0;

            for (std::size_t tactic = 0; tactic < levels.size(); ++tactic) {

                if (!eligible_tactic(RULES[category], profile, levels, tactic)) {
                    continue;
                }

                indices[count] = static_cast<int>(tactic);
                weights[count] = levels[tactic] == 0 ? (tactic == 4 ? 3 : 2) : 1;
                ++count;
            }

            if (count == 0) {
                break;
            }

            const int picked = weighted_pick(indices, weights, count, random);

            if (picked < 0) {
                break;
            }

            const std::size_t selected = static_cast<std::size_t>(picked);
            const std::size_t level = levels[selected];
            const int cost = selected == 4 ? FINAL_COSTS[level] : NORMAL_COSTS[level];

            if (aptitude < cost) {
                break;
            }

            aptitude -= cost;
            ++levels[selected];
            ++preview.tactic_level_ups[category];
            ++tactic_sum;
        }

        const std::size_t first = RESIDUAL_STATS[category][0];
        const std::size_t second = RESIDUAL_STATS[category][1];
        const std::size_t residual_stat = profile.stats[first] <= profile.stats[second] ? first : second;

        if (tactic_sum >= 15) {
            add_experience(preview.current_exp_after, residual_stat, aptitude);
            aptitude = 0;
        } else if (aptitude > 200) {
            add_experience(preview.current_exp_after, residual_stat, aptitude - 200);
            aptitude = 200;
        }

        preview.aptitude_after[category] = static_cast<std::uint8_t>(std::clamp(aptitude, 0, 255));
    }

    return preview;
}

} // namespace s8::domain
