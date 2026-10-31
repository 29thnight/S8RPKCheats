#pragma once

#include "../domain/officer_roster.h"
#include "../domain/officer_tools.h"

#include <array>
#include <cstdint>
#include <optional>

namespace s8::overlay {

struct OfficerToolsPanel {
    bool open = false;
    domain::GameEpoch epoch{};
    domain::BatchTrainingRequest training;
    domain::RandomTraitsRequest random;
    std::array<std::uint8_t, domain::OFFICER_TOOL_LIMIT + 1> child_years{};
    int random_officer = 0;
    int scenario_year = 200;
    int scenario_month = 1;
    int pregnancy_slot = 0;
    std::uint16_t pregnancy_spouse = 0;
    double last_date_sync = 0;
    bool date_input_invalid = false;
};

// The caller queues the returned value in the normal command mailbox. This
// panel reads only an immutable value snapshot and never accesses game memory.
std::optional<domain::OfficerToolsRequest> draw_officer_tools_panel(OfficerToolsPanel& panel,
    const domain::OfficerToolsView& view, const domain::OfficerNames* names, float scale, bool can_submit = true);

} // namespace s8::overlay
