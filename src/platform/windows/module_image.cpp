#include "module_image.h"
#include "process_memory.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <Psapi.h>

#include <algorithm>
#include <cstring>
#include <limits>
#include <span>

#pragma comment(lib, "Psapi.lib")

namespace s8::platform::windows {
namespace {

bool readable_image_region(const MEMORY_BASIC_INFORMATION& region, std::uintptr_t module) {
    if (region.State != MEM_COMMIT || region.Type != MEM_IMAGE ||
        reinterpret_cast<std::uintptr_t>(region.AllocationBase) != module ||
        (region.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0) {
        return false;
    }

    constexpr DWORD READABLE = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY |
        PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;

    return (region.Protect & READABLE) != 0;
}

} // namespace

std::optional<std::vector<std::byte>> read_module_image(std::uintptr_t module) {
    MODULEINFO info{};

    if (!module || !GetModuleInformation(GetCurrentProcess(), reinterpret_cast<HMODULE>(module), &info, sizeof(info)) ||
        info.SizeOfImage < sizeof(IMAGE_DOS_HEADER) ||
        info.SizeOfImage > std::numeric_limits<std::uintptr_t>::max() - module) {
        return std::nullopt;
    }

    // Preserve RVA offsets while leaving discarded sections and image padding
    // zero-filled. One inaccessible page must not invalidate readable code.
    std::vector<std::byte> image(info.SizeOfImage);
    const std::uintptr_t end = module + info.SizeOfImage;
    std::uintptr_t cursor = module;

    while (cursor < end) {
        MEMORY_BASIC_INFORMATION region{};

        if (VirtualQuery(reinterpret_cast<const void*>(cursor), &region, sizeof(region)) != sizeof(region) ||
            region.RegionSize == 0) {
            return std::nullopt;
        }

        const std::uintptr_t begin = reinterpret_cast<std::uintptr_t>(region.BaseAddress);

        if (begin > cursor || region.RegionSize > std::numeric_limits<std::uintptr_t>::max() - begin) {
            return std::nullopt;
        }

        const std::uintptr_t next = std::min(end, begin + region.RegionSize);

        if (next <= cursor) {
            return std::nullopt;
        }
        if (readable_image_region(region, module)) {
            const std::span<std::byte> bytes(image.data() + (cursor - module), next - cursor);

            if (!read_memory(cursor, bytes)) {
                return std::nullopt;
            }
        }

        cursor = next;
    }

    IMAGE_DOS_HEADER header{};

    std::memcpy(&header, image.data(), sizeof(header));
    if (header.e_magic != IMAGE_DOS_SIGNATURE) {
        return std::nullopt;
    }

    return image;
}

} // namespace s8::platform::windows
