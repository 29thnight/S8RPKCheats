#include "game_loop.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <chrono>
#include <thread>

namespace s8::runtime {

void run_game_loop(Runtime& runtime, std::stop_token stop,
                   std::uintptr_t game_module) {
    initialize_game_base_from_module(runtime, game_module);
    int attempts = 0;
    auto next_attempt = GetTickCount64();
    bool retry_done = false;
    while (!stop.stop_requested()) {
        const auto now = GetTickCount64();
        if (!retry_done && now >= next_attempt) {
            ++attempts;
            retry_done = initialize_game_base_from_module(runtime, game_module) || attempts >= 60;
            next_attempt = now + 500;
        }
        tick_game(runtime, game_module, now);
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }
}

} // namespace s8::runtime
