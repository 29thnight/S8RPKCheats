#include "config_save_queue.h"

#include <utility>

namespace s8::services {

std::uint64_t ConfigSaveQueue::submit(domain::ConfigSnapshot snapshot) {
    const auto revision = ++next_revision;
    pending = ConfigSave{revision, std::move(snapshot)};
    return revision;
}

std::optional<ConfigSave> ConfigSaveQueue::start_next() {
    if (running || !pending) {
        return std::nullopt;
    }
    auto save = std::move(pending);
    pending.reset();
    running = save->revision;
    return save;
}

bool ConfigSaveQueue::finish(std::uint64_t revision) {
    if (!running || *running != revision) {
        return false;
    }
    running.reset();
    return true;
}

bool ConfigSaveQueue::has_pending() const {
    return pending.has_value();
}

} // namespace s8::services
