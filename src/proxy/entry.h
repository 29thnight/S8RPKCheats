#pragma once

#include <cstdint>

namespace s8::proxy {

std::uintptr_t module_handle();
bool stop_requested();

} // namespace s8::proxy
