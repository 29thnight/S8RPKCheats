#include "config_save_pump.h"

#include <utility>

namespace s8::services {

std::optional<ConfigSaveJob> ConfigSavePump::next(UiBridge& bridge) {
    if (!coordinator) {
        auto baseline = bridge.take_config_baseline();
        if (!baseline) {
            return std::nullopt;
        }
        coordinator.emplace(std::move(baseline->contents));
    }
    if (auto snapshot = bridge.take_config_save()) {
        coordinator->submit(std::move(*snapshot));
    }
    if (bridge.take_config_retry()) {
        coordinator->retry_failure();
    }
    return coordinator->start_next();
}

void ConfigSavePump::finish(UiBridge& bridge, ConfigSaveResult result) {
    const ConfigSaveNotice notice{result.revision, result.status, result.native_code};
    if (coordinator->finish(std::move(result))) {
        bridge.publish_config_notice(notice);
    }
}

bool ConfigSavePump::failed() const {
    return coordinator && coordinator->failure().has_value();
}

} // namespace s8::services
