#include "city_control.h"

#include "runtime.h"
#include "../game/game_base.h"
#include "../services/ui_bridge.h"

#include <utility>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

namespace s8::runtime {

domain::Result<void> submit_city_request(Runtime& runtime, std::uintptr_t module, const domain::CityRequest& request) {
    const std::uintptr_t root = game::read_game_base(runtime.game_base_pointer);
    const domain::CityWorkStatus status = game::request_city_management(runtime.city_management, module,
        root, game::read_player(root), runtime.epoch, request);

    runtime.city_management.snapshot.last_request = status;
    ++runtime.city_management.snapshot.revision;
    if (status == domain::CityWorkStatus::Working || status == domain::CityWorkStatus::Complete ||
        status == domain::CityWorkStatus::Cancelled) {
        return {};
    }
    const domain::ErrorCode code = status == domain::CityWorkStatus::InvalidRequest ? domain::ErrorCode::InvalidData :
        status == domain::CityWorkStatus::SessionChanged ? domain::ErrorCode::StaleEpoch :
        status == domain::CityWorkStatus::Busy ? domain::ErrorCode::QueueFull : domain::ErrorCode::PlatformFailure;

    return std::unexpected(domain::Error{code, static_cast<std::uint32_t>(status)});
}

void service_city_management(Runtime& runtime, services::UiBridge& ui, std::uintptr_t module,
    const domain::WorkBudget& work_budget) {
    if (runtime.stopping || work_budget.exhausted()) { return; }
    own::shared_owner<const domain::CitySupportRoutes> routes = ui.take_city_support_routes();

    if (routes) {
        game::set_city_support_routes(runtime.city_management, std::move(routes));
    }
    const std::uintptr_t root = game::read_game_base(runtime.game_base_pointer);

    game::tick_city_management(runtime.city_management, module, root, game::read_player(root), runtime.epoch, GetTickCount64(), 4, work_budget);
    if (runtime.city_published_revision != runtime.city_management.snapshot.revision) {
        ui.city_management.publish(runtime.city_management.snapshot);
        runtime.city_published_revision = runtime.city_management.snapshot.revision;
    }
}

} // namespace s8::runtime
