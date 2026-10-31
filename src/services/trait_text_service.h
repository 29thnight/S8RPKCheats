#pragma once

#include "ui_bridge.h"

#include <filesystem>
#include <optional>
#include <string>

namespace s8::services {

// All file baselines and paths stay on the configuration service thread.
class TraitTextService {
public:
    explicit TraitTextService(std::filesystem::path directory);
    void load_legacy(UiBridge& bridge);
    void set_embedded_source(std::filesystem::path source, std::string contents);
    void clear_embedded_source();
    void poll(UiBridge& bridge);

private:
    std::filesystem::path legacy_path;
    std::optional<std::string> legacy_baseline;
    std::filesystem::path embedded_path;
    std::optional<std::string> embedded_baseline;
    bool legacy_loaded = false;
};

} // namespace s8::services
