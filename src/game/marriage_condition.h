#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace s8::game {

struct MarriageCondition {
    std::uintptr_t address = 0;
    std::array<std::byte, 2> original{};
    bool original_saved = false;
    bool applied = false;
};

bool set_marriage_condition(MarriageCondition& state, std::uintptr_t module_base,
                            std::span<const std::byte> image, bool enabled);

} // namespace s8::game
