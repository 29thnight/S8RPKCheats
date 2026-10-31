#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace s8::platform::windows {

std::optional<std::filesystem::path> config_path(std::uintptr_t module);
std::optional<std::string> read_config(const std::filesystem::path& path);

} // namespace s8::platform::windows
