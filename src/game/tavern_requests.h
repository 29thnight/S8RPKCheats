#pragma once

#include "../domain/work_budget.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace s8::game {

struct TavernRequest {
    std::uintptr_t name = 0;
    std::uintptr_t data = 0;
    bool operator==(const TavernRequest&) const = default;
};

struct TavernBackup {
    TavernRequest request;
    int source_city = -1;
    int source_slot = -1;
    bool borrowed = false;
};

struct TavernSource {
    int city = 0;
    int slot = 0;
    TavernRequest request;
};

enum class TavernMaintenancePhase : std::uint8_t { Idle, Scan, Fill };
enum class TavernFillPhase : std::uint8_t { Inspect, LiveFirst, Backup, LiveFallback };

struct TavernPendingPair {
    std::uintptr_t city_base = 0;
    std::uintptr_t address = 0;
    TavernRequest before;
    TavernRequest attempted;
    TavernBackup committed_backup;
    int city = 0;
    int slot = 0;
    bool replace_backup = false;
    bool active = false;
};

struct TavernRequests {
    std::array<std::array<TavernBackup, 4>, 51> backups{};
    std::array<std::array<TavernRequest, 4>, 51> injected{};
    std::array<TavernSource, 204> sources{};
    std::array<std::array<TavernRequest, 4>, 51> current{};
    std::array<std::array<bool, 4>, 51> readable{};
    TavernPendingPair pending;
    std::size_t source_count = 0;
    std::size_t cursor = 0;
    std::size_t source_cursor = 0;
    TavernMaintenancePhase maintenance = TavernMaintenancePhase::Idle;
    TavernFillPhase fill = TavernFillPhase::Inspect;
    bool prefer_live = false;
    std::uintptr_t city_base = 0;
    std::uint64_t phase_change_ms = 0;
    std::uint64_t last_tick_ms = 0;
    std::uint8_t last_phase = 0;
    bool waiting = false;
    bool enabled = false;
};

// False retains an unresolved write; explicit session reset may abandon it.
bool recover_tavern_requests(TavernRequests& state, std::uintptr_t module_base);
void update_tavern_requests(TavernRequests& state, std::uintptr_t module_base,
                            std::uintptr_t game_base, std::uint64_t now_ms, bool enabled,
                            const domain::WorkBudget& work_budget = {});

} // namespace s8::game
