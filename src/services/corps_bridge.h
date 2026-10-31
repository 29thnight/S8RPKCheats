#pragma once

#include "../domain/corps_deployment.h"

#include <array>
#include <atomic>

namespace s8::services {

// A single game-boundary writer and a single render reader own separate slots.
class CorpsBridge {
public:
    void publish(const domain::CorpsSnapshot& snapshot);
    const domain::CorpsSnapshot& read();

private:
    static constexpr unsigned DIRTY = 4;
    static constexpr unsigned INDEX = 3;
    std::array<domain::CorpsSnapshot, 3> slots{};
    std::atomic<unsigned> middle{1};
    unsigned producer = 0;
    unsigned consumer = 2;
};

} // namespace s8::services
