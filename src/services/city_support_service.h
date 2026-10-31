#pragma once

#include "ui_bridge.h"

#include <filesystem>
#include <optional>
#include <string>

namespace s8::services {

// Only the configuration worker owns paths, parsing and disk I/O.
class CitySupportService {
public:
    explicit CitySupportService(std::filesystem::path directory);
    void load(UiBridge& bridge);
    void poll(UiBridge& bridge);
    bool saved() const noexcept;

private:
    std::filesystem::path path;
    std::optional<std::string> baseline;
    bool loaded = false;
    bool save_failed = false;
};

} // namespace s8::services
