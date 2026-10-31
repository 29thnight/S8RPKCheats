#pragma once

#include <expected>

namespace s8::platform::windows {

std::expected<void*, int> find_dx11_present_target();

} // namespace s8::platform::windows
