#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace s8::game {

struct InstantAttitude {
    std::uintptr_t address = 0;
};

bool set_instant_attitude(InstantAttitude& state, std::uintptr_t module_base,
                          std::span<const std::byte> image, bool enabled);

} // namespace s8::game
