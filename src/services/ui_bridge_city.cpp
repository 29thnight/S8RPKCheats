#include "ui_bridge.h"

#include <utility>

namespace s8::services {

void UiBridge::publish_city_support_routes(own::shared_owner<const domain::CitySupportRoutes> routes) {
    {
        std::lock_guard lock(mutex);

        pending_city_support_routes.swap(routes);
    }
}

own::shared_owner<const domain::CitySupportRoutes> UiBridge::take_city_support_routes() {
    own::shared_owner<const domain::CitySupportRoutes> routes;

    {
        std::lock_guard lock(mutex);

        pending_city_support_routes.swap(routes);
    }
    return routes;
}

bool UiBridge::request_city_support_save(own::shared_owner<const domain::CitySupportRoutes> routes) {
    {
        std::lock_guard lock(mutex);

        if (!officer_name_requests_open || !routes || pending_city_support_save) {
            return false;
        }
        pending_city_support_save.swap(routes);
    }
    config_wake.signal();
    return true;
}

own::shared_owner<const domain::CitySupportRoutes> UiBridge::take_city_support_save() {
    own::shared_owner<const domain::CitySupportRoutes> routes;

    {
        std::lock_guard lock(mutex);

        pending_city_support_save.swap(routes);
    }
    return routes;
}

void UiBridge::publish_city_support_save_result(ConfigSaveStatus result) {
    std::lock_guard lock(mutex);

    city_support_save_result = result;
}

std::optional<ConfigSaveStatus> UiBridge::take_city_support_save_result() {
    std::lock_guard lock(mutex);

    return std::exchange(city_support_save_result, std::nullopt);
}

void UiBridge::request_city_support_reload() {
    {
        std::lock_guard lock(mutex);

        if (!officer_name_requests_open) {
            return;
        }
        city_support_reload = true;
    }
    config_wake.signal();
}

bool UiBridge::take_city_support_reload() {
    std::lock_guard lock(mutex);

    return std::exchange(city_support_reload, false);
}

} // namespace s8::services
