#include "specialty_bridge.h"

namespace s8::services {

bool SpecialtyBridge::push(const domain::SpecialtyRequest& request) {
    const unsigned position = write.load(std::memory_order_relaxed);
    const unsigned following = (position + 1) % CAPACITY;

    if (!open.load(std::memory_order_acquire) || following == next.load(std::memory_order_acquire)) {
        return false;
    }
    requests[position] = request;
    write.store(following, std::memory_order_release);
    return true;
}

std::optional<domain::SpecialtyRequest> SpecialtyBridge::pop() {
    const unsigned position = next.load(std::memory_order_relaxed);

    if (!open.load(std::memory_order_acquire) || position == write.load(std::memory_order_acquire)) {
        return std::nullopt;
    }
    const domain::SpecialtyRequest request = requests[position];

    next.store((position + 1) % CAPACITY, std::memory_order_release);
    return request;
}

void SpecialtyBridge::publish(const domain::SpecialtySnapshot& snapshot) {
    snapshots[producer] = snapshot;
    producer = middle.exchange(producer | 4, std::memory_order_acq_rel) & 3;
}

const domain::SpecialtySnapshot& SpecialtyBridge::read() {
    if ((middle.load(std::memory_order_acquire) & 4) != 0) {
        consumer = middle.exchange(consumer, std::memory_order_acq_rel) & 3;
    }
    return snapshots[consumer];
}

void SpecialtyBridge::close() {
    open.store(false, std::memory_order_release);
}

} // namespace s8::services
