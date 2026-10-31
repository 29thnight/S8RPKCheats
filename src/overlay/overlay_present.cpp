#include "overlay_present.h"
#include "../platform/windows/speed_clock.h"
#include "../../third_party/imgui_legacy/imgui.h"

namespace s8::overlay {

namespace {

void render_overlay(OverlayPresent& state, IDXGISwapChain* swap_chain) {
    DXGI_SWAP_CHAIN_DESC description{};

    if (FAILED(swap_chain->GetDesc(&description)) || description.OutputWindow != state.target_window) {
        return;
    }
    if (state.render_stop_requested.load(std::memory_order_acquire)) {
        state.capture_mouse.store(false, std::memory_order_release);
        state.capture_keyboard.store(false, std::memory_order_release);
        state.menu.range_textures.fill(nullptr);
        shutdown_dx11_overlay(state.renderer);
        state.target_swap_chain.store(nullptr, std::memory_order_release);
        state.render_stopped.store(true, std::memory_order_release);
        return;
    }
    if (state.renderer.swap_chain && state.renderer.swap_chain.get() != swap_chain) {
        state.menu.range_textures.fill(nullptr);
        state.capture_mouse.store(false, std::memory_order_release);
        state.capture_keyboard.store(false, std::memory_order_release);
        shutdown_dx11_overlay(state.renderer);
        state.render_retry_ms = 0;
    }
    state.target_swap_chain.store(swap_chain, std::memory_order_release);
    const ULONGLONG now_ms = GetTickCount64();

    if (state.render_failed.load(std::memory_order_acquire) && now_ms < state.render_retry_ms) {
        return;
    }
    if (state.toggle_requests.exchange(0) % 2 != 0) {
        state.menu.open = true;
        state.menu.toggle_collapse = true;
    }
    update_feature_menu(state.menu, state.bridge);
    const UINT dpi = GetDpiForWindow(state.target_window);

    state.menu.dpi_scale = dpi ? static_cast<float>(dpi) / 96.0f : 1.0f;
    const bool rendered = render_dx11_overlay(state.renderer, swap_chain, [](void* value) {
        OverlayPresent& state = *static_cast<OverlayPresent*>(value);

        if (!state.renderer.range_textures_loaded) {
            if (const own::shared_owner<const services::RangeImages> images = state.bridge.range_images_view()) {
                state.menu.range_textures.fill(nullptr);
                for (std::size_t i = 0; i < images->size(); ++i) {
                    const services::RangeImage& image = (*images)[i];

                    if (image.pixels.empty()) {
                        continue;
                    }
                    D3D11_TEXTURE2D_DESC description{};

                    description.Width = image.width;
                    description.Height = image.height;
                    description.MipLevels = 1;
                    description.ArraySize = 1;
                    description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                    description.SampleDesc.Count = 1;
                    description.Usage = D3D11_USAGE_DEFAULT;
                    description.BindFlags = D3D11_BIND_SHADER_RESOURCE;
                    D3D11_SUBRESOURCE_DATA data{};

                    data.pSysMem = image.pixels.data();
                    data.SysMemPitch = image.width * 4;
                    winrt::com_ptr<ID3D11Texture2D> texture;

                    if (SUCCEEDED(state.renderer.device->CreateTexture2D(&description, &data, texture.put()))) {
                        state.renderer.device->CreateShaderResourceView(texture.get(), nullptr,
                                                                        state.renderer.range_textures[i].put());
                        state.menu.range_textures[i] = state.renderer.range_textures[i].get();
                    }
                }
                state.renderer.range_textures_loaded = true;
            }
        }
        draw_feature_menu(state.menu, state.bridge.commands);
        if (state.menu.open) {
            if (state.menu.collapsed && state.menu.snapshot.features.get(domain::FeatureId::ShowWidgetNotif)) {
                const ImVec2 label_size = ImGui::CalcTextSize("알림확인");

                ImGui::SetNextWindowPos(ImVec2(state.menu.window_x - label_size.x - 23.0f,
                                              state.menu.window_y), ImGuiCond_Always);
                ImGui::SetNextWindowBgAlpha(0.6f);
                constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;
                if (ImGui::Begin("##notification_badge", nullptr, flags)) {
                    if (ImGui::Button("알림확인")) {
                        state.menu.notifications.show_history = !state.menu.notifications.show_history;
                    }
                }
                ImGui::End();
            }
            if (state.menu.collapsed && state.menu.snapshot.protagonist_id &&
                state.menu.snapshot.features.get(domain::FeatureId::ShowWidgetHero)) {
                const ImVec2 label_size = ImGui::CalcTextSize("주인공");
                const float notification_width = state.menu.snapshot.features.get(domain::FeatureId::ShowWidgetNotif)
                    ? ImGui::CalcTextSize("알림확인").x + 28.0f : 0.0f;
                ImGui::SetNextWindowPos(ImVec2(state.menu.window_x - label_size.x - 23.0f - notification_width,
                                              state.menu.window_y), ImGuiCond_Always);
                ImGui::SetNextWindowBgAlpha(0.6f);
                constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;
                if (ImGui::Begin("##protagonist_badge", nullptr, flags)) {
                    if (ImGui::Button("주인공")) {
                        state.menu.protagonist_detail = !state.menu.protagonist_detail;
                    }
                }
                ImGui::End();
            }
            if (state.menu.collapsed && state.menu.snapshot.protagonist_id &&
                state.menu.snapshot.features.get(domain::FeatureId::ShowWidgetAllOfficers)) {
                const ImVec2 label_size = ImGui::CalcTextSize("모든무장");
                const float notification_width = state.menu.snapshot.features.get(domain::FeatureId::ShowWidgetNotif)
                    ? ImGui::CalcTextSize("알림확인").x + 28.0f : 0.0f;
                const float hero_width = state.menu.snapshot.features.get(domain::FeatureId::ShowWidgetHero)
                    ? ImGui::CalcTextSize("주인공").x + 28.0f : 0.0f;
                ImGui::SetNextWindowPos(ImVec2(state.menu.window_x - label_size.x - 23.0f - notification_width - hero_width,
                                              state.menu.window_y), ImGuiCond_Always);
                ImGui::SetNextWindowBgAlpha(0.6f);
                constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;
                if (ImGui::Begin("##officer_roster_badge", nullptr, flags)) {
                    if (ImGui::Button("모든무장")) {
                        state.menu.officer_list_open = !state.menu.officer_list_open;
                    }
                }
                ImGui::End();
            }
            draw_officer_roster(state.menu, state.bridge);
            draw_protagonist_abilities(state.menu, state.bridge);
            draw_debug_tools(state.menu.debug_tools, state.bridge.debug_tools, state.menu.epoch);
            draw_notifications(state.menu.notifications, state.bridge, state.menu.snapshot.notification_speed);
        }
    }, &state);
    if (rendered) {
        if (state.menu.open) {
            state.bridge.ui_rendered.store(true);
        }
        const ImGuiIO& io = ImGui::GetIO();

        state.capture_mouse.store(io.WantCaptureMouse);
        state.capture_keyboard.store(io.WantCaptureKeyboard);
        state.render_failed.store(false, std::memory_order_release);
        state.render_retry_ms = 0;
    } else {
        state.capture_mouse.store(false, std::memory_order_release);
        state.capture_keyboard.store(false, std::memory_order_release);
        state.render_failed.store(true, std::memory_order_release);
        state.render_retry_ms = now_ms + 1000;
    }
}

} // namespace

void on_overlay_present(void* context, IDXGISwapChain* swap_chain) noexcept {
    OverlayPresent& state = *static_cast<OverlayPresent*>(context);
    const InputCaptureBypass input_bypass;
    const platform::windows::RealClockScope real_clock;

    try {
        render_overlay(state, swap_chain);
    } catch (...) {
        // Do not propagate a C++ exception through the COM Present ABI.
        state.capture_mouse.store(false, std::memory_order_release);
        state.capture_keyboard.store(false, std::memory_order_release);
        state.render_failed.store(true, std::memory_order_release);
        state.render_retry_ms = GetTickCount64() + 1000;
        state.menu.range_textures.fill(nullptr);
        shutdown_dx11_overlay(state.renderer);
        state.target_swap_chain.store(nullptr, std::memory_order_release);
    }
    if (state.frame_callback &&
        (state.target_swap_chain.load(std::memory_order_acquire) == swap_chain ||
         state.render_stopped.load(std::memory_order_acquire))) {
        state.frame_callback(state.frame_context);
    }
}

void request_overlay_stop(OverlayPresent& state) noexcept {
    state.capture_mouse.store(false, std::memory_order_release);
    state.capture_keyboard.store(false, std::memory_order_release);
    state.render_stop_requested.store(true, std::memory_order_release);
}

void on_overlay_before_resize(void* context, IDXGISwapChain* swap_chain) noexcept {
    OverlayPresent& state = *static_cast<OverlayPresent*>(context);

    if (state.renderer.swap_chain.get() == swap_chain) {
        release_dx11_render_target(state.renderer);
    }
}

void on_overlay_present_failed(void* context, IDXGISwapChain* swap_chain, std::int32_t result) noexcept {
    OverlayPresent& state = *static_cast<OverlayPresent*>(context);

    if (state.renderer.swap_chain.get() != swap_chain ||
        (result != DXGI_ERROR_DEVICE_REMOVED && result != DXGI_ERROR_DEVICE_RESET)) {
        return;
    }
    state.capture_mouse.store(false, std::memory_order_release);
    state.capture_keyboard.store(false, std::memory_order_release);
    state.menu.range_textures.fill(nullptr);
    shutdown_dx11_overlay(state.renderer);
    state.target_swap_chain.store(nullptr, std::memory_order_release);
    state.render_failed.store(true, std::memory_order_release);
    state.render_retry_ms = GetTickCount64() + 1000;
}

bool on_overlay_window_message(OverlayPresent& state, HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    const InputCaptureBypass input_bypass;
    const platform::windows::RealClockScope real_clock;

    if ((message == WM_KEYDOWN || message == WM_SYSKEYDOWN) && wparam == VK_OEM_3) {
        const ULONGLONG now = GetTickCount64();

        if (now - state.last_menu_key_ms > 500) {
            state.last_menu_key_ms = now;
            state.toggle_requests.fetch_add(1);
        }
        return false;
    }
    if (state.capture_keyboard.load() && message == WM_IME_COMPOSITION) {
        queue_ime_result(state.renderer.input, window, lparam);
        return false;
    }
    if (state.capture_keyboard.load() && (message == WM_IME_CHAR || (message == WM_CHAR && wparam >= 0x80))) {
        return true;
    }
    const InputMessageResult result = queue_win32_input(state.renderer.input, message, wparam, lparam);

    if (result == InputMessageResult::Ignored) {
        return false;
    }
    switch (message) {
    case WM_MOUSEMOVE:
    case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_LBUTTONDBLCLK:
    case WM_RBUTTONDOWN: case WM_RBUTTONUP: case WM_RBUTTONDBLCLK:
    case WM_MBUTTONDOWN: case WM_MBUTTONUP: case WM_MBUTTONDBLCLK:
    case WM_XBUTTONDOWN: case WM_XBUTTONUP: case WM_XBUTTONDBLCLK:
    case WM_MOUSEWHEEL: case WM_MOUSEHWHEEL:
        return state.capture_mouse.load();
    case WM_KEYDOWN: case WM_KEYUP: case WM_SYSKEYDOWN: case WM_SYSKEYUP: case WM_CHAR:
        return state.capture_keyboard.load();
    default:
        return false;
    }
}

} // namespace s8::overlay
