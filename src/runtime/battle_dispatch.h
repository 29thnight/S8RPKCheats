#pragma once

#include "../domain/error.h"
#include "../domain/ids.h"
#include "../game/stratagem5_native.h"

#include <cstdint>
#include <atomic>
#include <optional>
#include <string_view>

namespace s8::runtime {

struct Runtime;

struct BattleDispatch {
    game::Stratagem5Native native;
    Runtime* runtime = nullptr;
    std::uintptr_t module = 0;
    std::uintptr_t game_root = 0;
    std::uintptr_t root_pointer = 0;
    domain::GameEpoch epoch{0};
    std::optional<std::uint32_t> metadata_offset;
    bool metadata_searched = false;
    bool preparation_attempted = false;
    bool prepare_blocked = false;
    bool battle_seen = false;
    std::atomic<bool> stopping{false};
    bool stopped = false;
    std::uint64_t next_prepare_ms = 0;
    std::uint64_t now_ms = 0;
    game::Stratagem5NativeIssue observed_issue = game::Stratagem5NativeIssue::None;
    game::Stratagem5NativeIssue reported_issue = game::Stratagem5NativeIssue::None;
    std::uint64_t issue_since_ms = 0;
};

// Coordinator-only startup work. Pattern searches and unpublished code buffers
// are prepared here before the owner thread begins native commit dispatch.
bool prepare_battle_dispatch(BattleDispatch& state, Runtime& runtime, std::uintptr_t module_base);
// Call after tick_game established this dispatch's root/epoch, before command
// completion publishes the frame. Native UI work uses the same admitted thread.
void update_battle_dispatch(BattleDispatch& state, Runtime& runtime,
    std::uintptr_t module_base, std::uint64_t now_ms);
// A fixed message only when a requested feature changes failure state. Native
// model/layout readiness gets a short grace period; no per-frame log spam.
std::string_view take_battle_dispatch_notice(BattleDispatch& state, std::uint64_t now_ms);
// Close callbacks before stop_game; leave inactive bridges installed until the
// game's count/model restoration is complete. QueueFull is a retry boundary.
domain::Result<void> quiesce_battle_dispatch(BattleDispatch& state);
// Emergency loss of the dispatch window/thread cannot perform native cleanup.
// Close both C++ callbacks and direct assembly publications and retain owners.
void abandon_battle_dispatch(BattleDispatch& state) noexcept;
// Call after stop_game succeeded, so restoring original four-button code cannot
// race a still-extended model. Retained cave storage is never reclaimed.
domain::Result<void> stop_battle_dispatch(BattleDispatch& state, Runtime& runtime);

} // namespace s8::runtime
