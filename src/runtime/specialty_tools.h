#pragma once

#include "../game/specialty_tools.h"

namespace s8::services { class UiBridge; }

namespace s8::runtime {

struct Runtime;

void update_specialty_tools(game::SpecialtyTools& tools, Runtime& runtime,
    services::UiBridge& bridge, std::uintptr_t module);
void stop_specialty_tools(game::SpecialtyTools& tools, services::UiBridge& bridge);

} // namespace s8::runtime
