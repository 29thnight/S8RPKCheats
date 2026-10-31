#pragma once

#include "../domain/officer_tools.h"
#include "../domain/officer_notifications.h"

#include <mutex>
#include <optional>

namespace s8::services {

class OfficerToolsBridge {
public:
    bool publish(const domain::OfficerToolsView& view);
    std::optional<domain::OfficerToolsView> take();

private:
    std::mutex mutex;
    domain::OfficerToolsView snapshot;
    bool pending = false;
};

class OfficerNoticesBridge {
public:
    bool publish(const domain::OfficerNoticesSnapshot& view);
    std::optional<domain::OfficerNoticesSnapshot> take();

private:
    std::mutex mutex;
    domain::OfficerNoticesSnapshot snapshot;
    std::array<domain::OfficerNotificationView, 4> changes{};
    std::array<domain::OfficerNotice, 10> ronins{};
    std::size_t changes_begin = 0;
    std::size_t changes_count = 0;
    std::size_t ronin_count = 0;
    bool pending = false;
};

} // namespace s8::services
