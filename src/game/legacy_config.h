#pragma once

#include <optional>
#include <string_view>

namespace s8::game {

std::optional<bool> read_legacy_bool(std::string_view text, std::string_view quoted_key);
std::optional<int> read_legacy_int(std::string_view text, std::string_view quoted_key);
std::optional<float> read_legacy_float(std::string_view text, std::string_view quoted_key);

} // namespace s8::game
