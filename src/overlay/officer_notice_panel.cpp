#include "officer_notice_panel.h"

#include "../domain/city_management.h"

#include <algorithm>
#include <string>

namespace s8::overlay {
namespace {

std::string officer_name(const domain::OfficerNames* names, std::uint16_t id) {
    if (names != nullptr) {
        const domain::OfficerNames::const_iterator found = names->find(id);

        if (found != names->end() && !found->second.empty()) {
            return found->second;
        }
    }
    return "미등록 무장(ID:" + std::to_string(id) + ")";
}

std::string force_name(const domain::OfficerNames* names, std::uint16_t lord, bool has_force) {
    if (!has_force) {
        return "무소속";
    }
    return lord != 0 ? officer_name(names, lord) + " 세력" : "세력 미확인";
}

const char* city_name(std::uint8_t city) {
    return city < domain::CITY_NAMES.size() ? domain::CITY_NAMES[city].data() : "도시 미확인";
}

std::string history_line(const domain::OfficerNotice& notice, const domain::OfficerNames* names) {
    const std::string name = officer_name(names, notice.officer);
    const std::string previous = force_name(names, notice.previous_lord, notice.previous_has_force);

    if (notice.kind == domain::OfficerNoticeKind::Recruited) {
        return "[등용] " + name + " | " + previous + " → " +
            force_name(names, notice.current_lord, notice.current_has_force);
    }
    if (notice.kind == domain::OfficerNoticeKind::Died) {
        return "[사망] " + name + " | " + previous;
    }
    return "[재야 감지] " + name + " → " + city_name(notice.city);
}

// Automatic officer notices use the same non-interactive paper strips as
// ordinary notices. Detailed text remains in the shared history window.
void record_notice(Notifications& notifications, const domain::OfficerNotice& notice,
    const domain::OfficerNames* names, float speed, domain::GameEpoch epoch, bool show_toast) {
    const std::string message = history_line(notice, names);
    if (show_toast) {
        enqueue_notification(notifications, message, speed, epoch);
    } else {
        append_notification_history(notifications, message);
    }
}

} // namespace

void update_officer_notice_panel(OfficerNoticePanel& state, const domain::OfficerNoticesSnapshot& snapshot,
    Notifications& notifications, const domain::OfficerNames* names, float speed,
    bool monitor_ronins, bool notify_changes) {
    const domain::OfficerNotificationView& view = snapshot.view;
    if (state.epoch != view.epoch) {
        state = {};
        state.epoch = view.epoch;
    }
    // The producer's history contains recruits/deaths; newly detected ronins
    // live in their own bounded view. Consume each channel once per sequence.
    if (view.ronin_sequence > state.ronin_sequence) {
        state.ronin_sequence = view.ronin_sequence;
        for (std::size_t index = 0; index < std::min(view.ronin_count, view.ronins.size()); ++index) {
            record_notice(notifications, view.ronins[index], names, speed, view.epoch, monitor_ronins);
        }
    }
    state.changes_sequence = std::max(state.changes_sequence, view.changes_sequence);
    const std::size_t history_count = std::min(snapshot.history_count, snapshot.history.size());
    const std::uint64_t first_sequence = snapshot.history_sequence >= history_count
        ? snapshot.history_sequence - history_count : 0;
    for (std::size_t index = 0; index < history_count; ++index) {
        if (first_sequence + index + 1 > state.history_sequence) {
            const domain::OfficerNotice& notice = snapshot.history[index];
            record_notice(notifications, notice, names, speed, view.epoch,
                notice.kind == domain::OfficerNoticeKind::Ronin ? monitor_ronins : notify_changes);
        }
    }
    state.history_sequence = std::max(state.history_sequence, snapshot.history_sequence);
}

} // namespace s8::overlay
