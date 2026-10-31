#pragma once

#include "notifications.h"
#include "../domain/officer_notifications.h"
#include "../domain/officer_roster.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>

namespace s8::overlay {

struct RoninPopupRow {
    domain::OfficerNotice notice;
    float remaining_seconds = 12.0f;
};

struct OfficerChangePopup {
    domain::OfficerNotificationView notice;
    float remaining_seconds = 20.0f;
};

struct OfficerNoticePanel {
    domain::GameEpoch epoch{};
    std::uint64_t ronin_sequence = 0;
    std::uint64_t changes_sequence = 0;
    std::uint64_t history_sequence = 0;
    std::array<RoninPopupRow, 10> ronins{};
    std::size_t ronin_count = 0;
    std::array<OfficerChangePopup, 4> changes{};
    std::size_t changes_begin = 0;
    std::size_t changes_count = 0;
    std::chrono::steady_clock::time_point last_draw{};
};

void update_officer_notice_panel(OfficerNoticePanel& state, const domain::OfficerNoticesSnapshot& snapshot,
    Notifications& history, const domain::OfficerNames* names);
void draw_officer_notice_panel(OfficerNoticePanel& state, bool monitor_ronins,
    const domain::OfficerNames* names, float scale);

} // namespace s8::overlay
