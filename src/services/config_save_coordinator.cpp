#include "config_save_coordinator.h"

#include <utility>

namespace s8::services {

ConfigSaveCoordinator::ConfigSaveCoordinator(std::optional<std::string> baseline)
    : baseline(std::move(baseline)) {}

std::uint64_t ConfigSaveCoordinator::submit(domain::ConfigSnapshot snapshot) {
    return queue.submit(std::move(snapshot));
}

std::optional<ConfigSaveJob> ConfigSaveCoordinator::start_next() {
    if (last_failure) {
        return std::nullopt;
    }
    auto save = queue.start_next();
    if (!save) {
        return std::nullopt;
    }
    running_snapshot = save->snapshot;
    return ConfigSaveJob{std::move(*save), baseline};
}

bool ConfigSaveCoordinator::finish(ConfigSaveResult result) {
    if (!queue.finish(result.revision)) {
        return false;
    }
    if (result.status == ConfigSaveStatus::Completed) {
        baseline = std::move(result.persisted_content);
        persisted = result.revision;
    } else {
        if (!queue.has_pending()) {
            queue.submit(std::move(*running_snapshot));
        }
        last_failure = std::move(result);
    }
    running_snapshot.reset();
    return true;
}

bool ConfigSaveCoordinator::retry_failure() {
    if (!last_failure || (last_failure->status != ConfigSaveStatus::Failed &&
                          last_failure->status != ConfigSaveStatus::Cancelled)) {
        return false;
    }
    last_failure.reset();
    return true;
}

const std::optional<ConfigSaveResult>& ConfigSaveCoordinator::failure() const {
    return last_failure;
}

std::uint64_t ConfigSaveCoordinator::persisted_revision() const {
    return persisted;
}

} // namespace s8::services
