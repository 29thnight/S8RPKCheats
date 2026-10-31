#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace s8::game {

struct InfiniteTalk {
    bool signature_prepared = false;
    std::uintptr_t hook = 0;
    std::uintptr_t cave = 0;
    std::array<std::byte, 7> original{};
    bool applied = false;
};

bool set_infinite_talk(InfiniteTalk& state, std::uintptr_t module_base,
                       std::span<const std::byte> image, bool enabled);

} // namespace s8::game
