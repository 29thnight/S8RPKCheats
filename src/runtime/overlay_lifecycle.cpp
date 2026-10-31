#include "overlay_lifecycle.h"

#include "../platform/windows/dx11_present_target.h"
#include "../platform/windows/hook.h"
#include "../overlay/window_subclass.h"
#include "../../third_party/minhook/include/MinHook.h"

namespace s8::runtime {

std::expected<void, int> start_overlay(OverlayLifecycle& state, HWND game_window) {
    DWORD process = 0;
    if (!GetWindowThreadProcessId(game_window, &process) || process != GetCurrentProcessId()) {
        return std::unexpected(static_cast<int>(ERROR_INVALID_WINDOW_HANDLE));
    }
    const std::expected<platform::windows::Dx11PresentTargets, int> targets =
        platform::windows::find_dx11_present_targets();

    if (!targets) {
        return std::unexpected(targets.error());
    }
    const int initialized = platform::windows::initialize_hooks();

    if (initialized != MH_OK && initialized != MH_ERROR_ALREADY_INITIALIZED) {
        return std::unexpected(initialized);
    }

    state.overlay.target_window = game_window;
    const std::expected<void, int> captured = overlay::install_input_capture(
        state.overlay.input_capture, game_window, state.overlay.capture_mouse,
        state.overlay.capture_keyboard);

    if (!captured) {
        return std::unexpected(captured.error());
    }

    const platform::windows::PresentCallbacks callbacks{
        overlay::on_overlay_present, overlay::on_overlay_before_resize,
        overlay::on_overlay_present_failed, &state.overlay};
    const std::expected<void, int> installed =
        platform::windows::install_present_hook(state.present, *targets, callbacks);

    if (!installed) {
        overlay::disable_input_capture(state.overlay.input_capture);
        return std::unexpected(installed.error());
    }
    return {};
}

int stop_overlay_entries(OverlayLifecycle& state) {
    state.bridge.close_officer_name_requests();
    state.bridge.debug_tools.close();
    state.bridge.commands.close();
    while (std::optional<services::AcceptedCommand> command = state.bridge.commands.pop()) {
        state.bridge.commands.complete(command->completion,
            std::unexpected(domain::Error{domain::ErrorCode::Cancelled}));
    }
    overlay::request_overlay_stop(state.overlay);
    return MH_OK;
}

int finish_overlay_stop(OverlayLifecycle& state) {
    if (!state.overlay.render_stopped.load(std::memory_order_acquire)) {
        return static_cast<int>(ERROR_BUSY);
    }

    const int present_status = platform::windows::disable_present_hook(state.present);
    const int input_status = overlay::disable_input_capture(state.overlay.input_capture);

    if (present_status != MH_OK || input_status != MH_OK) {
        return present_status != MH_OK ? present_status : input_status;
    }
    if (state.overlay.window_attached.load(std::memory_order_acquire) ||
        !overlay::overlay_window_callbacks_idle(state.overlay) ||
        !platform::windows::present_callbacks_idle(state.present) ||
        !overlay::input_capture_callbacks_idle(state.overlay.input_capture)) {
        return static_cast<int>(ERROR_BUSY);
    }
    if (!platform::windows::detach_present_callbacks(state.present) ||
        !overlay::detach_input_capture(state.overlay.input_capture)) {
        return static_cast<int>(ERROR_BUSY);
    }

    // The render callback already destroyed ImGui/COM on its owning context.
    // Disabled trampolines remain mapped with the pinned runtime owner.
    return MH_OK;
}

} // namespace s8::runtime
