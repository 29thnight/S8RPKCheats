#pragma once

#include <array>
#include <cstddef>

namespace s8::game::signatures {

inline constexpr std::array INSTANT_ATTITUDE{
    std::byte{0x44}, std::byte{0x88}, std::byte{0x44}, std::byte{0x07}, std::byte{0x70}};
inline constexpr std::array INFINITE_GIFT{
    std::byte{0x81}, std::byte{0x8e}, std::byte{0x20}, std::byte{0x03}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x04}, std::byte{0x00}, std::byte{0x00}};
inline constexpr std::array INFINITE_GIFT_PATCHED{
    std::byte{0x81}, std::byte{0xa6}, std::byte{0x20}, std::byte{0x03}, std::byte{0x00},
    std::byte{0x00}, std::byte{0xff}, std::byte{0xfb}, std::byte{0xff}, std::byte{0xff}};
inline constexpr std::array INFINITE_TALK{
    std::byte{0x83}, std::byte{0x8f}, std::byte{0x20}, std::byte{0x03},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x04}};
inline constexpr std::array INSTANT_LOYALTY{
    std::byte{0x0f}, std::byte{0xb6}, std::byte{0x86}, std::byte{0xec},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
inline constexpr std::array FAST_RELATIONSHIP{
    std::byte{0x0f}, std::byte{0xb6}, std::byte{0xcb}, std::byte{0x3a},
    std::byte{0xda}, std::byte{0x0f}, std::byte{0x4f}, std::byte{0xca}};
inline constexpr std::array MARRIAGE_CONDITION{
    std::byte{0x48}, std::byte{0x8b}, std::byte{0x47}, std::byte{0x10},
    std::byte{0x4c}, std::byte{0x3b}, std::byte{0xf0}};
inline constexpr std::array INFINITE_DUEL_DEBATE{
    std::byte{0x40}, std::byte{0x0f}, std::byte{0xb6}, std::byte{0xc5},
    std::byte{0xc1}, std::byte{0xe0}, std::byte{0x08}, std::byte{0x05},
    std::byte{0x00}, std::byte{0x01}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x09}, std::byte{0x87}, std::byte{0x20}, std::byte{0x03},
    std::byte{0x00}, std::byte{0x00}};
inline constexpr std::array DOMESTICS{
    std::byte{0x44}, std::byte{0x0f}, std::byte{0x4e},
    std::byte{0xf7}, std::byte{0x0f}, std::byte{0xb6},
    std::byte{0x5c}, std::byte{0x24}, std::byte{0x60}};
inline constexpr std::array RESONANCE_SELECT{
    std::byte{0x8b}, std::byte{0x86}, std::byte{0x20}, std::byte{0x03},
    std::byte{0x00}, std::byte{0x00}, std::byte{0xc1}, std::byte{0xe8},
    std::byte{0x12}, std::byte{0xa8}, std::byte{0x01}, std::byte{0x74}, std::byte{0x0a}};
inline constexpr std::array RESONANCE_GETTER{
    std::byte{0x41}, std::byte{0x0f}, std::byte{0xb6}, std::byte{0x84},
    std::byte{0x08}, std::byte{0xda}, std::byte{0x5b}, std::byte{0x00}, std::byte{0x00}};

} // namespace s8::game::signatures
