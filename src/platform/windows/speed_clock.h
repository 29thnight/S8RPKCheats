#pragma once

#include "../../domain/speed_hack.h"

#include <cstdint>
#include <expected>

namespace s8::platform::windows {

// Configure outside loader lock, on the runtime's single control owner.
// A nonzero excluded_thread must be a dedicated coordinator, never the game thread.
std::expected<void, int> configure_speed_clock(domain::SpeedHackSettings settings,
                                               std::uint32_t excluded_thread = 0);

// Stops acceleration. Entry patches remain at 1x with the accumulated offset,
// preserving monotonic game time. The DLL and trampolines remain resident.
int stop_speed_clock();
bool speed_clock_has_retained_hooks();

// Also bypasses time queries routed through the CRT while running our work on
// a game thread. End the scope before forwarding to an original game callback.
class RealClockScope {
public:
    RealClockScope();
    ~RealClockScope();
    RealClockScope(const RealClockScope&) = delete;
    RealClockScope& operator=(const RealClockScope&) = delete;
    RealClockScope(RealClockScope&&) = delete;
    RealClockScope& operator=(RealClockScope&&) = delete;
};

struct RealClockFrame {
    std::int64_t frequency = 0;
    std::int64_t previous = 0;
};

float real_clock_delta(RealClockFrame& frame);

} // namespace s8::platform::windows
