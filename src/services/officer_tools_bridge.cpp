#include "officer_tools_bridge.h"

namespace s8::services {

bool OfficerToolsBridge::publish(const domain::OfficerToolsView& view) {
    const std::unique_lock<std::mutex> lock(mutex, std::try_to_lock);

    if (!lock.owns_lock()) {
        return false;
    }
    snapshot = view;
    pending = true;
    return true;
}

std::optional<domain::OfficerToolsView> OfficerToolsBridge::take() {
    const std::unique_lock<std::mutex> lock(mutex, std::try_to_lock);

    if (!lock.owns_lock() || !pending) {
        return std::nullopt;
    }
    pending = false;
    return snapshot;
}

bool OfficerNoticesBridge::publish(const domain::OfficerNoticesSnapshot& view) {
    const std::unique_lock<std::mutex> lock(mutex, std::try_to_lock);

    if (!lock.owns_lock()) {
        return false;
    }
    if (snapshot.view.epoch != view.view.epoch) {
        changes_begin = 0;
        changes_count = 0;
        ronin_count = 0;
        snapshot.view = {};
    }
    if (view.view.ronin_sequence != snapshot.view.ronin_sequence) {
        if (view.view.ronin_count == 0) {
            ronin_count = 0;
        } else {
            for (std::size_t i = 0; i < view.view.ronin_count && ronin_count < ronins.size(); ++i) {
                ronins[ronin_count++] = view.view.ronins[i];
            }
        }
    }
    if (view.view.changes_sequence != snapshot.view.changes_sequence) {
        if (view.view.recruit_count == 0 && view.view.death_count == 0) {
            changes_begin = 0;
            changes_count = 0;
        } else {
            if (changes_count == changes.size()) {
                changes_begin = (changes_begin + 1) % changes.size();
                --changes_count;
            }
            changes[(changes_begin + changes_count) % changes.size()] = view.view;
            ++changes_count;
        }
    }
    snapshot = view;
    pending = true;
    return true;
}

std::optional<domain::OfficerNoticesSnapshot> OfficerNoticesBridge::take() {
    const std::unique_lock<std::mutex> lock(mutex, std::try_to_lock);

    if (!lock.owns_lock() || !pending) {
        return std::nullopt;
    }
    domain::OfficerNoticesSnapshot result = snapshot;

    result.view.ronins = ronins;
    result.view.ronin_count = ronin_count;
    ronin_count = 0;
    if (changes_count != 0) {
        const domain::OfficerNotificationView& change = changes[changes_begin];

        result.view.changes_sequence = change.changes_sequence;
        result.view.recruits = change.recruits;
        result.view.deaths = change.deaths;
        result.view.recruit_count = change.recruit_count;
        result.view.death_count = change.death_count;
        result.view.total_recruits = change.total_recruits;
        result.view.total_deaths = change.total_deaths;
        changes_begin = (changes_begin + 1) % changes.size();
        --changes_count;
    }
    pending = changes_count != 0;
    return result;
}

} // namespace s8::services
