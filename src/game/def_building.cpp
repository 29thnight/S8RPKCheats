#include "def_building.h"

#include "../platform/windows/process_memory.h"

#include <array>
#include <span>

namespace s8::game {

bool set_def_building(std::uintptr_t module_base, bool enabled, const domain::DefBuildingSettings& settings) {
    if (!module_base) {
        return false;
    }
    auto root = module_base + 0x034c8630;
    constexpr std::array<std::uintptr_t, 5> CHAIN{0, 0xbb8, 0, 0x170, 0x10};
    for (const auto offset : CHAIN) {
        std::uintptr_t next = 0;
        if (!platform::windows::read_memory(root + offset, std::as_writable_bytes(std::span(&next, 1))) || !next) {
            return false;
        }
        root = next;
    }
    constexpr std::array<std::uintptr_t, 19> OFFSETS{
        0x20ace, 0x20ad2, 0x20ad4, 0x20adc, 0x20aee, 0x20af2, 0x20af4, 0x20afc,
        0x20b2e, 0x20b32, 0x20b34, 0x20b3c, 0x20b4e, 0x20b52, 0x20b54, 0x20b5c,
        0x20b6e, 0x20b72, 0x20b7c};
    constexpr std::array<int, 19> DEFAULTS{
        3000, 3, 8, 5, 2000, 3, 8, 3, 700, 3, 6, 3, 1000, 4, 9, 4, 700, 5, 3};
    for (std::size_t i = 0; i < OFFSETS.size(); ++i) {
        const auto value = static_cast<std::uint16_t>(enabled ? settings.values[i] : DEFAULTS[i]);
        if (!platform::windows::write_data_memory(root + OFFSETS[i], std::as_bytes(std::span(&value, 1)))) {
            return false;
        }
    }
    return true;
}

} // namespace s8::game
