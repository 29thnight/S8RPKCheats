#pragma once

#include "../../domain/error.h"

#include <filesystem>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>

namespace s8::platform::windows {

enum class ConfigCommitStatus { Completed, Cancelled, Conflict, Failed };

struct ConfigCommitResult {
    ConfigCommitStatus status;
    std::uint32_t native_code = 0;
    std::filesystem::path retained_temp;
};

domain::Result<std::optional<std::string>> read_config_for_save(const std::filesystem::path& path);
ConfigCommitResult commit_config(const std::filesystem::path& path,
                                 const std::optional<std::string>& baseline,
                                 std::string_view bytes, std::stop_token stop);

} // namespace s8::platform::windows
