#include "debug_bridge.h"

namespace s8::services {

bool DebugBridge::push(domain::DebugRequest request) {
    std::lock_guard<std::mutex> lock(mutex);

    if (closed || count == CAPACITY) {
        return false;
    }
    queue[(head + count) % CAPACITY] = request;
    ++count;
    return true;
}

std::optional<domain::DebugRequest> DebugBridge::pop() {
    std::lock_guard<std::mutex> lock(mutex);

    if (!count) {
        return std::nullopt;
    }
    const domain::DebugRequest request = queue[head];

    head = (head + 1) % CAPACITY;
    --count;
    return request;
}

void DebugBridge::publish(const domain::DebugSnapshot& value) {
    std::lock_guard<std::mutex> lock(mutex);

    current = value;
}

bool DebugBridge::snapshot_since(std::uint64_t revision, domain::DebugSnapshot& output) {
    std::lock_guard<std::mutex> lock(mutex);

    if (current.revision == revision) {
        return false;
    }
    output = current;
    return true;
}

void DebugBridge::close() {
    std::lock_guard<std::mutex> lock(mutex);

    closed = true;
    count = 0;
}

} // namespace s8::services
