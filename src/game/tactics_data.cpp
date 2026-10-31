#include "tactics_data.h"

#include "../platform/windows/process_memory.h"

#include <array>
#include <span>

namespace s8::game {

std::uintptr_t resolve_tactics_data(std::uintptr_t module_base) {
    if (!module_base) {
        return 0;
    }
    auto address = module_base + 0x02ed7a10;
    constexpr std::array<std::uintptr_t, 5> OFFSETS{0, 0x110, 0x120, 0x40, 0x168};
    for (const auto offset : OFFSETS) {
        std::uintptr_t next = 0;
        if (!platform::windows::read_memory(address + offset, std::as_writable_bytes(std::span(&next, 1))) || !next) {
            return 0;
        }
        address = next;
    }
    return address;
}

} // namespace s8::game
