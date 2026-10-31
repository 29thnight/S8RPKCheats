#pragma once

#include <cstddef>
#include <cstdint>

namespace s8::platform::windows {

std::uintptr_t allocate_code_near(std::uintptr_t target, std::size_t size);
void free_code(std::uintptr_t address);

} // namespace s8::platform::windows
