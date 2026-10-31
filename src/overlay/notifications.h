#pragma once

#include "../services/ui_bridge.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <string>
#include <string_view>

namespace s8::overlay {

struct Notification {
    std::string message;
    float x = 0;
    float width = 0;
    std::chrono::steady_clock::time_point created;
};

struct Notifications {
    std::array<Notification, 8> active{};
    std::array<std::string, 200> history{};
    std::size_t active_begin = 0;
    std::size_t active_count = 0;
    std::size_t history_begin = 0;
    std::size_t history_count = 0;
    std::chrono::steady_clock::time_point last_draw{};
    bool show_history = false;
};

void draw_notifications(Notifications& state, services::UiBridge& bridge, float speed);
void append_notification_history(Notifications& state, std::string_view message);
void clear_active_notifications(Notifications& state);

} // namespace s8::overlay
