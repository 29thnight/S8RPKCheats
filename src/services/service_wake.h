#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <stop_token>

namespace s8::services {

// Capture the revision before draining work. A request arriving during that
// drain then prevents the consumer from sleeping on an already handled signal.
struct ServiceWake {
    std::uint64_t observe() const noexcept {
        return revision.load(std::memory_order_acquire);
    }

    void signal() {
        {
            std::lock_guard lock(mutex);

            revision.fetch_add(1, std::memory_order_release);
        }
        ready.notify_one();
    }

    void wait_until(std::uint64_t observed, std::chrono::steady_clock::time_point deadline,
                    std::stop_token stop = {}) {
        std::unique_lock lock(mutex);

        ready.wait_until(lock, stop, deadline, [this, observed] {
            return revision.load(std::memory_order_acquire) != observed;
        });
    }

private:
    std::atomic<std::uint64_t> revision{0};
    std::mutex mutex;
    std::condition_variable_any ready;
};

} // namespace s8::services
