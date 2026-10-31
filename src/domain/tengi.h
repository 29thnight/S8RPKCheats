#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace s8::domain {

inline constexpr std::size_t TENGI_EVENT_COUNT = 15;
inline constexpr std::uint32_t ALL_TENGI_EVENTS = (1u << TENGI_EVENT_COUNT) - 1u;
inline constexpr std::array<std::uint8_t, TENGI_EVENT_COUNT> TENGI_EVENT_IDS{
    1, 2, 7, 8, 9, 10, 12, 14, 16, 18, 19, 20, 21, 22, 23};
inline constexpr std::array<std::string_view, TENGI_EVENT_COUNT> TENGI_EVENT_NAMES{
    "결전", "이민족습격", "악적발호", "의심암귀", "민심혹란",
    "붕벽", "여세", "피폐", "권위고양", "보장각성",
    "기장각성", "궁장각성", "병격난무", "전승기", "중지성성"};

struct TengiEventSettings {
    bool allowed = true;
    int duration_months = 0;
    int cooldown_years = -1;

    bool operator==(const TengiEventSettings&) const = default;
};

struct TengiSettings {
    bool force_quarterly = false;
    std::array<TengiEventSettings, TENGI_EVENT_COUNT> events{};

    bool operator==(const TengiSettings&) const = default;
};

constexpr TengiSettings normalize_tengi_settings(TengiSettings settings) {

    for (TengiEventSettings& event : settings.events) {

        event.duration_months = event.duration_months < 0 ? 0 :
            event.duration_months > 120 ? 120 : event.duration_months;
        event.cooldown_years = event.cooldown_years < -1 ? -1 :
            event.cooldown_years > 255 ? 255 : event.cooldown_years;
    }

    return settings;
}

constexpr std::uint32_t tengi_allowed_mask(const TengiSettings& settings) {

    std::uint32_t mask = 0;

    for (std::size_t index = 0; index < TENGI_EVENT_COUNT; ++index) {

        if (settings.events[index].allowed) {
            mask |= 1u << index;
        }
    }

    return mask;
}

constexpr bool tengi_id_allowed(std::uint8_t id, std::uint32_t mask) {

    if (mask == ALL_TENGI_EVENTS) {
        return true;
    }

    for (std::size_t index = 0; index < TENGI_EVENT_COUNT; ++index) {

        if (TENGI_EVENT_IDS[index] == id) {
            return (mask & (1u << index)) != 0;
        }
    }

    return false;
}

} // namespace s8::domain
