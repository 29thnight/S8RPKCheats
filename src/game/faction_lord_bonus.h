#pragma once

#include "../domain/work_budget.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace s8::game {

constexpr std::size_t FACTION_LORD_BONUS_CAPACITY = 240;
constexpr std::size_t FACTION_LORD_BONUS_DATA_COUNT = 6;

enum class FactionLordBonusPhase : std::uint8_t { Idle, ApplyData, Scan, Cleanup, RestoreData };

struct FactionLordAssignment {
    std::uintptr_t lord = 0;
    std::uintptr_t original = 0;
    std::uintptr_t applied = 0;
    std::uintptr_t write_before = 0;
    std::uintptr_t write_after = 0;
    std::uint64_t seen_pass = 0;
    std::uint16_t officer_id = 0;
    bool occupied = false;
    bool uncertain = false;
};

struct FactionLordDataBackup {
    std::array<std::byte, 48> original{};
    bool captured = false;
    bool applied = false;
    bool uncertain = false;
};

struct FactionLordBonus {
    // Keep failed cleanup records as well as this pass's at most 120 lords.
    std::array<FactionLordAssignment, FACTION_LORD_BONUS_CAPACITY> assigned{};
    std::array<FactionLordDataBackup, FACTION_LORD_BONUS_DATA_COUNT> data{};
    std::uintptr_t module = 0;
    std::uintptr_t root = 0;
    std::size_t assigned_count = 0;
    std::size_t assigned_limit = 0;
    std::size_t data_count = 0;
    std::size_t data_cursor = 0;
    std::size_t faction_cursor = 0;
    std::size_t cleanup_cursor = 0;
    std::uint64_t pass = 0;
    std::uint64_t last_tick_ms = 0;
    FactionLordBonusPhase phase = FactionLordBonusPhase::Idle;
    bool enabled = false;
    bool names_applied = false;
    bool restoring = false;
    bool scan_reliable = true;
    bool data_ready = true;
};

bool faction_lord_bonus_pending(const FactionLordBonus& state) noexcept;
// OFF only requests cleanup. The caller keeps updating while this returns false.
bool set_faction_lord_bonus(FactionLordBonus& state, std::uintptr_t module_base, bool enabled);
void update_faction_lord_bonus(FactionLordBonus& state, std::uintptr_t module_base,
    std::uint64_t now_ms, const domain::WorkBudget& work_budget = {});

} // namespace s8::game
