#pragma once

#include "../domain/tengi.h"

#include <cstdint>
#include <expected>

namespace s8::game {

std::expected<void, int> configure_tengi_filter(std::uintptr_t module_base,
                                               const domain::TengiSettings& settings);
int stop_tengi_filter(std::uintptr_t module_base);
bool tengi_filter_has_retained_hooks();

} // namespace s8::game
