#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <dxgi.h>

namespace {

using CreateFactory = HRESULT (WINAPI*)(REFIID, void**);
using CreateFactory2 = HRESULT (WINAPI*)(UINT, REFIID, void**);

INIT_ONCE original_once = INIT_ONCE_STATIC_INIT;
HMODULE original_module = nullptr;
CreateFactory create_factory = nullptr;
CreateFactory create_factory1 = nullptr;
CreateFactory2 create_factory2 = nullptr;

BOOL CALLBACK load_original(PINIT_ONCE, PVOID, PVOID*) {
    original_module = LoadLibraryExW(L"dxgi.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!original_module)
        return FALSE;
    create_factory = reinterpret_cast<CreateFactory>(GetProcAddress(original_module, "CreateDXGIFactory"));
    create_factory1 = reinterpret_cast<CreateFactory>(GetProcAddress(original_module, "CreateDXGIFactory1"));
    create_factory2 = reinterpret_cast<CreateFactory2>(GetProcAddress(original_module, "CreateDXGIFactory2"));
    return TRUE;
}

} // namespace

extern "C" HRESULT WINAPI proxy_create_dxgi_factory(REFIID riid, void** factory) {
    InitOnceExecuteOnce(&original_once, load_original, nullptr, nullptr);
    return create_factory ? create_factory(riid, factory) : E_FAIL;
}

extern "C" HRESULT WINAPI proxy_create_dxgi_factory1(REFIID riid, void** factory) {
    InitOnceExecuteOnce(&original_once, load_original, nullptr, nullptr);
    return create_factory1 ? create_factory1(riid, factory) : E_FAIL;
}

extern "C" HRESULT WINAPI proxy_create_dxgi_factory2(UINT flags, REFIID riid, void** factory) {
    InitOnceExecuteOnce(&original_once, load_original, nullptr, nullptr);
    return create_factory2 ? create_factory2(flags, riid, factory) : E_FAIL;
}
