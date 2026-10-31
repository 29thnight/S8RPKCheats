#pragma once

#include "../domain/specialty_tools.h"

#include <array>
#include <atomic>
#include <optional>

namespace s8::services {

// Render thread produces requests; the window dispatcher alone consumes them.
// Snapshot slots transfer ownership, so copying a large inventory holds no lock.
class SpecialtyBridge {
public:
    bool push(const domain::SpecialtyRequest& request);
    std::optional<domain::SpecialtyRequest> pop();
    void publish(const domain::SpecialtySnapshot& snapshot);
    const domain::SpecialtySnapshot& read();
    void close();

private:
    static constexpr unsigned CAPACITY = 8;
    std::array<domain::SpecialtyRequest, CAPACITY> requests{};
    std::atomic<unsigned> write{0};
    std::atomic<unsigned> next{0};
    std::atomic<bool> open{true};
    std::array<domain::SpecialtySnapshot, 3> snapshots{};
    std::atomic<unsigned> middle{1};
    unsigned producer = 0;
    unsigned consumer = 2;
};

} // namespace s8::services
