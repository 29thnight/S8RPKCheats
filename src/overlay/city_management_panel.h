#pragma once

#include "../domain/city_management.h"
#include "../domain/officer_roster.h"
#include "../services/city_bridge.h"
#include "../services/command_mailbox.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace s8::services { class UiBridge; }

namespace s8::overlay {

struct CityManagementPanel {
    bool open = false;
    bool was_open = false;
    bool settings_dirty = false;
    bool routes_dirty = false;
    bool route_save_pending = false;
    bool technology_requested = false;
    domain::GameEpoch epoch{};
    std::uint64_t revision = 0;
    std::optional<domain::RequestId> pending;
    domain::CitySettings settings;
    domain::CitySupportRoutes routes;
    own::shared_owner<const domain::CitySupportRoutes> observed_routes;
    domain::CitySupportRoute manual_support{0, 1};
    std::array<std::array<std::uint32_t, domain::CITY_FIELD_COUNT>, domain::CITY_COUNT> drafts{};
    std::array<std::array<bool, domain::CITY_FIELD_COUNT>, domain::CITY_COUNT> editing{};
    std::array<std::array<int, 5>, domain::CITY_FACTION_COUNT> technology_drafts{};
    std::array<std::array<bool, 5>, domain::CITY_FACTION_COUNT> technology_editing{};
    std::uint16_t selected_force = domain::NO_CITY_OFFICER;
    int frontier_filter = 0;
    const char* local_error = nullptr;
    const char* save_message = nullptr;
};

bool on_city_management_completion(CityManagementPanel& panel, const services::CommandCompletion& completion);
void draw_city_management(CityManagementPanel& panel, services::UiBridge& bridge,
    domain::GameEpoch epoch, std::uint64_t& next_request,
    const domain::OfficerNames* officer_names = nullptr);

} // namespace s8::overlay
