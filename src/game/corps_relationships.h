#pragma once

#include "corps_deployment.h"

namespace s8::game {

void begin_corps_relationships(CorpsDeployment& state);
bool step_corps_relationships(CorpsDeployment& state);

} // namespace s8::game
