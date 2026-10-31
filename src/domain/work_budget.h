#pragma once

#include <chrono>

namespace s8::domain {

// Cooperative admission only: a native call or transaction already in flight
// completes before its owner checks the next work item. The default preserves
// the explicit, unbounded setup/shutdown callers outside the frame scheduler.
struct WorkBudget {
    std::chrono::steady_clock::time_point deadline = (std::chrono::steady_clock::time_point::max)();

    bool exhausted() const noexcept {
        return deadline != (std::chrono::steady_clock::time_point::max)() &&
            std::chrono::steady_clock::now() >= deadline;
    }
};

} // namespace s8::domain
