#pragma once

#include "config_save_queue.h"
#include "../domain/error.h"
#include "../platform/windows/config_commit.h"

#include <filesystem>
#include <optional>
#include <stop_token>
#include <string>

namespace s8::services {

enum class ConfigSaveStatus { Completed, Cancelled, Conflict, InvalidData, Failed };

struct ConfigSaveResult {
    std::uint64_t revision;
    ConfigSaveStatus status;
    std::uint32_t native_code = 0;
    std::filesystem::path retained_temp;
    std::optional<std::string> persisted_content;
};

// Run on a file worker. Baseline is the file content used to create the snapshot.
ConfigSaveResult save_config(const std::filesystem::path& path, const std::optional<std::string>& baseline,
                             const ConfigSave& save, std::stop_token stop);

} // namespace s8::services
