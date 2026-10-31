#pragma once

#include <string>
#include <string_view>

namespace s8::platform::windows {

std::string normalize_utf8(const std::string& text);
std::string utf8_from_system_code_page(std::string_view text);

} // namespace s8::platform::windows
