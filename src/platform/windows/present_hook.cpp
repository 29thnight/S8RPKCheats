#include "present_hook.h"

#include "../../../third_party/minhook/include/MinHook.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <dxgi.h>

#include <atomic>

namespace s8::platform::windows {
namespace {

using Present = HRESULT (STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT);
using ResizeBuffers = HRESULT (STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);

constexpr std::uint64_t CLOSED = std::uint64_t{1} << 63;
constexpr std::uint64_t COUNT_MASK = CLOSED - 1;

std::atomic<PresentHook*> active_hook{nullptr};
std::atomic<Present> original_present{nullptr};
std::atomic<ResizeBuffers> original_resize{nullptr};
std::atomic<std::uint64_t> callback_gate{CLOSED};
std::atomic<std::uint32_t> active_detours{0};
std::atomic_flag rendering{};
thread_local bool in_overlay_callback = false;

struct DetourScope {
    DetourScope() noexcept {
        active_detours.fetch_add(1, std::memory_order_acq_rel);
    }

    ~DetourScope() {
        active_detours.fetch_sub(1, std::memory_order_release);
    }
};

struct CallbackScope {
    PresentHook* state = nullptr;

    CallbackScope() noexcept {
        std::uint64_t observed = callback_gate.load(std::memory_order_acquire);

        while (!(observed & CLOSED)) {
            if (callback_gate.compare_exchange_weak(observed, observed + 1,
                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                state = active_hook.load(std::memory_order_acquire);
                return;
            }
        }
    }

    ~CallbackScope() {
        if (state) {
            callback_gate.fetch_sub(1, std::memory_order_release);
        }
    }
};

struct RenderScope {
    bool acquired = !rendering.test_and_set(std::memory_order_acquire);

    ~RenderScope() {
        if (acquired) {
            rendering.clear(std::memory_order_release);
            rendering.notify_all();
        }
    }

    void wait_for_resize() noexcept {
        // ResizeBuffers is a resource transition, not the per-frame path.
        // Present only tries the lock and skips the overlay while a resize runs.
        while (!acquired) {
            rendering.wait(true, std::memory_order_acquire);
            acquired = !rendering.test_and_set(std::memory_order_acquire);
        }
    }
};

struct OverlayScope {
    const bool previous = in_overlay_callback;

    OverlayScope() noexcept {
        in_overlay_callback = true;
    }

