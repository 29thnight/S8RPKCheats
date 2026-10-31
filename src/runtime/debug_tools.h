#pragma once

#include "../domain/work_budget.h"

#include "../domain/debug_tools.h"
#include "../domain/ownership.h"
#include "../services/debug_bridge.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace s8::runtime {

struct MemoryScanStorage {
    static constexpr std::size_t BLOCK_SIZE = 64 * 1024;

    std::array<std::byte, BLOCK_SIZE + 7> block{};
    std::array<domain::MemoryMatch, domain::DEBUG_RESULT_LIMIT> results{};
};

struct DebugTools {
    domain::DebugSnapshot snapshot;
    own::unique_owner<MemoryScanStorage> storage;
    domain::MemoryValue query;
    std::uintptr_t cursor = 0;
    std::uintptr_t end = 0;
    std::uintptr_t block_address = 0;
    std::size_t block_candidates = 0;
    std::size_t block_cursor = 0;
    std::size_t block_advance = 0;
    std::size_t refine_cursor = 0;
    std::size_t refine_source_size = 0;
    std::size_t refine_matches = 0;
    std::int32_t refine_offset = 0;
    std::uint16_t refine_radius = 0;
    std::uint64_t last_publish_ms = 0;
    bool initialized = false;
    bool stopped = false;
    bool dirty = false;
};

// Called by the window dispatcher. One request and at most one 64 KiB block
// are handled per boundary, with a separate 500 us scan iteration deadline.
// An unobserved game context admits only requests that use cached UI state.
void update_debug_tools(DebugTools& state, services::DebugBridge& bridge, domain::GameEpoch epoch,
    std::uintptr_t module, std::uintptr_t game_base, std::uintptr_t player, std::uint64_t now_ms,
    bool game_context_observed, const domain::WorkBudget& work_budget = {});
void stop_debug_tools(DebugTools& state, services::DebugBridge& bridge);

} // namespace s8::runtime
