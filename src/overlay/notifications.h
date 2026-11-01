#pragma once

#include "../services/ui_bridge.h"
#include "ui_atlas.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace s8::overlay {

struct Notification {
    std::string message;
    float lifetime_seconds = 6.0f;
    std::chrono::steady_clock::time_point created{};
    std::optional<domain::GameEpoch> session_epoch;
};

struct Notifications {
    std::array<Notification, 8> active{};
    std::array<std::string, 200> history{};
    std::size_t active_begin = 0;
    std::size_t active_count = 0;
    std::size_t history_begin = 0;
    std::size_t history_count = 0;
    bool show_history = false;
};

// Preserve the saved marquee-speed setting while presenting a readable toast duration.
float notification_duration_seconds(float speed) noexcept;
void enqueue_notification(Notifications& state, std::string_view message, float speed,
    std::optional<domain::GameEpoch> session_epoch = std::nullopt);
void draw_notifications(Notifications& state, services::UiBridge& bridge, float speed,
    const UiAtlasTextures& atlases, domain::GameEpoch current_epoch);
void append_notification_history(Notifications& state, std::string_view message);
void clear_active_notifications(Notifications& state);

} // namespace s8::overlay
