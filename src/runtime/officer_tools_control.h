#pragma once

#include "../domain/error.h"
#include "../domain/officer_tools.h"

#include <cstdint>

namespace s8::services { class UiBridge; }

namespace s8::runtime {

struct Runtime;

domain::Result<void> submit_officer_tools_request(Runtime& runtime, services::UiBridge* ui,
    std::uintptr_t module, const domain::OfficerToolsRequest& request);
void service_officer_tools(Runtime& runtime, services::UiBridge& ui, std::uintptr_t module);

} // namespace s8::runtime
