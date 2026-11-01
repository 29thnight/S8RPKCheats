#include "dx11_overlay.h"

#include "../../third_party/imgui_legacy/imgui.h"
#include "../../third_party/imgui_legacy/imgui_impl_dx11.h"
#include "../../third_party/imgui_legacy/imgui_impl_win32.h"

#include <array>
#include <filesystem>
#include <fstream>
#include <limits>

namespace s8::overlay {
namespace {

bool load_font(const std::filesystem::path& path, float size_pixels) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        return false;
    }
    const std::ifstream::pos_type size = file.tellg();

    if (size <= 0 || size > (std::numeric_limits<int>::max)()) {
        return false;
    }
    const int length = static_cast<int>(size);
    void* data = IM_ALLOC(length);

    if (!data) {
        return false;
    }
    file.seekg(0);
    if (!file.read(static_cast<char*>(data), length)) {
        IM_FREE(data);
        return false;
    }
    static constexpr ImWchar RANGES[]{
        0x0020, 0x00ff, 0x2000, 0x206f, 0x2190, 0x21ff,
        0x2500, 0x257f, 0x25a0, 0x25ff, 0x2600, 0x26ff,
        0x3000, 0x303f, 0xac00, 0xd7a3, 0,
    };
    ImFontConfig config;

    config.PixelSnapH = true;
    config.OversampleH = 2;
    config.OversampleV = 1;
    config.GlyphExtraSpacing.x = -1.0f;
    ImGui::GetIO().Fonts->AddFontFromMemoryTTF(data, length, size_pixels, &config, RANGES);
    return true;
}

void load_korean_font(float size_pixels) {
    HMODULE module = nullptr;
    std::array<wchar_t, 32768> path{};
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(&load_korean_font), &module)) {
        const DWORD length = GetModuleFileNameW(module, path.data(), static_cast<DWORD>(path.size()));

        if (length && length < path.size()) {
            const std::filesystem::path directory = std::filesystem::path(path.data()).parent_path();

            if (load_font(directory / L"malgunbd.ttf", size_pixels) || load_font(directory / L"malgun.ttf", size_pixels)) {
                return;
            }
        }
    }
    const UINT length = GetWindowsDirectoryW(path.data(), static_cast<UINT>(path.size()));

    if (length && length < path.size()) {
        load_font(std::filesystem::path(path.data()) / L"Fonts" / L"malgun.ttf", size_pixels);
    }
}

bool create_render_target(Dx11Overlay& overlay, IDXGISwapChain* swap_chain) {
    winrt::com_ptr<ID3D11Texture2D> buffer;

    if (FAILED(swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), buffer.put_void()))) {
        return false;
    }
    return SUCCEEDED(overlay.device->CreateRenderTargetView(buffer.get(), nullptr, overlay.render_target.put()));
}

} // namespace

