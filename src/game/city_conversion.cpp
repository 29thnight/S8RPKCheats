#include "city_conversion.h"
#include "../platform/windows/process_memory.h"

#include <array>
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
    if (!module_base)
        return 0;
    constexpr std::array<std::uintptr_t, 5> OFFSETS{0, 8, 0x10, 0, 0};
    auto address = module_base + 0x034c8630;
    for (const auto offset : OFFSETS) {
        std::uintptr_t next = 0;
        if (!platform::windows::read_memory(address, std::as_writable_bytes(std::span(&next, 1))) || !next)
            return 0;
        address = next + offset;
    }
    return platform::windows::is_committed_memory(address, 0x80000) ? address : 0;
}

void write_limits(std::uintptr_t city, const std::array<std::uint16_t, 4>& values, std::size_t count) {
    if (!platform::windows::is_committed_memory(city, 0xdd))
        return;
    for (std::size_t i = 0; i < count; ++i)
        platform::windows::write_data_memory(city + 0xd0 + i * 4, std::as_bytes(std::span(&values[i], 1)));
}

void apply_city_phase(CityConversionKind kind, std::uintptr_t table, CityConversionPhase phase, bool enabled) {
    for (const auto offset : city_offsets(kind)) {
        const auto type_address = table + offset;
        std::uint8_t type = 0;
        bool write_type = true;
        if (enabled && phase == CityConversionPhase::First) {
            if (kind == CityConversionKind::Commerce) {
                if (!platform::windows::read_memory(type_address, std::as_writable_bytes(std::span(&type, 1))))
                    continue;
                write_type = type != 128;
                type = 168;
            } else {
                type = kind == CityConversionKind::BigCity ? 168 : 128;
            }
        } else if (enabled) {
            if (kind == CityConversionKind::Commerce) {
                if (!platform::windows::read_memory(type_address, std::as_writable_bytes(std::span(&type, 1))))
                    continue;
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
        if (write_type)
            platform::windows::write_data_memory(type_address, std::as_bytes(std::span(&type, 1)));
        if (enabled && phase == CityConversionPhase::First)
            continue;

        const auto city = type_address - 0x10;
        switch (kind) {
        case CityConversionKind::BigCity:
            write_limits(city, enabled ? std::array<std::uint16_t, 4>{9000, 12000, 9000, 4000}
                                       : std::array<std::uint16_t, 4>{4500, 6000, 4500, 2000}, 4);
            break;
        case CityConversionKind::Grazing:
            write_limits(city, enabled ? std::array<std::uint16_t, 4>{1500, 3000, 4500, 1500}
                                       : std::array<std::uint16_t, 4>{2000, 3500, 5000, 2000}, 4);
            break;
        case CityConversionKind::Farming:
            write_limits(city, enabled ? std::array<std::uint16_t, 4>{6500, 5000, 0, 0}
                                       : std::array<std::uint16_t, 4>{6000, 4500, 0, 0}, 2);
            break;
        case CityConversionKind::Commerce:
            write_limits(city, enabled ? std::array<std::uint16_t, 4>{5000, 6500, 0, 0}
                                       : std::array<std::uint16_t, 4>{4500, 6000, 0, 0}, 2);
            break;
        }
    }
}

} // namespace

bool request_city_conversion(CityConversion& state, CityConversionKind kind, bool enabled,
                             domain::GameEpoch epoch, std::uintptr_t module_base, std::uint64_t now_ms) {
    if (state.phase != CityConversionPhase::Idle)
        return false;
    const auto table = resolve_city_table(module_base);
    if (!table)
        return false;
    state.kind = kind;
    state.epoch = epoch;
    state.city_table = table;
    if (!enabled) {
        apply_city_phase(kind, table, CityConversionPhase::Second, false);
        state.applied = false;
        return true;
    }
    state.phase = CityConversionPhase::First;
    state.deadline_ms = now_ms + 200;
    return true;
}

void tick_city_conversion(CityConversion& state, domain::GameEpoch epoch, std::uint64_t now_ms) {
    if (state.phase == CityConversionPhase::Idle)
        return;
    if (state.epoch != epoch) {
        state.phase = CityConversionPhase::Idle;
        state.city_table = 0;
        return;
    }
    if (now_ms < state.deadline_ms)
        return;
    const auto phase = state.phase;
    apply_city_phase(state.kind, state.city_table, phase, true);
    if (phase == CityConversionPhase::First) {
        state.phase = CityConversionPhase::Second;
        state.deadline_ms = now_ms + 200;
    } else {
        state.phase = CityConversionPhase::Idle;
        state.applied = true;
    }
}

} // namespace s8::game
