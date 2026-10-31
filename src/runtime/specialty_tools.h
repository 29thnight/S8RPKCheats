#pragma once

#include "../domain/work_budget.h"

#include "../game/specialty_tools.h"

namespace s8::services { class UiBridge; }

namespace s8::runtime {

struct Runtime;

void update_specialty_tools(game::SpecialtyTools& tools, Runtime& runtime,
    services::UiBridge& bridge, std::uintptr_t module, const domain::WorkBudget& work_budget = {});
void stop_specialty_tools(game::SpecialtyTools& tools, services::UiBridge& bridge);

} // namespace s8::runtime
