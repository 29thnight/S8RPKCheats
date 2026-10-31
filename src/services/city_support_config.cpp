#include "city_support_config.h"

#include "../../third_party/nlohmann_json/single_include/nlohmann/json.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <sstream>

namespace s8::services {
namespace {

using Json = nlohmann::json;

bool boolean(const Json& object, const char* key, bool fallback) {
    const Json::const_iterator field = object.find(key);

    if (field == object.end()) {
        return fallback;
    }
    if (field->is_boolean()) {
        return field->get<bool>();
    }
    return field->is_number_integer() ? *field != 0 : fallback;
}

std::uint32_t amount(const Json& object, const char* key, bool percent) {
    const Json::const_iterator field = object.find(key);

    if (field == object.end() || !field->is_number_integer()) {
        return 0;
    }
    std::uint64_t value = 0;

    if (field->is_number_unsigned()) {
        value = field->get<std::uint64_t>();
    } else {
        const std::int64_t signed_value = field->get<std::int64_t>();

        value = signed_value < 0 ? 0 : static_cast<std::uint64_t>(signed_value);
    }
    const std::uint64_t maximum = percent ? 100 : std::numeric_limits<std::uint32_t>::max();

    return static_cast<std::uint32_t>(std::min(value, maximum));
}

bool city_id(const Json& object, const char* key, std::uint8_t& output) {
    const Json::const_iterator field = object.find(key);

    if (field == object.end() || !field->is_number_integer() || *field < 0 || *field >= domain::CITY_COUNT) {
        return false;
    }
    output = field->get<std::uint8_t>();
    return true;
}

} // namespace

domain::Result<domain::CitySupportRoutes> read_city_support_config(std::string_view text) {
    const Json json = Json::parse(text, nullptr, false);

    if (!json.is_object() || !json.contains("routes") || !json["routes"].is_array()) {
        return std::unexpected(domain::Error{domain::ErrorCode::InvalidData});
    }
    domain::CitySupportRoutes result;
    const Json& source = json["routes"];

    result.yearly_auto = boolean(json, "yearly_auto", false);
    result.routes.reserve(source.size());
    for (const Json& row : source) {
        domain::CitySupportRoute route;

        if (!row.is_object() || !city_id(row, "source", route.source) || !city_id(row, "target", route.target) ||
            route.source == route.target) {
            continue;
        }
        const bool percent = row.contains("mode") && row["mode"].is_number_integer() && row["mode"] == 1;

        route.enabled = boolean(row, "enabled", true);
        route.mode = percent ? domain::CitySupportMode::Percent : domain::CitySupportMode::Fixed;
        route.gold = amount(row, "gold", percent);
        route.grain = amount(row, "grain", percent);
        route.troops = amount(row, "troops", percent);
        result.routes.push_back(route);
    }
    return result;
}

domain::Result<std::string> write_city_support_config(const domain::CitySupportRoutes& routes) {
    std::ostringstream output;

    output << "{\n  \"yearly_auto\": " << (routes.yearly_auto ? 1 : 0) << ",\n  \"routes\": [\n";
    for (std::size_t index = 0; index < routes.routes.size(); ++index) {
        const domain::CitySupportRoute& route = routes.routes[index];

        if (!domain::valid_city_route(route)) {
            return std::unexpected(domain::Error{domain::ErrorCode::InvalidData});
        }
        output << "    {\"enabled\": " << (route.enabled ? 1 : 0)
            << ", \"source\": " << static_cast<unsigned>(route.source)
            << ", \"target\": " << static_cast<unsigned>(route.target)
            << ", \"mode\": " << (route.mode == domain::CitySupportMode::Percent ? 1 : 0)
            << ", \"gold\": " << route.gold << ", \"grain\": " << route.grain
            << ", \"troops\": " << route.troops << "}" << (index + 1 < routes.routes.size() ? ",\n" : "\n");
    }
    output << "  ]\n}\n";
    return output.str();
}

} // namespace s8::services
