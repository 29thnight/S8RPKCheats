#pragma once

#include "trait_text_service.h"

#include <array>
#include <filesystem>
#include <optional>

namespace s8::services {

class TraitCatalogService {
public:
    explicit TraitCatalogService(const std::filesystem::path& directory);
    void poll(UiBridge& bridge, TraitTextService& texts, bool force = false);

private:
    std::array<std::filesystem::path, 2> custom_paths;
    std::array<std::filesystem::path, 2> default_paths;
    std::filesystem::path external_runtime_path;
    std::filesystem::path observed_source;
    std::optional<std::filesystem::file_time_type> observed_write_time;
    std::uintmax_t observed_size = 0;
    bool observed_external = false;
    bool observed = false;
};

} // namespace s8::services
