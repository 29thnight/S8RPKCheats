#include "config_commit.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <algorithm>
#include <array>
#include <mutex>
#include <utility>

namespace s8::platform::windows {
namespace {

std::mutex writer_lease;

} // namespace

domain::Result<std::optional<std::string>> read_config_for_save(const std::filesystem::path& path) {
    const auto file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                  nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        const auto error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) {
            return std::optional<std::string>{};
        }
        return std::unexpected(domain::Error{domain::ErrorCode::PlatformFailure, error});
    }
    std::string text;
    std::array<char, 16384> buffer{};
    DWORD count = 0;
    DWORD read_error = ERROR_SUCCESS;
    while (true) {
        if (!ReadFile(file, buffer.data(), static_cast<DWORD>(buffer.size()), &count, nullptr)) {
            read_error = GetLastError();
            break;
        }
        if (!count) {
            break;
        }
        text.append(buffer.data(), count);
    }
    const bool closed = CloseHandle(file) != FALSE;
    if (read_error != ERROR_SUCCESS || !closed) {
        return std::unexpected(domain::Error{domain::ErrorCode::PlatformFailure,
                                             read_error != ERROR_SUCCESS ? read_error : GetLastError()});
    }
    return std::optional<std::string>{std::move(text)};
}

ConfigCommitResult commit_config(const std::filesystem::path& path,
                                 const std::optional<std::string>& baseline,
                                 std::string_view bytes, std::stop_token stop) {
    std::lock_guard lease(writer_lease);
    if (stop.stop_requested()) {
        return {ConfigCommitStatus::Cancelled};
    }
    std::array<wchar_t, MAX_PATH> temporary{};
    if (!GetTempFileNameW(path.parent_path().c_str(), L"s8c", 0, temporary.data())) {
        return {ConfigCommitStatus::Failed, GetLastError()};
    }
    const std::filesystem::path temp_path(temporary.data());
    const auto file = CreateFileW(temp_path.c_str(), GENERIC_WRITE, 0, nullptr, TRUNCATE_EXISTING,
                                  FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        const auto error = GetLastError();
        DeleteFileW(temp_path.c_str());
        return {ConfigCommitStatus::Failed, error};
    }
    DWORD error = ERROR_SUCCESS;
    while (!bytes.empty()) {
        const auto chunk = static_cast<DWORD>(std::min<std::size_t>(bytes.size(), 1u << 20));
        DWORD written = 0;
        if (!WriteFile(file, bytes.data(), chunk, &written, nullptr) || written != chunk) {
            error = GetLastError();
            if (error == ERROR_SUCCESS) {
                error = ERROR_WRITE_FAULT;
            }
            break;
        }
        bytes.remove_prefix(written);
    }
    if (error == ERROR_SUCCESS && !FlushFileBuffers(file)) {
        error = GetLastError();
    }
    if (!CloseHandle(file) && error == ERROR_SUCCESS) {
        error = GetLastError();
    }
    if (error != ERROR_SUCCESS || stop.stop_requested()) {
        DeleteFileW(temp_path.c_str());
        return error != ERROR_SUCCESS ? ConfigCommitResult{ConfigCommitStatus::Failed, error}
                                      : ConfigCommitResult{ConfigCommitStatus::Cancelled};
    }
    const auto current = read_config_for_save(path);
    if (!current) {
        DeleteFileW(temp_path.c_str());
        return {ConfigCommitStatus::Failed, current.error().native_code};
    }
    if (*current != baseline) {
        DeleteFileW(temp_path.c_str());
        return {ConfigCommitStatus::Conflict};
    }
    if (stop.stop_requested()) {
        DeleteFileW(temp_path.c_str());
        return {ConfigCommitStatus::Cancelled};
    }
    const BOOL replaced = baseline ? ReplaceFileW(path.c_str(), temp_path.c_str(), nullptr, 0, nullptr, nullptr)
                                   : MoveFileExW(temp_path.c_str(), path.c_str(), 0);
    if (!replaced) {
        return {ConfigCommitStatus::Failed, GetLastError(), temp_path};
    }
    return {ConfigCommitStatus::Completed};
}

} // namespace s8::platform::windows
