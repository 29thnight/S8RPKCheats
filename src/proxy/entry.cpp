#include "entry.h"
#include "../runtime/game_loop.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <atomic>

namespace {
std::atomic<std::uintptr_t> module{0};
std::atomic<bool> stopping{false};
}

namespace s8::proxy {

std::uintptr_t module_handle() {
    return module.load();
}

bool stop_requested() {
    return stopping.load();
}

} // namespace s8::proxy

BOOL APIENTRY DllMain(HMODULE module_handle, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module_handle);
        module.store(reinterpret_cast<std::uintptr_t>(module_handle));
        s8::runtime::start_runtime_async(reinterpret_cast<std::uintptr_t>(module_handle));
    } else if (reason == DLL_PROCESS_DETACH) {
        stopping.store(true);
    }
    return TRUE;
}
