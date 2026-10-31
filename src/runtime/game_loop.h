#pragma once

#include "runtime.h"

#include <cstdint>
#include <stop_token>

namespace s8::runtime {

// The caller owns the thread and waits for this function to return before unloading.
void run_game_loop(Runtime& runtime, std::stop_token stop,
                   std::uintptr_t game_module);

} // namespace s8::runtime
