#pragma once

#include "../services/ui_bridge.h"

#include <chrono>
#include <string>
#include <vector>

namespace s8::overlay {

struct Notification {
    std::string message;
    float x = 0;
    float width = 0;
    std::chrono::steady_clock::time_point created;
};

struct Notifications {
    std::vector<Notification> active;
    std::vector<std::string> history;
    std::chrono::steady_clock::time_point last_draw{};
    bool show_history = false;
};

void draw_notifications(Notifications& state, services::UiBridge& bridge, float speed);

} // namespace s8::overlay
