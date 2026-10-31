#pragma once

#include <array>
#include <cstdint>

namespace s8::domain {

struct OfficerBasicField {
    std::uint16_t offset;
    std::uint8_t size;
};

inline constexpr std::array officer_basic_fields{
    OfficerBasicField{0xaa, 1}, OfficerBasicField{0xab, 1}, OfficerBasicField{0xac, 1},
    OfficerBasicField{0xad, 1}, OfficerBasicField{0xae, 1}, OfficerBasicField{0x104, 2},
    OfficerBasicField{0x106, 2}, OfficerBasicField{0x108, 2}, OfficerBasicField{0x100, 2},
    OfficerBasicField{0xe8, 4}, OfficerBasicField{0xec, 1}, OfficerBasicField{0xed, 1},
    OfficerBasicField{0xee, 1},
    OfficerBasicField{0x2e, 2}, OfficerBasicField{0xa5, 1}, OfficerBasicField{0xa6, 1},
    OfficerBasicField{0x32, 2}, OfficerBasicField{0x34, 2}, OfficerBasicField{0x36, 2},
};

using OfficerBasicValues = std::array<std::uint32_t, officer_basic_fields.size()>;

} // namespace s8::domain
