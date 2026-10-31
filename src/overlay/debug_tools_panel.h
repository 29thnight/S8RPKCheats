#pragma once

#include "../domain/debug_tools.h"
#include "../services/debug_bridge.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace s8::overlay {

struct DebugToolsPanel {
    bool open = false;
    bool memory_selected = false;
    bool auto_refresh = true;
    bool nearby = false;
    domain::DebugSnapshot snapshot;
    std::array<char, 32768> notes{};
    std::array<char, 24576> clipboard{};
    std::array<char, 32> address{};
    std::array<char, 64> search_value{};
    std::uint64_t next_request = 0;
    std::uint64_t pending_request = 0;
    std::size_t selected_byte = (std::numeric_limits<std::size_t>::max)();
    std::uint8_t replacement = 0;
    std::byte selected_expected{};
    int rows = 16;
    int value_type = 2;
    int offset = 0;
    int radius = 16;
    double last_refresh = 0;
    const char* local_error = nullptr;
};

void draw_debug_tools(DebugToolsPanel& panel, services::DebugBridge& bridge, domain::GameEpoch epoch);

} // namespace s8::overlay
