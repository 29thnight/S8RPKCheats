#pragma once

#include "../domain/officer_notifications.h"
#include "../domain/work_budget.h"

#include <span>

namespace s8::game {

inline constexpr std::size_t OFFICER_NOTICE_SLOT_COUNT = 5102;

struct OfficerNoticeBaseline {
    std::uintptr_t force = 0;
    std::uint16_t lord = 0;
    std::uint8_t status = 0;
    std::uint8_t city = 0xff;
    bool valid = false;
};

struct OfficerNoticeHistory {
    std::array<domain::OfficerNotice, 200> entries{};
    std::size_t begin = 0;
    std::size_t count = 0;
    std::uint64_t sequence = 0;
};

struct OfficerNotifications {
    domain::GameEpoch epoch{};
    std::uintptr_t hero = 0;
    std::uintptr_t roster = 0;
    std::uintptr_t city_base = 0;
    std::uint8_t last_phase = 0;
    std::uint8_t scan_phase = 0;
    bool enabled_ronins = false;
    bool enabled_changes = false;
    bool ronin_baseline_ready = false;
    bool change_baseline_ready = false;
    bool scanning = false;
    bool refresh_ronins = false;
    bool refresh_changes = false;
    bool notify_ronins = false;
    bool notify_changes = false;
    std::size_t next_slot = 0;
    std::array<bool, OFFICER_NOTICE_SLOT_COUNT + 1> was_ronin{};
    std::array<bool, OFFICER_NOTICE_SLOT_COUNT + 1> staging_ronins{};
    std::array<OfficerNoticeBaseline, OFFICER_NOTICE_SLOT_COUNT + 1> baseline{};
    std::array<OfficerNoticeBaseline, OFFICER_NOTICE_SLOT_COUNT + 1> staging{};
    domain::OfficerNotificationView view{};
    domain::OfficerNotificationView pending{};
    OfficerNoticeHistory history{};
    OfficerNoticeHistory pending_recruits{};
    OfficerNoticeHistory pending_deaths{};
};

void reset_officer_notifications(OfficerNotifications& state, domain::GameEpoch epoch);
// Game-owner call only. Skips battle/unknown phases. The first observation is a
// baseline; no historical changes are announced when a checkbox is enabled.
void tick_officer_notifications(OfficerNotifications& state, std::uintptr_t module_base,
    domain::GameEpoch epoch, std::uintptr_t hero, std::uint8_t phase, bool in_battle,
    bool monitor_ronins, bool monitor_changes, std::size_t slot_budget = 128,
    const domain::WorkBudget& work_budget = {});
void update_ronin_notice_baseline(OfficerNotifications& state, std::uint16_t officer, std::uint8_t status);
std::size_t copy_officer_notice_history(const OfficerNotifications& state,
    std::span<domain::OfficerNotice> destination);

} // namespace s8::game
