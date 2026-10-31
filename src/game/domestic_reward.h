#pragma once

#include "../domain/domestic_reward.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace s8::game {

struct DomesticReward {
    std::atomic<std::int32_t> mode{0};
    std::uintptr_t cave = 0;
    std::array<std::byte, 5> jump{};
    bool applied = false;
};

bool set_domestic_reward(DomesticReward& state, std::uintptr_t module_base,
                         domain::DomesticRewardMode mode);

} // namespace s8::game
