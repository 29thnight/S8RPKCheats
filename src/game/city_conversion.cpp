#include "city_conversion.h"
#include "../platform/windows/process_memory.h"

#include <array>
#include <limits>
#include <span>

namespace s8::game {
namespace {

constexpr std::array<std::uintptr_t, 7> BIG_CITY{
    0xa90, 0x24d0, 0x2cb0, 0x31f0, 0x4450, 0x56b0, 0x70f0};
constexpr std::array<std::uintptr_t, 16> GRAZING{
    0x10, 0x550, 0x1510, 0x3490, 0x3730, 0x3c70, 0x41b0, 0x4ed0,
    0x5410, 0x5bf0, 0x63d0, 0x6910, 0x7390, 0x7b70, 0x7e10, 0x80b0};
constexpr std::array<std::uintptr_t, 12> FARMING{
    0x7f0, 0xd30, 0xfd0, 0x1270, 0x1a50, 0x1cf0, 0x2a10, 0x39d0, 0x4c30, 0x5e90, 0x6130, 0x6670};
constexpr std::array<std::uintptr_t, 15> COMMERCE{
    0xa90, 0x24d0, 0x2cb0, 0x31f0, 0x4450, 0x56b0, 0x70f0,
    0x1f90, 0x2230, 0x2770, 0x5170, 0x6bb0, 0x6e50, 0x7630, 0x8350};

std::span<const std::uintptr_t> city_offsets(CityConversionKind kind) {
    switch (kind) {
    case CityConversionKind::BigCity: return BIG_CITY;
    case CityConversionKind::Grazing: return GRAZING;
    case CityConversionKind::Farming: return FARMING;
    case CityConversionKind::Commerce: return COMMERCE;
    }
    return {};
}

std::uintptr_t resolve_city_table(std::uintptr_t module_base) {
    if (!module_base || module_base > std::numeric_limits<std::uintptr_t>::max() - 0x034c8630) {
        return 0;
    }
    constexpr std::array<std::uintptr_t, 5> OFFSETS{0, 8, 0x10, 0, 0};
    std::uintptr_t address = module_base + 0x034c8630;

    for (const std::uintptr_t offset : OFFSETS) {
        std::uintptr_t next = 0;

        if (!platform::windows::read_memory(address, std::as_writable_bytes(std::span(&next, 1))) || !next ||
            next > std::numeric_limits<std::uintptr_t>::max() - offset) {
            return 0;
        }
        address = next + offset;
    }
    return platform::windows::is_committed_memory(address, 0x80000) ? address : 0;
}

bool write_limits(std::uintptr_t city, const std::array<std::uint16_t, 4>& values, std::size_t count) {
    if (!platform::windows::is_committed_memory(city, 0xde)) {
        return false;
    }
    for (std::size_t i = 0; i < count; ++i) {
        if (!platform::windows::write_data_memory(city + 0xd0 + i * 4, std::as_bytes(std::span(&values[i], 1)))) {
            return false;
        }
    }
    return true;
}

bool apply_city_phase(CityConversionKind kind, std::uintptr_t table, CityConversionPhase phase, bool enabled) {
    for (const std::uintptr_t offset : city_offsets(kind)) {
        const std::uintptr_t type_address = table + offset;
        std::uint8_t type = 0;
        bool write_type = true;

        if (enabled && phase == CityConversionPhase::First) {
            if (kind == CityConversionKind::Commerce) {
                if (!platform::windows::read_memory(type_address, std::as_writable_bytes(std::span(&type, 1)))) {
                    return false;
                }
                write_type = type != 128;
                type = 168;
            } else {
                type = kind == CityConversionKind::BigCity ? 168 : 128;
            }
        } else if (enabled) {
            if (kind == CityConversionKind::Commerce) {
                if (!platform::windows::read_memory(type_address, std::as_writable_bytes(std::span(&type, 1)))) {
                    return false;
                }
                write_type = type == 168 || type == 88;
                type = 88;
            } else {
                type = kind == CityConversionKind::BigCity ? 128 :
                       kind == CityConversionKind::Grazing ? 208 : 48;
            }
        } else {
            write_type = kind != CityConversionKind::Commerce;
            type = 88;
        }
        if (write_type && !platform::windows::write_data_memory(type_address, std::as_bytes(std::span(&type, 1)))) {
            return false;
        }
        if (enabled && phase == CityConversionPhase::First) {
            continue;
        }

        const std::uintptr_t city = type_address - 0x10;
        bool written = false;

        switch (kind) {
        case CityConversionKind::BigCity:
            written = write_limits(city, enabled ? std::array<std::uint16_t, 4>{9000, 12000, 9000, 4000}
                                       : std::array<std::uint16_t, 4>{4500, 6000, 4500, 2000}, 4);
            break;
        case CityConversionKind::Grazing:
            written = write_limits(city, enabled ? std::array<std::uint16_t, 4>{1500, 3000, 4500, 1500}
                                       : std::array<std::uint16_t, 4>{2000, 3500, 5000, 2000}, 4);
            break;
        case CityConversionKind::Farming:
            written = write_limits(city, enabled ? std::array<std::uint16_t, 4>{6500, 5000, 0, 0}
                                       : std::array<std::uint16_t, 4>{6000, 4500, 0, 0}, 2);
            break;
        case CityConversionKind::Commerce:
            written = write_limits(city, enabled ? std::array<std::uint16_t, 4>{5000, 6500, 0, 0}
                                       : std::array<std::uint16_t, 4>{4500, 6000, 0, 0}, 2);
            break;
        }
        if (!written) {
            return false;
        }
    }
    return true;
}

void forget_city_conversion(CityConversion& state) {
    state.phase = CityConversionPhase::Idle;
    state.epoch = {};
    state.module_base = 0;
    state.city_table = 0;
    state.deadline_ms = 0;
    state.applied = false;
}

bool finish_commerce_staging(std::uintptr_t table) {
    constexpr std::uint8_t COMMERCE_TYPE = 88;

    for (const std::uintptr_t offset : COMMERCE) {
        std::uint8_t type = 0;

        if (!platform::windows::read_memory(table + offset, std::as_writable_bytes(std::span(&type, 1)))) {
            return false;
        }
        if (type == 168 && !platform::windows::write_data_memory(table + offset,
                std::as_bytes(std::span(&COMMERCE_TYPE, 1)))) {
            return false;
        }
    }
    return true;
}

} // namespace

bool request_city_conversion(CityConversion& state, CityConversionKind kind, bool enabled,
                             domain::GameEpoch epoch, std::uintptr_t module_base, std::uint64_t now_ms) {
    if (state.phase != CityConversionPhase::Idle) {
        return false;
    }
    const std::uintptr_t table = resolve_city_table(module_base);

    if (!table) {
        return false;
    }
    if (state.epoch != epoch || state.city_table != table || state.module_base != module_base) {
        forget_city_conversion(state);
    }
    state.kind = kind;
    state.epoch = epoch;
    state.module_base = module_base;
    state.city_table = table;
    if (!enabled) {
        // A failed write can leave a prefix changed. Keep restoration eligible.
        state.applied = true;
        if (!apply_city_phase(kind, table, CityConversionPhase::Second, false)) {
            return false;
        }
        state.applied = false;
        return true;
    }
    state.phase = CityConversionPhase::First;
    state.deadline_ms = now_ms + 200;
    return true;
}

void tick_city_conversion(CityConversion& state, domain::GameEpoch epoch, std::uint64_t now_ms) {
    if (state.phase == CityConversionPhase::Idle) {
        return;
    }
    if (state.epoch != epoch) {
        forget_city_conversion(state);
        return;
    }
    if (now_ms < state.deadline_ms) {
        return;
    }
    const std::uintptr_t current_table = resolve_city_table(state.module_base);

    if (!current_table) {
        return;
    }
    if (current_table != state.city_table) {
        forget_city_conversion(state);
        return;
    }
    const CityConversionPhase phase = state.phase;

    state.applied = true;
    state.deadline_ms = now_ms + 200;
    if (!apply_city_phase(state.kind, state.city_table, phase, true)) {
        return;
    }
    if (phase == CityConversionPhase::First) {
        state.phase = CityConversionPhase::Second;
    } else {
        state.phase = CityConversionPhase::Idle;
        state.applied = true;
    }
}

bool stop_city_conversion(CityConversion& state, std::uintptr_t module_base, domain::GameEpoch epoch) {
    if (state.epoch != epoch || (state.module_base && state.module_base != module_base)) {
        // The old session's city object must never be written through a new binding.
        forget_city_conversion(state);
        return true;
    }
    if (!state.applied && state.phase != CityConversionPhase::Second) {
        // First only schedules work; no write has started yet.
        forget_city_conversion(state);
        return true;
    }
    const std::uintptr_t current_table = resolve_city_table(module_base);

    if (!current_table) {
        return false;
    }
    if (current_table != state.city_table) {
        forget_city_conversion(state);
        return true;
    }
    if (state.kind == CityConversionKind::Commerce && state.phase != CityConversionPhase::Idle &&
        !finish_commerce_staging(state.city_table)) {
        return false;
    }
    if (!apply_city_phase(state.kind, state.city_table, CityConversionPhase::Second, false)) {
        return false;
    }
    forget_city_conversion(state);
    return true;
}

} // namespace s8::game
