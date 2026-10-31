#include "legacy_config.h"

#include <charconv>
#include <cctype>
#include <system_error>

namespace s8::game {
namespace {

std::string_view find_line(std::string_view text, std::string_view key) {
    while (!text.empty()) {
        const auto end = text.find('\n');
        const auto line = text.substr(0, end);
        if (line.find(key) != std::string_view::npos)
            return line;
        if (end == std::string_view::npos)
            break;
        text.remove_prefix(end + 1);
    }
    return {};
}

} // namespace

std::optional<bool> read_legacy_bool(std::string_view text, std::string_view quoted_key) {
    const auto line = find_line(text, quoted_key);
    if (line.empty())
        return std::nullopt;
    return line.find("true") != std::string_view::npos;
}

std::optional<int> read_legacy_int(std::string_view text, std::string_view quoted_key) {
    auto line = find_line(text, quoted_key);
    const auto colon = line.find(':');
    if (colon == std::string_view::npos)
        return std::nullopt;
    line.remove_prefix(colon + 1);
    while (!line.empty() && std::isspace(static_cast<unsigned char>(line.front())))
        line.remove_prefix(1);
    if (!line.empty() && line.front() == '+') {
        line.remove_prefix(1);
        if (line.empty() || line.front() == '-' || line.front() == '+')
            return std::nullopt;
    }
    if (line.empty())
        return std::nullopt;
    int value = 0;
    const auto result = std::from_chars(line.data(), line.data() + line.size(), value);
    if (result.ec != std::errc{})
        return std::nullopt;
    return value;
}

std::optional<float> read_legacy_float(std::string_view text, std::string_view quoted_key) {
    auto line = find_line(text, quoted_key);
    const auto colon = line.find(':');
    if (colon == std::string_view::npos) {
        return std::nullopt;
    }
    line.remove_prefix(colon + 1);
    while (!line.empty() && std::isspace(static_cast<unsigned char>(line.front()))) {
        line.remove_prefix(1);
    }
    float value = 0;
    const auto result = std::from_chars(line.data(), line.data() + line.size(), value);
    if (result.ec != std::errc{}) {
        return std::nullopt;
    }
    return value;
}

} // namespace s8::game
