#pragma once

#include "../domain/work_budget.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace s8::game {

enum class GameBaseProbeStatus { Idle, Scanning, Waiting, Found, ReadFailed };

struct GameBaseProbe {
    GameBaseProbeStatus status = GameBaseProbeStatus::Idle;
    std::size_t cursor = 0;
    std::size_t image_size = 0;
    std::uint64_t passes = 0;

    // Window-thread-owned scan state. Only the progress fields above belong
    // in UI snapshots; neither the live addresses nor this buffer do.
    static constexpr std::size_t BLOCK_SIZE = 4096;
    static constexpr std::size_t OVERLAP_SIZE = 9;
    std::array<std::byte, BLOCK_SIZE + OVERLAP_SIZE> bytes{};
    std::uintptr_t module = 0;
    std::uintptr_t result = 0;
    std::uint64_t next_retry_ms = 0;
    std::size_t overlap_size = 0;
    bool read_failed = false;
};

// The input is a snapshot of the executable image beginning at module_base.
std::optional<std::uintptr_t> find_game_base_pointer(
    std::span<const std::byte> image, std::uintptr_t module_base);
// A signature alone is not sufficient to authorize a game root. The slot must
// be live image data and resolve to a readable root and identifiable player.
bool validate_game_base_pointer(std::uintptr_t module_base, std::uintptr_t slot);
// Each admitted block completes its scan and advances even if its read used
// the remaining budget. Missing/loading candidates are freshly read on later
// sweeps. cursor is an RVA; passes counts completed full sweeps. Assign {} to
// the probe to request a new search without retaining an old candidate.
std::optional<std::uintptr_t> probe_game_base_pointer(GameBaseProbe& probe,
    std::uintptr_t module_base, std::uint64_t now_ms, const domain::WorkBudget& budget = {});
std::uintptr_t read_game_base(std::uintptr_t pointer_address);
std::uintptr_t read_player(std::uintptr_t game_base);

} // namespace s8::game
