#include "unicode.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <winnls.h>

#pragma comment(lib, "Normaliz.lib")

namespace s8::platform::windows {

std::string utf8_from_system_code_page(std::string_view text) {
    if (text.empty() || MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                                            static_cast<int>(text.size()), nullptr, 0) > 0) {
        return std::string(text);
    }
    const auto wide_size = MultiByteToWideChar(CP_ACP, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (wide_size <= 0) {
        return std::string(text);
    }
    std::wstring wide(static_cast<std::size_t>(wide_size), L'\0');
    MultiByteToWideChar(CP_ACP, 0, text.data(), static_cast<int>(text.size()), wide.data(), wide_size);
    const auto utf8_size = WideCharToMultiByte(CP_UTF8, 0, wide.data(), wide_size, nullptr, 0, nullptr, nullptr);
    if (utf8_size <= 0) {
        return std::string(text);
    }
    std::string result(static_cast<std::size_t>(utf8_size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), wide_size, result.data(), utf8_size, nullptr, nullptr);
    return result;
}

std::string normalize_utf8(const std::string& text) {
    if (text.empty()) {
        return {};
    }
    const auto wide_size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    if (wide_size <= 0) {
        return text;
    }
    std::wstring wide(static_cast<std::size_t>(wide_size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, wide.data(), wide_size);
    const auto normalized_size = NormalizeString(NormalizationC, wide.c_str(), -1, nullptr, 0);
    if (normalized_size <= 0) {
        return text;
    }
    std::wstring normalized(static_cast<std::size_t>(normalized_size), L'\0');
    NormalizeString(NormalizationC, wide.c_str(), -1, normalized.data(), normalized_size);
    const auto utf8_size = WideCharToMultiByte(CP_UTF8, 0, normalized.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (utf8_size <= 0) {
        return text;
    }
    std::string result(static_cast<std::size_t>(utf8_size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, normalized.c_str(), -1, result.data(), utf8_size, nullptr, nullptr);
    result.resize(static_cast<std::size_t>(utf8_size - 1));
    return result;
}

} // namespace s8::platform::windows
