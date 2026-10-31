#pragma once

#include "../domain/city_management.h"
#include "../domain/error.h"

#include <string>
#include <string_view>

namespace s8::services {

// Original S8RPK_rear_support_routes.json schema, including integer booleans.
domain::Result<domain::CitySupportRoutes> read_city_support_config(std::string_view text);
domain::Result<std::string> write_city_support_config(const domain::CitySupportRoutes& routes);

} // namespace s8::services
