#pragma once

#include "config_save_coordinator.h"
#include "ui_bridge.h"

namespace s8::services {

// Called by the coordinator; the returned job is executed by a file worker.
class ConfigSavePump {
public:
    std::optional<ConfigSaveJob> next(UiBridge& bridge);
    void finish(UiBridge& bridge, ConfigSaveResult result);
    bool failed() const;

private:
    std::optional<ConfigSaveCoordinator> coordinator;
};

} // namespace s8::services
