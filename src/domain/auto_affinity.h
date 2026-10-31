#pragma once

#include <cstdint>
#include <optional>

namespace s8::domain {

struct AffinityTraits {
    std::uint8_t compatibility;
    std::uint8_t interest;
    std::uint8_t favored_reputation;
};

struct AffinityCalendar {
    std::uint16_t year = 0;
    std::uint8_t month = 0;
};

// The game adapter supplies validated compatibility values in the range 0..149.
std::optional<std::uint8_t> grow_affinity(std::uint8_t current, AffinityTraits left, AffinityTraits right);
std::optional<std::uint32_t> affinity_pair_offset(std::uint16_t left_id, std::uint16_t right_id);
bool advance_affinity_calendar(AffinityCalendar& calendar, std::uint16_t year, std::uint8_t month);

} // namespace s8::domain
