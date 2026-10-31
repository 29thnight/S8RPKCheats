#pragma once

#include "../domain/tengi.h"

#include <array>
#include <cstdint>

namespace s8::game {

struct TengiDurationOverride {
    std::uintptr_t address = 0;
    std::uint16_t original = 0;
    std::uint16_t written = 0;
    bool active = false;
};

struct TengiCycle {
    std::uintptr_t session = 0;
    std::uintptr_t manager = 0;
    std::uintptr_t data = 0;
    int last_month = -1;
    int handled_council = -1;
    int start = -1;
    int earliest = -1;
    bool pending = false;
    bool needs_event_observation = false;
};

struct TengiState {
    TengiCycle cycle{};
    std::array<TengiDurationOverride, domain::TENGI_EVENT_COUNT> durations{};
    domain::TengiSettings settings{};
    std::uintptr_t module_base = 0;
    std::uint64_t last_tick_ms = 0;
    std::uint64_t last_duration_ms = 0;
    std::uint64_t last_filter_attempt_ms = 0;
    int filter_error = 0;
    std::uint8_t momentum = 0;
    bool layout_checked = false;
    bool layout_supported = false;
    bool settings_seen = false;
    bool filter_ready = false;
    bool snapshot_ready = false;
};

bool verified_tengi_layout(std::uintptr_t module_base);

// One game-thread owner. Preferences remain active when force_quarterly is OFF.
// A zero player marks a load/menu transition; no forced occurrence is requested.
void update_tengi(TengiState& state, std::uintptr_t module_base, std::uintptr_t player,
                  std::uint64_t now_ms, const domain::TengiSettings& settings);

// Restore only still-current definitions, then disable entry patches. Trampolines
// stay resident until process exit; do not globally uninitialize MinHook.
int stop_tengi(TengiState& state);

} // namespace s8::game
