#pragma once

#include "../domain/config_snapshot.h"

#include <cstdint>
#include <optional>

namespace s8::services {

struct ConfigSave {
    std::uint64_t revision;
    domain::ConfigSnapshot snapshot;
};

// Owned by the coordinator; worker threads return completion to that owner.
class ConfigSaveQueue {
public:
    std::uint64_t submit(domain::ConfigSnapshot snapshot);
    std::optional<ConfigSave> start_next();
    bool finish(std::uint64_t revision);
    bool has_pending() const;

private:
    std::uint64_t next_revision = 0;
    std::optional<std::uint64_t> running;
    std::optional<ConfigSave> pending;
};

} // namespace s8::services
