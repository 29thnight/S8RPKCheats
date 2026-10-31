#pragma once

#include <cstdint>
#include <optional>

namespace s8::game {

enum class BattleMode { Unknown, NonBattle, Battle };

struct NativeBattleState {
    std::uint8_t id;
    std::uint8_t meeting;
};

struct BattleState {
    std::uint64_t last_sample_ms = 0;
    std::uint64_t missing_since_ms = 0;
    BattleMode mode = BattleMode::Unknown;
    int game_state = -1;
    bool native_present = false;
};

BattleMode sample_battle_state(BattleState& state, std::uintptr_t module_base,
                               std::uintptr_t game_base, std::uint64_t now_ms);
BattleMode classify_battle_state(BattleState& state, bool session_ready, int game_state,
                                 std::optional<NativeBattleState> native, std::uint64_t now_ms);

} // namespace s8::game