bool initialize_dx11_overlay(Dx11Overlay& overlay, IDXGISwapChain* swap_chain) {
    if (!swap_chain) {
        return false;
    }
    DXGI_SWAP_CHAIN_DESC description{};

    if (FAILED(swap_chain->GetDesc(&description)) ||
        FAILED(swap_chain->GetDevice(__uuidof(ID3D11Device), overlay.device.put_void()))) {
        return false;
    }
    overlay.device->GetImmediateContext(overlay.context.put());
    if (!overlay.context || !create_render_target(overlay, swap_chain)) {
        overlay.device = nullptr;
        overlay.context = nullptr;
        return false;
    }
    IMGUI_CHECKVERSION();
    overlay.imgui = ImGui::CreateContext();
    ImGui::SetCurrentContext(overlay.imgui);
    ImGuiIO& io = ImGui::GetIO();

    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();

    style.WindowPadding = ImVec2(10, 10);
    style.WindowRounding = 8.0f;
    style.FramePadding = ImVec2(5, 2);
    style.FrameRounding = 4.0f;
    style.ItemSpacing = ImVec2(8, 4);
    style.ItemInnerSpacing = ImVec2(6, 4);
    style.IndentSpacing = 20.0f;
    style.ScrollbarSize = 15.0f;
    style.ScrollbarRounding = 9.0f;
    style.GrabMinSize = 8.0f;
    style.GrabRounding = 4.0f;
    style.PopupRounding = 8.0f;
    style.ChildRounding = 8.0f;
    style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
    ImVec4* const colors = style.Colors;

    colors[ImGuiCol_Text] = ImVec4(0.95f, 0.96f, 0.98f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.08f, 0.08f, 0.09f, 0.96f);
    colors[ImGuiCol_Border] = ImVec4(1.00f, 0.75f, 0.00f, 0.50f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.18f, 0.18f, 0.19f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.28f, 0.28f, 0.29f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.38f, 0.38f, 0.39f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.10f, 0.10f, 0.11f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.15f, 0.15f, 0.16f, 1.00f);
    colors[ImGuiCol_CheckMark] = ImVec4(1.00f, 0.80f, 0.00f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(1.00f, 0.75f, 0.00f, 0.70f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(1.00f, 0.75f, 0.00f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(1.00f, 0.75f, 0.00f, 0.18f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(1.00f, 0.75f, 0.00f, 0.38f);
    colors[ImGuiCol_ButtonActive] = ImVec4(1.00f, 0.75f, 0.00f, 0.58f);
    colors[ImGuiCol_Header] = ImVec4(1.00f, 0.75f, 0.00f, 0.22f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(1.00f, 0.75f, 0.00f, 0.42f);
    colors[ImGuiCol_HeaderActive] = ImVec4(1.00f, 0.75f, 0.00f, 0.62f);
    colors[ImGuiCol_Separator] = ImVec4(1.00f, 0.75f, 0.00f, 0.32f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(1.00f, 0.75f, 0.00f, 0.52f);
    colors[ImGuiCol_SeparatorActive] = ImVec4(1.00f, 0.75f, 0.00f, 0.72f);
    colors[ImGuiCol_Tab] = ImVec4(1.00f, 0.75f, 0.00f, 0.18f);
    colors[ImGuiCol_TabHovered] = ImVec4(1.00f, 0.75f, 0.00f, 0.42f);
    colors[ImGuiCol_TabSelected] = ImVec4(1.00f, 0.75f, 0.00f, 0.58f);
    const UINT dpi = GetDpiForWindow(description.OutputWindow);

    load_korean_font(15.0f * (dpi ? static_cast<float>(dpi) / 96.0f : 1.0f));
    overlay.window = description.OutputWindow;
    if (!ImGui_ImplWin32_Init(overlay.window)) {
        ImGui::DestroyContext(overlay.imgui);
        overlay.imgui = nullptr;
        overlay.render_target = nullptr;
        overlay.context = nullptr;
        overlay.device = nullptr;
        return false;
    }
    overlay.win32_ready = true;
    if (!ImGui_ImplDX11_Init(overlay.device.get(), overlay.context.get())) {
        ImGui_ImplWin32_Shutdown();
        overlay.win32_ready = false;
        ImGui::DestroyContext(overlay.imgui);
        overlay.imgui = nullptr;
        overlay.render_target = nullptr;
        overlay.context = nullptr;
        overlay.device = nullptr;
        return false;
    }
    overlay.dx11_ready = true;
    overlay.swap_chain.copy_from(swap_chain);
    return true;
}

bool render_dx11_overlay(Dx11Overlay& overlay, IDXGISwapChain* swap_chain, DrawUi draw, void* context) {
    if (!swap_chain || !draw) {
        return false;
    }
    if (!overlay.imgui && !initialize_dx11_overlay(overlay, swap_chain)) {
        return false;
    }
    ImGui::SetCurrentContext(overlay.imgui);
    if (!overlay.render_target && !create_render_target(overlay, swap_chain)) {
        return false;
    }

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    overlay.input.deliver();
    ImGui::NewFrame();
    draw(context);
    ImGui::Render();

    std::array<ID3D11RenderTargetView*, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT> previous_targets{};
    std::array<winrt::com_ptr<ID3D11RenderTargetView>, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT> retained_targets;
    winrt::com_ptr<ID3D11DepthStencilView> previous_depth;

    overlay.context->OMGetRenderTargets(static_cast<UINT>(previous_targets.size()), previous_targets.data(), previous_depth.put());
    for (std::size_t i = 0; i < previous_targets.size(); ++i) {
        retained_targets[i].attach(previous_targets[i]);
    }

    ID3D11RenderTargetView* const target = overlay.render_target.get();

    overlay.context->OMSetRenderTargets(1, &target, nullptr);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    overlay.context->OMSetRenderTargets(static_cast<UINT>(previous_targets.size()), previous_targets.data(), previous_depth.get());
    return true;
}

void release_dx11_render_target(Dx11Overlay& overlay) {
    overlay.render_target = nullptr;
}

void release_dx11_ui_atlases(Dx11Overlay& overlay) {
    for (winrt::com_ptr<ID3D11ShaderResourceView>& texture : overlay.ui_atlas_textures) {
        texture = nullptr;
    }
    overlay.ui_atlases_loaded = false;
    overlay.ui_atlas_retry_ms = 0;
}

void shutdown_dx11_overlay(Dx11Overlay& overlay) {
    if (overlay.imgui) {
        ImGui::SetCurrentContext(overlay.imgui);
        if (overlay.dx11_ready) {
            ImGui_ImplDX11_Shutdown();
            overlay.dx11_ready = false;
        }
        if (overlay.win32_ready) {
            ImGui_ImplWin32_Shutdown();
            overlay.win32_ready = false;
        }
        ImGui::DestroyContext(overlay.imgui);
        overlay.imgui = nullptr;
    }
    overlay.render_target = nullptr;
    for (winrt::com_ptr<ID3D11ShaderResourceView>& texture : overlay.range_textures) {
        texture = nullptr;
    }
    overlay.range_textures_loaded = false;
    release_dx11_ui_atlases(overlay);
    overlay.context = nullptr;
    overlay.device = nullptr;
    overlay.window = nullptr;
    overlay.swap_chain = nullptr;
    overlay.input.request_reset();
}

} // namespace s8::overlay
