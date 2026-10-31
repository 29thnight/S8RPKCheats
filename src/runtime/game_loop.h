#pragma once

#include <cstdint>

namespace s8::runtime {

// Pins the proxy and schedules initialization. Does not wait or initialize
// graphics, hooks, configuration, or C++ runtime owners under loader lock.
bool start_runtime_async(std::uintptr_t plugin_module) noexcept;

} // namespace s8::runtime
