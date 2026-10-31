#pragma once

#include "ids.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace s8::domain {

enum class OfficerNoticeKind : std::uint8_t { Ronin, Recruited, Died };

struct OfficerNotice {
    OfficerNoticeKind kind = OfficerNoticeKind::Ronin;
    std::uint16_t officer = 0;
    std::uint16_t previous_lord = 0;
    std::uint16_t current_lord = 0;
    std::uint8_t city = 0xff;
    bool previous_has_force = false;
    bool current_has_force = false;
};

struct OfficerNotificationView {
    GameEpoch epoch{};
    std::uint64_t ronin_sequence = 0;
    std::uint64_t changes_sequence = 0;
    std::array<OfficerNotice, 10> ronins{};
    std::array<OfficerNotice, 16> recruits{};
    std::array<OfficerNotice, 16> deaths{};
    std::size_t ronin_count = 0;
    std::size_t recruit_count = 0;
    std::size_t death_count = 0;
    std::size_t total_ronins = 0;
    std::size_t total_recruits = 0;
    std::size_t total_deaths = 0;
};

struct OfficerNoticesSnapshot {
    OfficerNotificationView view;
    std::array<OfficerNotice, 200> history{};
    std::size_t history_count = 0;
    std::uint64_t history_sequence = 0;
};

} // namespace s8::domain
