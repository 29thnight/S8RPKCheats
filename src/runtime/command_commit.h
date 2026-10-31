#pragma once

#include "runtime.h"
#include "../services/command_mailbox.h"
#include "../services/ui_bridge.h"

#include <chrono>
#include <cstddef>

namespace s8::runtime {

// Invoke only from the verified game commit context. Completion acknowledges
// a setting request; deferred game effects have their own lifecycle.
std::size_t commit_commands(Runtime& runtime, services::CommandMailbox& mailbox,
                            std::uintptr_t game_module, std::size_t max_commands,
                            std::chrono::steady_clock::duration time_budget,
                            services::UiBridge* ui = nullptr);
std::size_t commit_ui_commands(Runtime& runtime, services::UiBridge& ui,
                               std::uintptr_t game_module, std::size_t max_commands,
                               std::chrono::steady_clock::duration time_budget);

} // namespace s8::runtime
