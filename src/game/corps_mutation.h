#pragma once

#include "corps_deployment.h"

#include <span>

namespace s8::game {

void begin_corps_journal(CorpsDeployment& state);
bool restore_corps_journal_unit(CorpsDeployment& state);
bool refresh_corps_live_row(CorpsDeployment& state, std::uint16_t index);
domain::CorpsStatus move_corps_officer(CorpsDeployment& state, std::uint16_t index, std::uint8_t target, bool allow_cross_corps);
domain::CorpsStatus swap_corps_governor(CorpsDeployment& state, std::uint8_t city, std::uint16_t candidate);
domain::CorpsStatus appoint_corps_governor(CorpsDeployment& state, std::uint8_t city, std::uint16_t candidate);
domain::CorpsStatus swap_corps_general(CorpsDeployment& state, std::uint16_t previous, std::uint16_t candidate);

} // namespace s8::game
