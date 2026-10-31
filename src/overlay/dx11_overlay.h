#pragma once

#include "input_events.h"

#include <d3d11.h>
#include <dxgi.h>
#include <winrt/base.h>
#include <array>

struct ImGuiContext;

namespace s8::overlay {

using DrawUi = void (*)(void* context);

struct Dx11Overlay {
    Dx11Overlay() = default;
    Dx11Overlay(const Dx11Overlay&) = delete;
    Dx11Overlay& operator=(const Dx11Overlay&) = delete;
    winrt::com_ptr<IDXGISwapChain> swap_chain;
    winrt::com_ptr<ID3D11Device> device;
    winrt::com_ptr<ID3D11DeviceContext> context;
    winrt::com_ptr<ID3D11RenderTargetView> render_target;
    std::array<winrt::com_ptr<ID3D11ShaderResourceView>, 11> range_textures;
    bool range_textures_loaded = false;
    ImGuiContext* imgui = nullptr;
    bool win32_ready = false;
    bool dx11_ready = false;
    InputEvents input;
    void* window = nullptr;
};

bool initialize_dx11_overlay(Dx11Overlay& overlay, IDXGISwapChain* swap_chain);
bool render_dx11_overlay(Dx11Overlay& overlay, IDXGISwapChain* swap_chain, DrawUi draw, void* context);
void release_dx11_render_target(Dx11Overlay& overlay);
void shutdown_dx11_overlay(Dx11Overlay& overlay);

} // namespace s8::overlay
