#pragma once

#include "../domain/specialty_tools.h"
#include "../services/ui_bridge.h"

#include <array>
#include <bitset>

namespace s8::overlay {

struct SpecialtyToolsPanel {
    bool open = false;
    bool was_open = false;
    bool target_list_open = false;
    domain::GameEpoch epoch{};
    std::uint64_t generation = 0;
    std::bitset<domain::SPECIALTY_COUNT> selected;
    std::array<char, 96> filter{};
    std::array<std::uint16_t, 5102> filtered_ids{};
    std::size_t filtered_count = 0;
    int target = 1;
    const char* error = nullptr;
};

void draw_specialty_tools(SpecialtyToolsPanel& panel, services::UiBridge& bridge,
    domain::GameEpoch epoch, const domain::OfficerNames* names);

} // namespace s8::overlay
