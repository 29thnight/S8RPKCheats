#include "auto_affinity.h"

#include <algorithm>
#include <cstdlib>

namespace s8::domain {

std::optional<std::uint32_t> affinity_pair_offset(std::uint16_t left_id, std::uint16_t right_id) {
    const auto index = [](std::uint16_t id) -> int {
        if (id >= 1 && id <= 1000) { return id; }
        if (id >= 2001 && id <= 2100) { return id - 1000; }
        if (id >= 4001 && id <= 4200) { return id - 2900; }
        if (id >= 5001 && id <= 5100) { return id - 3700; }
        if (id >= 3001 && id <= 3150) { return id - 1600; }
        if (id >= 5101 && id <= 5200) { return id - 3550; }
        return -1;
    };
    const auto left_index = index(left_id);
    const auto right_index = index(right_id);
    if (left_index < 1 || right_index < 1 || left_index == right_index) {
        return std::nullopt;
    }
    const auto first = std::min(left_index, right_index);
    const auto second = std::max(left_index, right_index);
    const auto row = static_cast<std::uint32_t>(first - 1);
    return row * (3299u - row) / 2u + static_cast<std::uint32_t>(second - first - 1);
}

bool advance_affinity_calendar(AffinityCalendar& calendar, std::uint16_t year, std::uint8_t month) {
    if (calendar.year == 0 || calendar.month == 0) {
        calendar = {year, month};
        return false;
    }
    const auto previous = static_cast<unsigned>(calendar.year) * 12 + calendar.month;
    const auto current = static_cast<unsigned>(year) * 12 + month;
    calendar = {year, month};
    return current > previous && (month == 1 || month == 4 || month == 7 || month == 10);
}

std::optional<std::uint8_t> grow_affinity(std::uint8_t current, AffinityTraits left, AffinityTraits right) {
    if (current >= 100) {
        return std::nullopt;
    }
    const auto difference = std::abs(static_cast<int>(left.compatibility) - static_cast<int>(right.compatibility));
    const auto distance = std::min(difference, 150 - difference);
    int gain = std::max(0, 15 - (distance + 4) / 5);
    for (int shift = 0; shift <= 6; shift += 2) {
        const auto a = (left.interest >> shift) & 3;
        const auto b = (right.interest >> shift) & 3;
        if (a != 0 && a == b) {
            gain += 3;
        }
    }
    if (left.favored_reputation != 0 && left.favored_reputation == right.favored_reputation) {
        gain += 3;
    }
    if (gain == 0) {
        return std::nullopt;
    }
    return static_cast<std::uint8_t>(std::min(100, static_cast<int>(current) + gain));
}

} // namespace s8::domain
