#pragma once

#include "../domain/city_management.h"

#include <array>
#include <atomic>

namespace s8::services {

// One game-boundary publisher and one render consumer. Each owns its current
// slot exclusively; the middle slot transfers ownership with an atomic exchange.
class CityBridge {
public:
    void publish(const domain::CitySnapshot& snapshot);
    const domain::CitySnapshot& read();

private:
    static constexpr unsigned DIRTY = 4;
    static constexpr unsigned INDEX = 3;
    std::array<domain::CitySnapshot, 3> slots{};
    std::atomic<unsigned> middle{1};
    unsigned producer = 0;
    unsigned consumer = 2;
};

} // namespace s8::services
