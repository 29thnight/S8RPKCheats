#pragma once

#include "ids.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace s8::domain {

inline constexpr std::size_t NATIVE_NAME_BATCH_CAPACITY = 32;
inline constexpr std::size_t NATIVE_NAME_TEXT_CAPACITY = 384;

struct NativeOfficerName {
    std::uint16_t id = 0;
    std::array<char, NATIVE_NAME_TEXT_CAPACITY> text{};
};

struct NativeOfficerNameBatch {
    GameEpoch epoch{};
    std::uint64_t generation = 0;
    std::size_t count = 0;
    bool reset = false;
    bool publish = false;
    std::array<NativeOfficerName, NATIVE_NAME_BATCH_CAPACITY> entries{};
};

} // namespace s8::domain