    ~OverlayScope() {
        in_overlay_callback = previous;
    }
};

HRESULT STDMETHODCALLTYPE present_detour(IDXGISwapChain* swap_chain, UINT sync_interval, UINT flags) {
    const DetourScope detour;

    {
        RenderScope render;

        if (render.acquired) {
            const CallbackScope callback;

            if (callback.state && callback.state->callbacks.present) {
                const OverlayScope overlay;

                callback.state->callbacks.present(callback.state->callbacks.context, swap_chain);
            }
        }
    }

    const Present original = original_present.load(std::memory_order_acquire);

    if (!original) {
        return E_UNEXPECTED;
    }

    const HRESULT result = original(swap_chain, sync_interval, flags);

    if (FAILED(result)) {
        RenderScope render;

        if (render.acquired) {
            const CallbackScope callback;

            if (callback.state && callback.state->callbacks.failed_present) {
                const OverlayScope overlay;

                callback.state->callbacks.failed_present(callback.state->callbacks.context,
                                                         swap_chain, static_cast<std::int32_t>(result));
            }
        }
    }
    return result;
}

HRESULT STDMETHODCALLTYPE resize_buffers_detour(IDXGISwapChain* swap_chain, UINT buffer_count,
                                                 UINT width, UINT height, DXGI_FORMAT format, UINT flags) {
    const DetourScope detour;
    const ResizeBuffers original = original_resize.load(std::memory_order_acquire);

    if (!original) {
        return E_UNEXPECTED;
    }
    if (in_overlay_callback) {
        return DXGI_ERROR_INVALID_CALL;
    }

    RenderScope render;

    render.wait_for_resize();

    const CallbackScope callback;
    const OverlayScope overlay;

    if (callback.state && callback.state->callbacks.before_resize) {
        callback.state->callbacks.before_resize(callback.state->callbacks.context, swap_chain);
    }

    // Keep the render gate until ResizeBuffers returns so another Present
    // cannot recreate an RTV between release and the actual buffer resize.
    return original(swap_chain, buffer_count, width, height, format, flags);
}

int disable_if_present(const std::optional<Hook>& hook) {
    if (!hook) {
        return MH_OK;
    }

    const int status = disable_hook(*hook);
    const MH_STATUS queued = MH_QueueDisableHook(hook->target);

    if (queued != MH_OK) {
        return static_cast<int>(queued);
    }
    return status == MH_ERROR_DISABLED ? MH_OK : status;
}

void discard_unpublished_hooks(PresentHook& state) {
    if (state.resize_hook) {
        remove_hook(*state.resize_hook);
        state.resize_hook.reset();
    }
    if (state.hook) {
        remove_hook(*state.hook);
        state.hook.reset();
    }
    state.callbacks = {};
}

} // namespace

std::expected<void, int> install_present_hook(PresentHook& state, Dx11PresentTargets targets,
                                              PresentCallbacks callbacks) {
    if (!targets.present || !callbacks.present || state.hook || state.resize_hook) {
        return std::unexpected(static_cast<int>(ERROR_INVALID_PARAMETER));
    }

    const std::expected<Hook, int> present = create_hook(
        targets.present, reinterpret_cast<void*>(&present_detour));

    if (!present) {
        return std::unexpected(present.error());
    }
    state.hook = *present;

    if (targets.resize_buffers) {
        const std::expected<Hook, int> resize = create_hook(
            targets.resize_buffers, reinterpret_cast<void*>(&resize_buffers_detour));

        if (!resize) {
            discard_unpublished_hooks(state);
            return std::unexpected(resize.error());
        }
        state.resize_hook = *resize;
    }

    PresentHook* expected = nullptr;

    state.callbacks = callbacks;
    if (!active_hook.compare_exchange_strong(expected, &state, std::memory_order_acq_rel)) {
        discard_unpublished_hooks(state);
        return std::unexpected(static_cast<int>(ERROR_ALREADY_INITIALIZED));
    }

    original_present.store(reinterpret_cast<Present>(state.hook->original), std::memory_order_release);
    original_resize.store(state.resize_hook ? reinterpret_cast<ResizeBuffers>(state.resize_hook->original)
                                            : nullptr, std::memory_order_release);
    callback_gate.store(0, std::memory_order_release);

    MH_STATUS status = MH_QueueEnableHook(state.hook->target);

    if (status == MH_OK && state.resize_hook) {
        status = MH_QueueEnableHook(state.resize_hook->target);
    }
    if (status == MH_OK) {
        status = MH_ApplyQueued();
    }
    if (status != MH_OK) {
        // Once an entry may have been published its trampoline is retained.
        // A failed start must use the same drain/retention path as normal stop.
        disable_present_hook(state);
        return std::unexpected(static_cast<int>(status));
    }

    state.enabled = true;
    return {};
}

std::expected<void, int> install_present_hook(PresentHook& state, void* target,
                                              PresentCallback callback, void* context) {
    return install_present_hook(state, Dx11PresentTargets{target, nullptr},
                                PresentCallbacks{callback, nullptr, nullptr, context});
}

int disable_present_hook(PresentHook& state) {
    if (active_hook.load(std::memory_order_acquire) == &state) {
        callback_gate.fetch_or(CLOSED, std::memory_order_acq_rel);
    }

    const int present_status = disable_if_present(state.hook);
    const int resize_status = disable_if_present(state.resize_hook);

    if (present_status == MH_OK && resize_status == MH_OK) {
        state.enabled = false;
    }
    return present_status != MH_OK ? present_status : resize_status;
}

bool present_callbacks_idle(const PresentHook&) noexcept {
    return (callback_gate.load(std::memory_order_acquire) & COUNT_MASK) == 0;
}

bool present_detours_idle(const PresentHook&) noexcept {
    return active_detours.load(std::memory_order_acquire) == 0;
}

bool detach_present_callbacks(PresentHook& state) noexcept {
    const std::uint64_t gate = callback_gate.load(std::memory_order_acquire);

    if (!(gate & CLOSED) || (gate & COUNT_MASK)) {
        return false;
    }

    PresentHook* expected = &state;

    if (!active_hook.compare_exchange_strong(expected, nullptr, std::memory_order_acq_rel) &&
        expected != nullptr) {
        return false;
    }
    state.callbacks = {};
    return true;
}

int remove_present_hook(PresentHook& state) {
    // The owner must additionally prove that no thread can still enter a
    // disabled detour. Counters alone are not permission to unload the DLL.
    if (state.enabled || !present_callbacks_idle(state) || !present_detours_idle(state) ||
        !(callback_gate.load(std::memory_order_acquire) & CLOSED)) {
        return static_cast<int>(ERROR_BUSY);
    }
    if (state.resize_hook) {
        const int status = remove_hook(*state.resize_hook);

        if (status != MH_OK) {
            return status;
        }
        state.resize_hook.reset();
        original_resize.store(nullptr, std::memory_order_release);
    }
    if (state.hook) {
        const int status = remove_hook(*state.hook);

        if (status != MH_OK) {
            return status;
        }
        state.hook.reset();
        original_present.store(nullptr, std::memory_order_release);
    }
    detach_present_callbacks(state);
    return MH_OK;
}

} // namespace s8::platform::windows
