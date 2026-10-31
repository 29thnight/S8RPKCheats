#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace s8::game {

struct FastRelationship {
    std::uintptr_t address = 0;
    bool applied = false;
};

bool set_fast_relationship(FastRelationship& state, std::uintptr_t module_base,
                           std::span<const std::byte> image, bool enabled);

} // namespace s8::game
