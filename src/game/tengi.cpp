#include "tengi.h"

#include "tengi_filter.h"
#include "../platform/windows/process_memory.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <array>
#include <cstddef>
#include <span>

namespace s8::game {
namespace {

constexpr std::uintptr_t MANAGER_RVA = 0x2e98bc8;
constexpr std::uintptr_t EVENT_TABLE_OFFSET = 0x58a670;
constexpr std::uintptr_t MOMENTUM_OFFSET = 0x1ebde0;
constexpr std::uintptr_t DATE_OFFSET = 0x72d0;
constexpr std::uint64_t CYCLE_INTERVAL_MS = 200;
constexpr std::uint64_t DEFINITION_INTERVAL_MS = 2000;
constexpr std::array<std::byte, 24> CODEVIEW_ID{
    std::byte{0x52}, std::byte{0x53}, std::byte{0x44}, std::byte{0x53},
    std::byte{0x7e}, std::byte{0x02}, std::byte{0x8a}, std::byte{0xb1},
    std::byte{0xf4}, std::byte{0x19}, std::byte{0x35}, std::byte{0x4c},
    std::byte{0x87}, std::byte{0x49}, std::byte{0x5b}, std::byte{0x6f},
    std::byte{0x0f}, std::byte{0xf8}, std::byte{0x14}, std::byte{0xd8},
    std::byte{0x01}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};

template <typename T>
bool read_value(std::uintptr_t address, T& value) {

    return platform::windows::read_memory(address, std::as_writable_bytes(std::span(&value, 1)));
}

template <typename T>
bool write_value(std::uintptr_t address, const T& value) {

    if (!platform::windows::write_data_memory(address, std::as_bytes(std::span(&value, 1)))) {
        return false;
    }

    T actual{};

    return read_value(address, actual) && actual == value;
}

std::uintptr_t resolve_manager(std::uintptr_t module_base) {

    std::uintptr_t manager = 0;

    return module_base != 0 && read_value(module_base + MANAGER_RVA, manager) && manager > 0x10000
        ? manager : 0;
}

std::uintptr_t resolve_event(std::uintptr_t manager, std::uint8_t id) {

    std::uintptr_t object = 0;
    std::uint8_t actual_id = 0;

    if (!manager || !read_value(manager + EVENT_TABLE_OFFSET + std::uintptr_t{id} * 8, object) ||
        object <= 0x10000 || !read_value(object + 8, actual_id) || actual_id != id) {
        return 0;
    }

    return object;
}

void update_durations(TengiState& state, const domain::TengiSettings& settings) {

    const std::uintptr_t manager = resolve_manager(state.module_base);

    for (std::size_t index = 0; index < domain::TENGI_EVENT_COUNT; ++index) {

        TengiDurationOverride& saved = state.durations[index];
        const domain::TengiEventSettings& event = settings.events[index];
        const int months = event.allowed ? event.duration_months : 0;

        if (months == 0 && !saved.active) {
            continue;
        }

        const std::uintptr_t object = resolve_event(manager, domain::TENGI_EVENT_IDS[index]);

        if (!object || resolve_manager(state.module_base) != manager) {
            saved = {};
            continue;
        }

        const std::uintptr_t address = object + 0x12;

        if (saved.address != address) {
            saved = {};
            saved.address = address;
        }

        std::uint16_t current = 0;

        if (!read_value(address, current)) {
            continue;
        }

        if (months == 0) {

            if (!saved.active || current != saved.written || write_value(address, saved.original)) {
                saved.active = false;
            }

            continue;
        }

        // Loading a scenario can replace a definition at the same address.
        if (saved.active && current != saved.written) {
            saved.active = false;
        }

        if (!saved.active && current == 0) {
            continue; // Instant effects must remain instant.
        }

        const std::uint16_t original = saved.active ? saved.original : current;
        const std::uint16_t desired = static_cast<std::uint16_t>(months);

        if (current == desired || write_value(address, desired)) {
            saved.original = original;
            saved.written = desired;
            saved.active = true;
        }
    }
}

struct CycleSnapshot {
    std::uintptr_t manager = 0;
    std::uintptr_t data = 0;
    int month = -1;
    int start = -1;
    std::uint16_t duration = 0;
    std::uint8_t id = 0;
    std::uint8_t momentum = 0;
    bool constructed = false;
};

bool read_cycle_snapshot(std::uintptr_t module_base, CycleSnapshot& snapshot) {

    snapshot.manager = resolve_manager(module_base);

    if (snapshot.manager == 0) {
        return false;
    }

    std::array<std::uint8_t, 3> date{};
    std::uint8_t constructed = 0;
    const std::uintptr_t momentum = snapshot.manager + MOMENTUM_OFFSET;

    if (!platform::windows::read_memory(snapshot.manager + DATE_OFFSET,
                                        std::as_writable_bytes(std::span(date))) ||
        !read_value(momentum, snapshot.momentum) || !read_value(momentum + 24, constructed) ||
        constructed > 1 || date[2] < 1 || date[2] > 12) {
        return false;
    }

    const int year = date[0] | (static_cast<int>(date[1]) << 8);

    if (year == 0) {
        return false;
    }

    snapshot.month = year * 12 + date[2] - 1;
    snapshot.constructed = constructed != 0;

    if (!snapshot.constructed) {
        return resolve_manager(module_base) == snapshot.manager;
    }

    if (!read_value(momentum + 16, snapshot.data) || snapshot.data <= 0x10000 ||
        !read_value(snapshot.data + 8, snapshot.id) || snapshot.id < 1 || snapshot.id > 100 ||
        resolve_event(snapshot.manager, snapshot.id) != snapshot.data ||
        !platform::windows::read_memory(snapshot.data + 0x36, std::as_writable_bytes(std::span(date))) ||
        date[2] < 1 || date[2] > 12 || !read_value(snapshot.data + 0x12, snapshot.duration)) {
        return false;
    }

    const int start_year = date[0] | (static_cast<int>(date[1]) << 8);

    if (start_year == 0) {
        return false;
    }

    snapshot.start = start_year * 12 + date[2] - 1;
    return snapshot.start <= snapshot.month && resolve_manager(module_base) == snapshot.manager;
}

bool request_momentum(std::uintptr_t module_base, const CycleSnapshot& snapshot) {

    const std::uintptr_t address = snapshot.manager + MOMENTUM_OFFSET;
    std::uint8_t constructed = 1;
    constexpr std::uint8_t FULL_MOMENTUM = 100;

    return resolve_manager(module_base) == snapshot.manager && read_value(address + 24, constructed) &&
        constructed == 0 && write_value(address, FULL_MOMENTUM);
}

void update_cycle(TengiState& state, std::uintptr_t player, const domain::TengiSettings& settings) {

    CycleSnapshot snapshot{};

    if (!read_cycle_snapshot(state.module_base, snapshot)) {
        state.cycle = {};
        state.cycle.session = player;
        state.cycle.needs_event_observation = true;
        state.snapshot_ready = false;
        return;
    }

    TengiCycle& cycle = state.cycle;

    state.snapshot_ready = true;
    state.momentum = snapshot.momentum;

    if (cycle.session != player || cycle.manager != snapshot.manager || snapshot.month < cycle.last_month) {

        const bool uncertain = cycle.session == player && cycle.needs_event_observation;

        cycle = {};
        cycle.session = player;
        cycle.manager = snapshot.manager;
        cycle.needs_event_observation = uncertain;
        cycle.handled_council = snapshot.month; // Attaching/loading never forces in the current month.
    }

    cycle.last_month = snapshot.month;

    if (snapshot.constructed && (cycle.data != snapshot.data || cycle.start != snapshot.start)) {

        int duration = snapshot.duration;

        for (std::size_t index = 0; index < domain::TENGI_EVENT_COUNT; ++index) {

            const domain::TengiEventSettings& event = settings.events[index];

            if (domain::TENGI_EVENT_IDS[index] == snapshot.id && event.allowed && event.duration_months > 0) {
                duration = event.duration_months;
                break;
            }
        }

        cycle.data = snapshot.data;
        cycle.start = snapshot.start;
        cycle.earliest = snapshot.start + duration;
        cycle.pending = false;
        cycle.needs_event_observation = false;
    }

    if (snapshot.month % 3 != 0 || cycle.handled_council == snapshot.month) {
        return;
    }

    cycle.handled_council = snapshot.month;

    if (!settings.force_quarterly || !state.filter_ready || snapshot.constructed || cycle.pending ||
        cycle.needs_event_observation || snapshot.month < cycle.earliest ||
        domain::tengi_allowed_mask(settings) == 0) {
        return;
    }

    if (request_momentum(state.module_base, snapshot)) {
        cycle.pending = true; // Only a real constructed event confirms occurrence.
        state.momentum = 100;
    }
}

} // namespace

bool verified_tengi_layout(std::uintptr_t module_base) {

    IMAGE_DOS_HEADER dos{};

    if (!module_base || !read_value(module_base, dos) || dos.e_magic != IMAGE_DOS_SIGNATURE ||
        dos.e_lfanew <= 0 || dos.e_lfanew > 0x100000) {
        return false;
    }

    IMAGE_NT_HEADERS64 nt{};

    if (!read_value(module_base + static_cast<std::uintptr_t>(dos.e_lfanew), nt) ||
        nt.Signature != IMAGE_NT_SIGNATURE || nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
        nt.OptionalHeader.NumberOfRvaAndSizes <= IMAGE_DIRECTORY_ENTRY_DEBUG) {
        return false;
    }

    const IMAGE_DATA_DIRECTORY& directory = nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG];
    const std::uint32_t image_size = nt.OptionalHeader.SizeOfImage;

