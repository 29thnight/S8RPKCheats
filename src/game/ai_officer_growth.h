#pragma once

#include "../domain/ai_officer_growth.h"
#include "../domain/ids.h"
#include "../domain/work_budget.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace s8::game {

using AiGrowthOutcome = domain::AiGrowthOutcome;
using AiGrowthReport = domain::AiGrowthReport;

struct AiGrowthState {
    domain::GameEpoch epoch{};
    std::uintptr_t module_base = 0;
    std::uintptr_t game_base = 0;
    std::uintptr_t player = 0;
    std::uintptr_t manager = 0;
    std::uintptr_t roster = 0;
    std::uint64_t last_poll_ms = 0;
    std::uint64_t last_setting_attempt_ms = 0;
    std::uint16_t last_year = 0;
    std::uint8_t last_month = 0;
    std::uint16_t pending_year = 0;
    std::uint16_t active_year = 0;
    std::uint16_t completed_year = 0;
    std::uint16_t player_id = 0;
    int active_speed = 2;
    int applied_game_speed = 0;
    std::size_t cursor = 0;
    bool date_initialized = false;
    bool running = false;
    bool game_setting_failed = false;
    std::array<bool, domain::AI_GROWTH_OFFICER_LIMIT + 1> seen{};
    std::array<std::byte, 0x3d0> record{};
    AiGrowthReport totals{};
    std::optional<AiGrowthReport> report;
};

// Command path: preserve the original game's nonzero speed on enabling; remember
// 0/0 for restoration on disabling. High bits of the game's setting are retained.
bool set_ai_growth_enabled(std::uintptr_t module_base, domain::AiGrowthSettings& settings, bool enabled);
bool set_ai_growth_speed(std::uintptr_t module_base, int speed);

bool read_ai_growth_preview(std::uintptr_t officer, std::uintptr_t player, int speed,
                            std::uint32_t seed, domain::AiGrowthPreview& preview);
bool read_current_ai_growth_preview(std::uintptr_t module_base, std::uintptr_t officer,
                                    std::uintptr_t player, int speed, domain::AiGrowthPreview& preview);

// At most eight roster records per call. Initial January attachment never runs a
// yearly job. A Completed report is the trigger for annual special assignments.
void update_ai_officer_growth(AiGrowthState& state, std::uintptr_t module_base,
                              std::uintptr_t game_base, std::uintptr_t player,
                              domain::GameEpoch epoch, std::uint64_t now_ms,
                              const domain::AiGrowthSettings& settings,
                              const domain::WorkBudget& work_budget = {});

} // namespace s8::game
