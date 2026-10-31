#pragma once

#include "config_save.h"

namespace s8::services {

struct ConfigSaveJob {
    ConfigSave save;
    std::optional<std::string> baseline;
};

// All methods are called by the coordinator, not the file worker.
class ConfigSaveCoordinator {
public:
    explicit ConfigSaveCoordinator(std::optional<std::string> baseline);
    std::uint64_t submit(domain::ConfigSnapshot snapshot);
    std::optional<ConfigSaveJob> start_next();
    bool finish(ConfigSaveResult result);
    bool retry_failure();
    const std::optional<ConfigSaveResult>& failure() const;
    std::uint64_t persisted_revision() const;

private:
    ConfigSaveQueue queue;
    std::optional<std::string> baseline;
    std::optional<ConfigSaveResult> last_failure;
    std::optional<domain::ConfigSnapshot> running_snapshot;
    std::uint64_t persisted = 0;
};

} // namespace s8::services
