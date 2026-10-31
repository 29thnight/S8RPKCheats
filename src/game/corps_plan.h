#pragma once

#include "corps_deployment.h"

namespace s8::game {

void begin_corps_sort(CorpsDeployment& state, CorpsSort kind, CorpsStage next);
bool step_corps_sort(CorpsDeployment& state);
void begin_corps_plan(CorpsDeployment& state);
bool step_corps_plan(CorpsDeployment& state);
std::uint16_t corps_group_root(CorpsDeployment& state, std::uint16_t row);
void unite_corps_group(CorpsDeployment& state, std::uint16_t first, std::uint16_t second);

} // namespace s8::game
