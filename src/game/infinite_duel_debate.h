#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace s8::game {

enum class DuelDebateFeature { Duel, Debate };

struct InfiniteDuelDebate {
    std::uintptr_t hook = 0;
    std::uintptr_t cave = 0;
    bool duel = false;
    bool debate = false;
    bool applied = false;
};

bool set_infinite_duel_debate(InfiniteDuelDebate& state, std::uintptr_t module_base,
    std::span<const std::byte> image, DuelDebateFeature feature, bool enabled);

} // namespace s8::game
