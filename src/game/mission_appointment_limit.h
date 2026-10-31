#pragma once

#include <cstdint>
#include <optional>

namespace s8::game {

struct MissionAppointmentLimit {
    std::int32_t original = 10;
    std::int32_t last_applied = 10;
    bool managed = false;
};

std::optional<std::int32_t> read_mission_appointment_limit(std::uintptr_t module_base);
bool set_mission_appointment_limit(MissionAppointmentLimit& state, std::uintptr_t module_base, std::int32_t value);
bool restore_mission_appointment_limit(MissionAppointmentLimit& state, std::uintptr_t module_base);

} // namespace s8::game
