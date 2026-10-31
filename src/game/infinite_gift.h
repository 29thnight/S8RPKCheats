#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace s8::game {

struct InfiniteGift {
    std::uintptr_t code_address = 0;
    bool applied = false;
};

bool set_infinite_gift(InfiniteGift& state, std::uintptr_t module_base,
                       std::span<const std::byte> image, bool enabled);

} // namespace s8::game
