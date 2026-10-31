#include "native_officer_name_bridge.h"

namespace s8::services {

bool NativeOfficerNameBridge::push(const domain::NativeOfficerNameBatch& batch) {
    const std::size_t position = write.load(std::memory_order_relaxed);
    const std::size_t next = (position + 1) % CAPACITY;

    if (batch.count > batch.entries.size() || next == read.load(std::memory_order_acquire)) {
        return false;
    }
    batches[position] = batch;
    write.store(next, std::memory_order_release);
    wake.signal();
    return true;
}

std::optional<domain::NativeOfficerNameBatch> NativeOfficerNameBridge::pop() {
    const std::size_t position = read.load(std::memory_order_relaxed);

    if (position == write.load(std::memory_order_acquire)) {
        return std::nullopt;
    }
    const domain::NativeOfficerNameBatch batch = batches[position];

    read.store((position + 1) % CAPACITY, std::memory_order_release);
    return batch;
}

bool NativeOfficerNameBridge::pending() const {
    return read.load(std::memory_order_relaxed) != write.load(std::memory_order_acquire);
}

} // namespace s8::services
