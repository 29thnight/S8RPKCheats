#include "overlay_lifecycle.h"

#include "../platform/windows/dx11_present_target.h"
#include "../platform/windows/hook.h"
#include "../../third_party/minhook/include/MinHook.h"

namespace s8::runtime {

std::expected<void, int> start_overlay(OverlayLifecycle& state, HWND game_window) {
    DWORD process = 0;
    if (!GetWindowThreadProcessId(game_window, &process) || process != GetCurrentProcessId()) {
        return std::unexpected(static_cast<int>(ERROR_INVALID_WINDOW_HANDLE));
    }
    state.overlay.target_window = game_window;
    const auto target = platform::windows::find_dx11_present_target();
    if (!target) {
        return std::unexpected(target.error());
    }
    const auto initialized = platform::windows::initialize_hooks();
    if (initialized != MH_OK && initialized != MH_ERROR_ALREADY_INITIALIZED) {
        return std::unexpected(initialized);
    }
    const auto installed = platform::windows::install_present_hook(
        state.present, *target, overlay::on_overlay_present, &state.overlay);
    if (!installed) {
        return std::unexpected(installed.error());
    }
    return {};
}

int stop_overlay_entries(OverlayLifecycle& state) {
    state.bridge.close_officer_name_requests();
    state.bridge.commands.close();
    while (auto command = state.bridge.commands.pop()) {
        state.bridge.commands.complete(command->completion,
            std::unexpected(domain::Error{domain::ErrorCode::Cancelled}));
    }
    return platform::windows::disable_present_hook(state.present);
}

int finish_overlay_stop(OverlayLifecycle& state) {
    const auto status = platform::windows::remove_present_hook(state.present);
    if (status != MH_OK) {
        return status;
    }
    overlay::shutdown_dx11_overlay(state.overlay.renderer);
    return MH_OK;
}

} // namespace s8::runtime
