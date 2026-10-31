#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace s8::domain {

inline constexpr std::size_t AI_GROWTH_CATEGORY_COUNT = 7;
inline constexpr std::uint16_t AI_GROWTH_OFFICER_LIMIT = 1800;
inline constexpr std::array<std::string_view, AI_GROWTH_CATEGORY_COUNT> AI_GROWTH_CATEGORY_NAMES{
    "보병", "기병", "궁병", "함선", "군략", "보조", "둔갑"};

struct AiGrowthSettings {
    bool enabled = false;
    int speed = 2;
    bool restore_none = false;
    bool annual_special_abilities = false;

    bool operator==(const AiGrowthSettings&) const = default;
};

enum class AiGrowthOutcome { Completed, Cancelled, SessionChanged };

struct AiGrowthReport {
    AiGrowthOutcome outcome = AiGrowthOutcome::Completed;
    std::uint16_t year = 0;
    std::uint32_t eligible = 0;
    std::uint32_t consumed = 0;
    std::uint32_t changed = 0;
    std::uint32_t failed = 0;
    std::uint32_t tactic_level_ups = 0;
    std::uint32_t aptitude_gain = 0;
};

constexpr int normalize_ai_growth_speed(int speed) {

    return speed < 1 ? 1 : speed > 3 ? 3 : speed;
}

constexpr AiGrowthSettings normalize_ai_growth_settings(AiGrowthSettings settings) {

    settings.speed = normalize_ai_growth_speed(settings.speed);

    if (!settings.enabled) {
        settings.annual_special_abilities = false;
    }

    return settings;
}

constexpr std::uint32_t make_ai_growth_annual_seed(std::uint16_t year, std::uint16_t officer_id) {

    return (static_cast<std::uint32_t>(year) << 16) ^
        (static_cast<std::uint32_t>(officer_id) * 0x45d9f3bu) ^ 0xa17e5d31u;
}

struct AiGrowthProfile {
    std::uint16_t officer_id = 0;
    bool eligible = false;
    std::array<std::uint8_t, 5> stats{};
    std::array<std::uint8_t, 24> skills{};
    std::array<std::uint16_t, 5> current_exp{};
    std::array<std::uint16_t, 5> cumulative_exp{};
    std::array<std::uint8_t, AI_GROWTH_CATEGORY_COUNT> aptitudes{};
    std::array<std::array<std::uint8_t, 5>, AI_GROWTH_CATEGORY_COUNT> tactics{};
};

struct AiGrowthPreview {
    bool valid = false;
    bool eligible = false;
    std::uint16_t officer_id = 0;
    int speed = 2;
    int growth_factor = 5;
    std::array<std::uint8_t, AI_GROWTH_CATEGORY_COUNT> aptitude_before{};
    std::array<int, AI_GROWTH_CATEGORY_COUNT> aptitude_gain{};
    std::array<std::uint8_t, AI_GROWTH_CATEGORY_COUNT> aptitude_after{};
    std::array<int, AI_GROWTH_CATEGORY_COUNT> tactic_level_ups{};
    std::array<std::uint16_t, 5> current_exp_before{};
    std::array<std::uint16_t, 5> current_exp_after{};
    std::array<std::uint16_t, 5> cumulative_exp_before{};
    std::array<std::array<std::uint8_t, 5>, AI_GROWTH_CATEGORY_COUNT> tactics_before{};
    std::array<std::array<std::uint8_t, 5>, AI_GROWTH_CATEGORY_COUNT> tactics_after{};
};

AiGrowthPreview calculate_ai_growth(const AiGrowthProfile& profile, int speed,
                                    std::uint32_t random_seed = 0);

} // namespace s8::domain
