#pragma once

#include "../domain/work_budget.h"

#include "../game/native_officer_names.h"

namespace s8::services { class UiBridge; }

namespace s8::runtime {

struct Runtime;

void update_officer_names(game::NativeOfficerNames& state, Runtime& runtime,
    services::UiBridge& bridge, std::uintptr_t module, std::uint64_t now_ms,
    const domain::WorkBudget& work_budget = {});

} // namespace s8::runtime
