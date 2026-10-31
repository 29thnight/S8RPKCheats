#pragma once

#include "../domain/named_specialties.h"
#include "../domain/specialty_tools.h"

#include <array>
#include <random>

namespace s8::game {

struct SpecialtyContext {
    std::uintptr_t module = 0;
    std::uintptr_t game_base = 0;
    std::uintptr_t player = 0;
    std::uintptr_t game_base_pointer = 0;
    domain::GameEpoch epoch{};
};

enum class SpecialtyPhase : std::uint8_t { Idle, Slots, Items, Distribute, Target, Give };

struct SpecialtyTools {
    domain::SpecialtySnapshot snapshot;
    SpecialtyContext context;
    SpecialtyPhase phase = SpecialtyPhase::Idle;
    domain::NamedSpecialties named{};
    std::bitset<domain::SPECIALTY_COUNT> selected;
    std::array<std::uint16_t, domain::SPECIALTY_COUNT> pool{};
    std::array<std::uint16_t, domain::SPECIALTY_SLOT_COUNT> empty_slots{};
    std::uintptr_t cities = 0;
    std::uintptr_t items = 0;
    std::uintptr_t stride = 0x40;
    std::uintptr_t target = 0;
    std::uintptr_t target_vtable = 0;
    std::uintptr_t target_roster = 0;
    std::uint16_t target_id = 0;
    std::size_t cursor = 0;
    std::size_t pool_size = 0;
    std::size_t slot_count = 0;
    bool distribute_after_refresh = false;
    bool target_is_player = false;
};

bool request_specialty_tools(SpecialtyTools& state, const SpecialtyContext& context,
    const domain::SpecialtyRequest& request, const domain::NamedSpecialties& named);
void update_specialty_tools(SpecialtyTools& state, const SpecialtyContext& context, std::mt19937& random);
void stop_specialty_tools(SpecialtyTools& state);

} // namespace s8::game
