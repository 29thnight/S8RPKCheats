#include "corps_bridge.h"

namespace s8::services {

void CorpsBridge::publish(const domain::CorpsSnapshot& snapshot) {
    slots[producer] = snapshot;
    producer = middle.exchange(producer | DIRTY, std::memory_order_acq_rel) & INDEX;
}

const domain::CorpsSnapshot& CorpsBridge::read() {
    if ((middle.load(std::memory_order_acquire) & DIRTY) != 0) {
        consumer = middle.exchange(consumer, std::memory_order_acq_rel) & INDEX;
    }
    return slots[consumer];
}

} // namespace s8::services
