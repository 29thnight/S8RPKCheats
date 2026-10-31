#pragma once

#include "../domain/work_budget.h"

#include "../domain/corps_deployment.h"
#include "../domain/error.h"

#include <cstdint>

namespace s8::services { class UiBridge; }

namespace s8::runtime {

struct Runtime;

domain::Result<void> submit_corps_request(Runtime& runtime, std::uintptr_t module, const domain::CorpsRequest& request);
void service_corps_deployment(Runtime& runtime, services::UiBridge& ui, std::uintptr_t module,
    const domain::WorkBudget& work_budget = {});

} // namespace s8::runtime
