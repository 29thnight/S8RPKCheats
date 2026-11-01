#pragma once

#include "notifications.h"
#include "../domain/officer_notifications.h"
#include "../domain/officer_roster.h"

#include <cstdint>

namespace s8::overlay {

struct OfficerNoticePanel {
    domain::GameEpoch epoch{};
    std::uint64_t ronin_sequence = 0;
    std::uint64_t changes_sequence = 0;
    std::uint64_t history_sequence = 0;
};

void update_officer_notice_panel(OfficerNoticePanel& state, const domain::OfficerNoticesSnapshot& snapshot,
    Notifications& notifications, const domain::OfficerNames* names, float speed,
    bool monitor_ronins, bool notify_changes);

} // namespace s8::overlay
