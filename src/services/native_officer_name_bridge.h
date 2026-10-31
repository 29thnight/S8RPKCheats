#pragma once

#include "service_wake.h"
#include "../domain/native_officer_names.h"

#include <atomic>
#include <optional>

namespace s8::services {

// Single producer: admitted game dispatcher. Single consumer: config service.
// Fixed value batches keep native reads and map/string publication separate.
class NativeOfficerNameBridge {
public:
    explicit NativeOfficerNameBridge(ServiceWake& wake) : wake(wake) {}
    bool push(const domain::NativeOfficerNameBatch& batch);
    std::optional<domain::NativeOfficerNameBatch> pop();
    bool pending() const;

private:
    static constexpr std::size_t CAPACITY = 8;
    ServiceWake& wake;
    std::array<domain::NativeOfficerNameBatch, CAPACITY> batches{};
    std::atomic<std::size_t> read{0};
    std::atomic<std::size_t> write{0};
};

} // namespace s8::services
