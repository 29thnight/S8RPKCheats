#pragma once

#include "ui_bridge.h"

#include <filesystem>
#include <stop_token>

namespace s8::services {

// Run on the coordinator thread. Stop producers before requesting stop.
bool run_config_service(UiBridge& bridge, const std::filesystem::path& path,
                        std::optional<std::string> skill_baseline, std::stop_token stop);

} // namespace s8::services
