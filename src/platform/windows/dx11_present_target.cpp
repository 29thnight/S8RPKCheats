#include "dx11_present_target.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <winrt/base.h>

#pragma comment(lib, "d3d11.lib")

namespace s8::platform::windows {

std::expected<void*, int> find_dx11_present_target() {
    const auto window = CreateWindowExW(0, L"STATIC", L"", WS_OVERLAPPEDWINDOW,
                                         0, 0, 100, 100, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!window)
        return std::unexpected(static_cast<int>(HRESULT_FROM_WIN32(GetLastError())));
    struct Window {
        HWND handle;
        ~Window() { DestroyWindow(handle); }
    } owned_window{window};

    DXGI_SWAP_CHAIN_DESC description{};
    description.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    description.SampleDesc.Count = 1;
    description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    description.BufferCount = 1;
    description.OutputWindow = window;
    description.Windowed = TRUE;
    description.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    constexpr D3D_FEATURE_LEVEL LEVELS[] = {D3D_FEATURE_LEVEL_11_0};
    D3D_FEATURE_LEVEL actual_level{};
    winrt::com_ptr<IDXGISwapChain> swap_chain;
    winrt::com_ptr<ID3D11Device> device;
    winrt::com_ptr<ID3D11DeviceContext> context;
    const auto status = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, LEVELS, 1, D3D11_SDK_VERSION,
        &description, swap_chain.put(), device.put(), &actual_level, context.put());
    if (FAILED(status))
        return std::unexpected(static_cast<int>(status));
    const auto table = *reinterpret_cast<void***>(swap_chain.get());
    return table[8];
}

} // namespace s8::platform::windows
