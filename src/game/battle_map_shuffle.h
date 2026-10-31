#pragma once

#include <cstdint>
#include <random>
#include <vector>

namespace s8::game {

struct BattleMapRecord {
    std::uintptr_t address;
    std::uint64_t original;
};

struct BattleMapShuffle {
    std::vector<BattleMapRecord> records;
    std::vector<std::uint64_t> pool;
    std::uintptr_t cached_base = 0;
    std::uint64_t last_resolve_ms = 0;
    bool active = false;
    bool last_council = false;
    bool applied_this_council = false;
};

bool disable_battle_map_shuffle(BattleMapShuffle& state);
void update_battle_map_shuffle(BattleMapShuffle& state, std::uintptr_t module_base,
                               std::uint64_t now_ms, bool enabled, bool council, std::mt19937& random);

} // namespace s8::game