    if (!directory.VirtualAddress || directory.VirtualAddress >= image_size ||
        directory.Size > image_size - directory.VirtualAddress ||
        directory.Size / sizeof(IMAGE_DEBUG_DIRECTORY) > 1024) {
        return false;
    }

    for (std::size_t index = 0; index < directory.Size / sizeof(IMAGE_DEBUG_DIRECTORY); ++index) {

        IMAGE_DEBUG_DIRECTORY entry{};

        if (!read_value(module_base + directory.VirtualAddress + index * sizeof(entry), entry)) {
            return false;
        }

        if (entry.Type != IMAGE_DEBUG_TYPE_CODEVIEW || entry.SizeOfData < CODEVIEW_ID.size() ||
            !entry.AddressOfRawData || entry.AddressOfRawData >= image_size ||
            entry.SizeOfData > image_size - entry.AddressOfRawData) {
            continue;
        }

        std::array<std::byte, CODEVIEW_ID.size()> identity{};

        if (platform::windows::read_memory(module_base + entry.AddressOfRawData, identity) &&
            identity == CODEVIEW_ID) {
            return true;
        }
    }

    return false;
}

void update_tengi(TengiState& state, std::uintptr_t module_base, std::uintptr_t player,
                  std::uint64_t now_ms, const domain::TengiSettings& requested) {

    if (state.module_base != module_base) {
        state = {};
        state.module_base = module_base;
    }

    if (!module_base) {
        return;
    }

    if (!state.layout_checked) {
        state.layout_supported = verified_tengi_layout(module_base);
        state.layout_checked = true;
    }

    if (!state.layout_supported) {
        return;
    }

    const domain::TengiSettings settings = domain::normalize_tengi_settings(requested);
    const bool changed = !state.settings_seen || state.settings != settings;

    if (changed || (!state.filter_ready &&
                   (state.last_filter_attempt_ms == 0 || now_ms - state.last_filter_attempt_ms >= 2000))) {

        const std::expected<void, int> configured = configure_tengi_filter(module_base, settings);

        state.last_filter_attempt_ms = now_ms;
        state.filter_ready = configured.has_value();
        state.filter_error = configured ? 0 : configured.error();
        state.settings = settings;
        state.settings_seen = true;
    }

    if (changed || state.last_duration_ms == 0 || now_ms - state.last_duration_ms >= DEFINITION_INTERVAL_MS) {
        update_durations(state, settings);
        state.last_duration_ms = now_ms;
    }

    if (!player || state.cycle.session != player) {
        state.cycle = {};
        state.last_tick_ms = 0;
        state.snapshot_ready = false;
    }

    if (!player || (state.last_tick_ms != 0 && now_ms - state.last_tick_ms < CYCLE_INTERVAL_MS)) {
        return;
    }

    state.last_tick_ms = now_ms;
    update_cycle(state, player, settings);
}

int stop_tengi(TengiState& state) {

    const int status = stop_tengi_filter(state.module_base);

    if (state.layout_supported) {
        update_durations(state, domain::TengiSettings{});
    }

    if (status != 0) {
        return status;
    }

    for (const TengiDurationOverride& duration : state.durations) {

        if (duration.active) {
            return ERROR_WRITE_FAULT;
        }
    }

    state = {};
    return status;
}

} // namespace s8::game
