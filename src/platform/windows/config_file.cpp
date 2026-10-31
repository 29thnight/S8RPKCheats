#include "config_file.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <array>
#include <fstream>
#include <iterator>

namespace s8::platform::windows {

std::optional<std::filesystem::path> config_path(std::uintptr_t module) {
    std::array<wchar_t, 32768> path{};
    const auto length = GetModuleFileNameW(reinterpret_cast<HMODULE>(module), path.data(),
                                           static_cast<DWORD>(path.size()));
    if (length == 0 || length >= path.size())
        return std::nullopt;
    return std::filesystem::path(path.data()).parent_path() / L"S8RPK_cheat_config.json";
}

std::optional<std::string> read_config(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file)
        return std::nullopt;
    std::string text{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    if (file.bad())
        return std::nullopt;
    return text;
}

} // namespace s8::platform::windows
