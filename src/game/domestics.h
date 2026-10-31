#pragma once

#include "../domain/domestics.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace s8::game {

struct Domestics {
    bool signature_prepared = false;
    std::uintptr_t address = 0;
    std::uintptr_t cave = 0;
    std::uintptr_t player = 0;
    std::uintptr_t force = 0;
    std::array<std::byte, 9> original{};
    domain::DomesticsMultipliers multipliers;
    bool applied = false;
};

bool set_domestics(Domestics& state, std::uintptr_t module_base,
                   std::span<const std::byte> image, bool enabled);

} // namespace s8::game
