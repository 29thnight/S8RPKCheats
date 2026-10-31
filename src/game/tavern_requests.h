#pragma once

#include <array>
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

struct TavernRequests {
    std::array<std::array<TavernBackup, 4>, 51> backups{};
    std::array<std::array<TavernRequest, 4>, 51> injected{};
    std::uintptr_t city_base = 0;
    std::uint64_t phase_change_ms = 0;
    std::uint64_t last_tick_ms = 0;
    std::uint8_t last_phase = 0;
    bool waiting = false;
    bool enabled = false;
};

void update_tavern_requests(TavernRequests& state, std::uintptr_t module_base,
                            std::uintptr_t game_base, std::uint64_t now_ms, bool enabled);

} // namespace s8::game
