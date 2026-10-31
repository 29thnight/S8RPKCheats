#pragma once

#include "../domain/ids.h"

#include <cstdint>

namespace s8::game {

enum class CityConversionKind { BigCity, Grazing, Farming, Commerce };
enum class CityConversionPhase { Idle, First, Second };

struct CityConversion {
    CityConversionPhase phase = CityConversionPhase::Idle;
    CityConversionKind kind = CityConversionKind::BigCity;
    domain::GameEpoch epoch{0};
    std::uintptr_t module_base = 0;
    std::uintptr_t city_table = 0;
    std::uint64_t deadline_ms = 0;
    bool applied = false;
};

bool request_city_conversion(CityConversion& state, CityConversionKind kind, bool enabled,
                             domain::GameEpoch epoch, std::uintptr_t module_base, std::uint64_t now_ms);
void tick_city_conversion(CityConversion& state, domain::GameEpoch epoch, std::uint64_t now_ms);
bool stop_city_conversion(CityConversion& state, std::uintptr_t module_base, domain::GameEpoch epoch);

} // namespace s8::game
