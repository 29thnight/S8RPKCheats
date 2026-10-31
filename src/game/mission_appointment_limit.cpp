#include "mission_appointment_limit.h"
#include "../platform/windows/process_memory.h"

#include <algorithm>
#include <array>
#include <span>

namespace s8::game {
namespace {

constexpr std::uintptr_t value_offset = 0x03bb2148;
struct CodeReference {
    std::uintptr_t offset;
    std::array<std::byte, 7> bytes;
    std::size_t size = 7;
};
constexpr std::array references{
    CodeReference{0x00685860, {std::byte{0x48},std::byte{0x8d},std::byte{0x05},std::byte{0xa9},std::byte{0x8d},std::byte{0x52},std::byte{0x03}}},
    CodeReference{0x01357341, {std::byte{0x44},std::byte{0x8b},std::byte{0xb8},std::byte{0x38},std::byte{0x3b},std::byte{0x00},std::byte{0x00}}},
    CodeReference{0x01ea0883, {std::byte{0x3b},std::byte{0xa8},std::byte{0x38},std::byte{0x3b},std::byte{0x00},std::byte{0x00},std::byte{0x00}}, 6},
    CodeReference{0x01e9f0da, {std::byte{0x48},std::byte{0x63},std::byte{0x88},std::byte{0x38},std::byte{0x3b},std::byte{0x00},std::byte{0x00}}},
    CodeReference{0x01e9ebde, {std::byte{0x44},std::byte{0x8b},std::byte{0xb8},std::byte{0x38},std::byte{0x3b},std::byte{0x00},std::byte{0x00}}},
    CodeReference{0x01e89e25, {std::byte{0x44},std::byte{0x3b},std::byte{0xa0},std::byte{0x38},std::byte{0x3b},std::byte{0x00},std::byte{0x00}}},
    CodeReference{0x01452460, {std::byte{0x44},std::byte{0x8b},std::byte{0xa0},std::byte{0x38},std::byte{0x3b},std::byte{0x00},std::byte{0x00}}},
};

} // namespace

std::optional<std::int32_t> read_mission_appointment_limit(std::uintptr_t module_base) {
    std::int32_t value;
    if (!module_base || !platform::windows::read_memory(module_base + value_offset,
            std::as_writable_bytes(std::span{&value, 1}))) {
        return std::nullopt;
    }
    return value;
}

bool set_mission_appointment_limit(MissionAppointmentLimit& state, std::uintptr_t module_base, std::int32_t value) {
    const auto current = read_mission_appointment_limit(module_base);
    if (!current) {
        return false;
    }
    if (!state.managed) {
        for (const auto& reference : references) {
            std::array<std::byte, 7> bytes;
            if (!platform::windows::read_memory(module_base + reference.offset, std::span{bytes}.first(reference.size)) ||
                !std::equal(bytes.begin(), bytes.begin() + reference.size, reference.bytes.begin())) {
                return false;
            }
        }
        if (*current != 10 && *current != value) {
            return false;
        }
        state.original = *current;
    } else if (*current != state.last_applied && *current != value) {
        return false;
    }
    if (*current != value &&
        (!platform::windows::write_data_memory(module_base + value_offset, std::as_bytes(std::span{&value, 1})) ||
         read_mission_appointment_limit(module_base) != value)) {
        return false;
    }
    state.last_applied = value;
    state.managed = true;
    return true;
}

bool restore_mission_appointment_limit(MissionAppointmentLimit& state, std::uintptr_t module_base) {
    if (!state.managed) {
        return true;
    }
    const auto current = read_mission_appointment_limit(module_base);
    if (!current || *current != state.last_applied) {
        return false;
    }
    if (*current != state.original &&
        (!platform::windows::write_data_memory(module_base + value_offset, std::as_bytes(std::span{&state.original, 1})) ||
         read_mission_appointment_limit(module_base) != state.original)) {
        return false;
    }
    state = {};
    return true;
}

} // namespace s8::game
