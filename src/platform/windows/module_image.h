#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace s8::platform::windows {

std::optional<std::vector<std::byte>> read_module_image(std::uintptr_t module);

} // namespace s8::platform::windows
