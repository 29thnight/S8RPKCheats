#pragma once

#include "../domain/city_management.h"
#include "../domain/error.h"

#include <cstdint>

namespace s8::services { class UiBridge; }

namespace s8::runtime {

struct Runtime;

domain::Result<void> submit_city_request(Runtime& runtime, std::uintptr_t module, const domain::CityRequest& request);
void service_city_management(Runtime& runtime, services::UiBridge& ui, std::uintptr_t module);

} // namespace s8::runtime
