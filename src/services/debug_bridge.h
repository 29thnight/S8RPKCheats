#pragma once

#include "../domain/debug_tools.h"

#include <array>
#include <mutex>
#include <optional>

namespace s8::services {

class DebugBridge {
public:
    bool push(domain::DebugRequest request);
    std::optional<domain::DebugRequest> pop();
    void publish(const domain::DebugSnapshot& value);
    bool snapshot_since(std::uint64_t revision, domain::DebugSnapshot& output);
    void close();

private:
    static constexpr std::size_t CAPACITY = 16;
    std::mutex mutex;
    std::array<domain::DebugRequest, CAPACITY> queue{};
    std::size_t head = 0;
    std::size_t count = 0;
    bool closed = false;
    domain::DebugSnapshot current{};
};

} // namespace s8::services
