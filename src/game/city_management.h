#pragma once

#include "../domain/city_management.h"
#include "../domain/work_budget.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace s8::game {

enum class CityWork { Idle, Refresh, Maximize, Revolt, Exchange, Support, Routes, Technology, ConvertSelected };

struct CityDataWrite {
    std::uintptr_t address = 0;
    std::uint32_t before = 0;
    std::uint32_t after = 0;
    std::size_t width = 4;
};

struct CityManagement {
    domain::CitySnapshot snapshot;
    own::shared_owner<const domain::CitySupportRoutes> active_routes;
    domain::CityRequest request;
    std::array<std::uintptr_t, domain::CITY_COUNT> forces{};
    std::array<CityDataWrite, 6> recovery{};
    std::size_t recovery_count = 0;
    std::uintptr_t module_base = 0;
    std::uintptr_t game_root = 0;
    std::uintptr_t player = 0;
    std::uintptr_t city_table = 0;
    std::uintptr_t faction_table = 0;
    std::uintptr_t recovery_table = 0;
    std::uintptr_t route_force = 0;
    std::uint64_t next_maintenance_ms = 0;
    std::uint64_t next_revolt_ms = 0;
    domain::GameEpoch epoch{};
    std::size_t cursor = 0;
    unsigned pending = 0;
    CityWork work = CityWork::Idle;
    domain::CityWorkStatus work_result = domain::CityWorkStatus::Complete;
    std::uint16_t observed_year = 0;
    std::uint8_t observed_month = 0;
    std::uint8_t exchanged_month = 0xff;
    bool annual_routes = false;
    bool recovery_faction = false;
    bool stopping = false;
};

bool set_city_settings(CityManagement& state, const domain::CitySettings& settings);
bool set_city_support_routes(CityManagement& state, own::shared_owner<const domain::CitySupportRoutes> routes);
domain::CityWorkStatus request_city_management(CityManagement& state, std::uintptr_t module_base,
    std::uintptr_t game_root, std::uintptr_t player, domain::GameEpoch epoch, const domain::CityRequest& request);

// At most max_items city records or one route are processed per dispatch. This
// function performs no heap allocation and retains no UI or game object owners.
void tick_city_management(CityManagement& state, std::uintptr_t module_base, std::uintptr_t game_root,
    std::uintptr_t player, domain::GameEpoch epoch, std::uint64_t real_milliseconds, std::size_t max_items = 4,
    const domain::WorkBudget& work_budget = {});
bool stop_city_management(CityManagement& state, std::uintptr_t module_base, domain::GameEpoch epoch);

// Shared read-only binding helpers used by the independent corps planner.
std::uintptr_t resolve_city_data(std::uintptr_t module_base);
std::uintptr_t read_city_force(std::uintptr_t raw_city);
std::uint8_t city_index_from_pointer(std::uintptr_t city_table, std::uintptr_t raw_city);

} // namespace s8::game
