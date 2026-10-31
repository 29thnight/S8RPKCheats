#pragma once

#include "../domain/config_snapshot.h"

#include <string_view>
#include <string>
#include <variant>
#include <vector>

namespace s8::services {

struct ConfigChange {
    std::string_view key;
    std::variant<bool, int, float, std::string> value;
};

std::vector<ConfigChange> config_changes(const domain::ConfigSnapshot& snapshot);

} // namespace s8::services
