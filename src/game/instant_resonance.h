#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace s8::game {

struct InstantResonance {
    bool signature_prepared = false;
    std::uintptr_t select_address = 0;
    std::uintptr_t getter_address = 0;
    std::uintptr_t select_cave = 0;
    std::uintptr_t getter_cave = 0;
    std::array<std::byte, 6> select_original{};
    std::array<std::byte, 9> getter_original{};
    std::uint16_t target_id = 0;
    std::uint16_t locked_id = 0;
    bool select_applied = false;
    bool getter_applied = false;
};

bool set_instant_resonance(InstantResonance& state, std::uintptr_t module_base,
                           std::span<const std::byte> image, bool enabled);

} // namespace s8::game
