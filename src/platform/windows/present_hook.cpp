#include "present_hook.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <dxgi.h>

#include <atomic>

namespace s8::platform::windows {
namespace {

using Present = HRESULT (STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT);
std::atomic<PresentHook*> active_hook{nullptr};

HRESULT STDMETHODCALLTYPE present_detour(IDXGISwapChain* swap_chain, UINT sync_interval, UINT flags) {
    auto& state = *active_hook.load();
    state.callback(state.context, swap_chain);
    const auto original = reinterpret_cast<Present>(state.hook->original);
    return original(swap_chain, sync_interval, flags);
}

} // namespace

std::expected<void, int> install_present_hook(PresentHook& state, void* target,
                                              PresentCallback callback, void* context) {
    if (!target || !callback)
        return std::unexpected(-1);
    auto hook = create_hook(target, reinterpret_cast<void*>(&present_detour));
    if (!hook)
        return std::unexpected(hook.error());
    state.hook = *hook;
    state.callback = callback;
    state.context = context;
    PresentHook* expected = nullptr;
    if (!active_hook.compare_exchange_strong(expected, &state)) {
        remove_hook(*state.hook);
        state.hook.reset();
        return std::unexpected(-1);
    }
    const auto status = enable_hook(*state.hook);
    if (status != 0) {
        remove_hook(*state.hook);
        active_hook.store(nullptr);
        state.hook.reset();
        return std::unexpected(status);
    }
    return {};
}

int disable_present_hook(PresentHook& state) {
    return disable_hook(*state.hook);
}

int remove_present_hook(PresentHook& state) {
    const auto status = remove_hook(*state.hook);
    if (status == 0) {
        active_hook.store(nullptr);
        state.hook.reset();
    }
    return status;
}

} // namespace s8::platform::windows
