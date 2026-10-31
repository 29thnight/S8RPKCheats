#pragma once

#include "overlay_lifecycle.h"

#include <atomic>
#include <expected>
#include <thread>

namespace s8::runtime {

struct RuntimeServices {
    OverlayLifecycle overlay;
    std::atomic<bool> config_saved{false};
    std::jthread config_thread;
};

std::expected<void, int> start_runtime_services(RuntimeServices& services, HWND game_window,
                                                 std::uintptr_t plugin_module);
int stop_runtime_entries(RuntimeServices& services);
// Call after game and Present callbacks have returned and the subclass is removed.
std::expected<bool, int> finish_runtime_services(RuntimeServices& services);

} // namespace s8::runtime
