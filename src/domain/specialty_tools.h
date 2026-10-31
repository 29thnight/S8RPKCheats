#pragma once

#include "ids.h"

#include <array>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <string>

namespace s8::domain {

inline constexpr std::size_t SPECIALTY_COUNT = 301;
inline constexpr std::size_t SPECIALTY_SLOT_COUNT = 51 * 3;

struct SpecialtyDefinition {
    std::string name;
    std::string description;
};

using SpecialtyDefinitions = std::array<SpecialtyDefinition, SPECIALTY_COUNT>;

enum class SpecialtyAction : std::uint8_t { Refresh, Distribute, Give, Cancel };
enum class SpecialtyStatus : std::uint8_t {
    Ready, Reading, Distributing, Giving, Complete, Cancelled, SessionChanged,
    InvalidTarget, ReadFailed, Changed, WriteFailed, RollbackFailed, Busy
};

struct SpecialtyRequest {
    GameEpoch epoch{};
    std::uint64_t generation = 0;
    SpecialtyAction action = SpecialtyAction::Refresh;
    std::uint16_t target = 0;
    bool player = false;
    std::bitset<SPECIALTY_COUNT> selected;
};

struct SpecialtyRow {
    std::uintptr_t object = 0;
    std::uintptr_t vtable = 0;
    std::uintptr_t owner = 0;
    std::uint32_t owner_type = 0;
    std::uint16_t owner_id = 0;
    std::uint8_t city = 0xff;
    std::uint8_t type = 0;
    std::uint8_t trait = 0;
    std::uint8_t trait_level = 0;
    std::uint8_t ability = 0;
    std::uint8_t ability_value = 0;
    std::uint8_t effect = 0;
    std::uint8_t effect_value = 0;
    std::uint8_t value = 0;
    bool valid = false;
};

struct SpecialtySlot {
    std::uintptr_t object = 0;
    std::uint32_t enabled = 0;
    std::uint32_t bought = 0;
    std::uint16_t id = 0;
    bool valid = false;
};

struct SpecialtySnapshot {
    GameEpoch epoch{};
    std::uint64_t revision = 0;
    std::uint64_t generation = 0;
    std::array<SpecialtyRow, SPECIALTY_COUNT> items{};
    std::array<SpecialtySlot, SPECIALTY_SLOT_COUNT> slots{};
    SpecialtyStatus status = SpecialtyStatus::Ready;
    SpecialtyStatus last_error = SpecialtyStatus::Ready;
    std::size_t completed = 0;
    std::size_t total = 0;
    std::size_t succeeded = 0;
    std::size_t failed = 0;
    bool busy = false;
    bool available = false;
};

} // namespace s8::domain
