#include "module_image.h"
#include "process_memory.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <Psapi.h>

#pragma comment(lib, "Psapi.lib")

namespace s8::platform::windows {

std::optional<std::vector<std::byte>> read_module_image(std::uintptr_t module) {
    MODULEINFO info{};
    if (!module || !GetModuleInformation(GetCurrentProcess(), reinterpret_cast<HMODULE>(module), &info, sizeof(info)))
        return std::nullopt;
    std::vector<std::byte> image(info.SizeOfImage);
    if (!read_memory(module, image))
        return std::nullopt;
    return image;
}

} // namespace s8::platform::windows
