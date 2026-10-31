#include "debug_tools.h"

#include "../platform/windows/process_memory.h"
#include "../platform/windows/speed_clock.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <limits>
#include <span>

namespace s8::runtime {
namespace {

constexpr std::chrono::microseconds SCAN_BUDGET{500};
constexpr std::uint64_t PUBLISH_INTERVAL_MS = 100;
constexpr std::size_t REGIONS_PER_TICK = 32;
constexpr std::size_t REFINE_CANDIDATES_PER_TICK = 8;
constexpr std::uint16_t MAXIMUM_RADIUS = 4096;

bool readable(const MEMORY_BASIC_INFORMATION& region) {
    const DWORD protection = region.Protect & 0xff;

    return region.State == MEM_COMMIT && !(region.Protect & PAGE_GUARD) &&
        (protection == PAGE_READONLY || protection == PAGE_READWRITE || protection == PAGE_WRITECOPY ||
         protection == PAGE_EXECUTE_READ || protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY);
}

std::uintptr_t region_end(const MEMORY_BASIC_INFORMATION& region) {
    const std::uintptr_t begin = reinterpret_cast<std::uintptr_t>(region.BaseAddress);

    return region.RegionSize <= std::numeric_limits<std::uintptr_t>::max() - begin
        ? begin + region.RegionSize : std::numeric_limits<std::uintptr_t>::max();
}

bool read_view(domain::DebugSnapshot& snapshot, std::uintptr_t address, std::size_t rows) {
    snapshot.view_address = address;
    snapshot.view_size = std::clamp(rows, std::size_t{1}, std::size_t{256}) * 16;
    snapshot.readable.fill(false);
    snapshot.bytes.fill(std::byte{});
    std::size_t offset = 0;
    bool any_readable = false;

    if (!address || snapshot.view_size > std::numeric_limits<std::uintptr_t>::max() - address) {
        return false;
    }
    while (offset < snapshot.view_size) {
        MEMORY_BASIC_INFORMATION region{};
        const std::uintptr_t cursor = address + offset;

        if (VirtualQuery(reinterpret_cast<const void*>(cursor), &region, sizeof(region)) != sizeof(region)) {
            break;
        }
        const std::uintptr_t end = region_end(region);

        if (end <= cursor) {
            break;
        }
        const std::size_t count = std::min(snapshot.view_size - offset, static_cast<std::size_t>(end - cursor));

        if (readable(region) && platform::windows::read_memory(cursor,
                std::span(snapshot.bytes).subspan(offset, count))) {
            std::fill_n(snapshot.readable.begin() + offset, count, true);
            any_readable = true;
        }
        offset += count;
    }
    return any_readable;
}

bool matches_value(const std::byte* bytes, const domain::MemoryValue& value) {
    if (value.type == domain::MemoryValueType::Float) {
        float candidate = 0;
        float expected = 0;

        std::memcpy(&candidate, bytes, sizeof(candidate));
        std::memcpy(&expected, value.bytes.data(), sizeof(expected));
        return candidate == expected;
    }
    if (value.type == domain::MemoryValueType::Double) {
        double candidate = 0;
        double expected = 0;

        std::memcpy(&candidate, bytes, sizeof(candidate));
        std::memcpy(&expected, value.bytes.data(), sizeof(expected));
        return candidate == expected;
    }
    return std::memcmp(bytes, value.bytes.data(), domain::memory_value_size(value.type)) == 0;
}

void finish_scan(DebugTools& state, domain::DebugStatus status) {
    if (state.snapshot.refining) {
        state.snapshot.matches = state.refine_matches;
    }
    state.snapshot.scanning = false;
    state.snapshot.refining = false;
    state.snapshot.status = status;
    state.dirty = true;
}

void load_scan_block(DebugTools& state) {
    const std::size_t width = domain::memory_value_size(state.query.type);

    for (std::size_t visited = 0; visited < REGIONS_PER_TICK && state.cursor < state.end; ++visited) {
        MEMORY_BASIC_INFORMATION region{};

        if (VirtualQuery(reinterpret_cast<const void*>(state.cursor), &region, sizeof(region)) != sizeof(region)) {
            state.cursor = state.end;
            break;
        }
        std::uintptr_t end = std::min(region_end(region), state.end);
        const std::uintptr_t storage_begin = reinterpret_cast<std::uintptr_t>(&*state.storage);
        const std::uintptr_t storage_end = storage_begin + sizeof(MemoryScanStorage);

        // Do not discover matches that the scanner itself just copied into
        // its reusable block/result buffers.
        if (state.cursor >= storage_begin && state.cursor < storage_end) {
            state.cursor = storage_end;
            continue;
        }
        if (state.cursor < storage_begin && end > storage_begin) {
            end = storage_begin;
        }

        if (end <= state.cursor) {
            state.cursor = state.end;
            break;
        }
        if (!readable(region) || end - state.cursor < width) {
            state.cursor = end;
            continue;
        }

        const std::size_t remaining = static_cast<std::size_t>(end - state.cursor);
        const std::size_t bytes = std::min(remaining, MemoryScanStorage::BLOCK_SIZE + width - 1);
        const std::size_t advance = std::min(remaining, MemoryScanStorage::BLOCK_SIZE);

        if (!platform::windows::read_memory(state.cursor, std::span(state.storage->block).first(bytes))) {
            state.cursor += advance;
            state.snapshot.scanned_bytes += advance;
            return;
        }
        state.block_address = state.cursor;
        state.block_candidates = bytes - width + 1;
        state.block_cursor = 0;
        state.block_advance = advance;
        return;
    }
    if (state.cursor >= state.end) {
        finish_scan(state, domain::DebugStatus::Complete);
    }
}

void search_tick(DebugTools& state, std::chrono::steady_clock::time_point deadline) {
    if (state.block_cursor == state.block_candidates) {
        state.block_candidates = 0;
        state.block_cursor = 0;
        load_scan_block(state);
    }
    if (!state.snapshot.scanning || !state.block_candidates) {
        return;
    }
    const std::size_t width = domain::memory_value_size(state.query.type);

    while (state.block_cursor < state.block_candidates) {
        const std::size_t offset = state.block_cursor++;

        if (matches_value(state.storage->block.data() + offset, state.query)) {
            domain::MemoryMatch& match = state.storage->results[state.snapshot.matches++];

            match = {};
            match.address = state.block_address + offset;
            std::memcpy(match.value.data(), state.storage->block.data() + offset, width);
            if (state.snapshot.matches == domain::DEBUG_RESULT_LIMIT) {
                state.snapshot.result_limit_reached = true;
                finish_scan(state, domain::DebugStatus::Complete);
                return;
            }
        }
        if ((state.block_cursor & 1023) == 0 && std::chrono::steady_clock::now() >= deadline) {
            return;
        }
    }
    state.cursor += state.block_advance;
    state.snapshot.scanned_bytes += state.block_advance;
}

bool offset_address(std::uintptr_t address, std::int32_t offset, std::uintptr_t& result) {
    if (offset < 0) {
        const std::uintptr_t distance = static_cast<std::uintptr_t>(-static_cast<std::int64_t>(offset));

        if (address < distance) {
            return false;
        }
        result = address - distance;
    } else {
        const std::uintptr_t distance = static_cast<std::uintptr_t>(offset);

        if (distance > std::numeric_limits<std::uintptr_t>::max() - address) {
            return false;
        }
        result = address + distance;
    }
    return true;
}

void refine_tick(DebugTools& state, std::chrono::steady_clock::time_point deadline) {
    const std::size_t width = domain::memory_value_size(state.query.type);

    for (std::size_t processed = 0; processed < REFINE_CANDIDATES_PER_TICK &&
            state.refine_cursor < state.refine_source_size; ++processed) {
        const domain::MemoryMatch source = state.storage->results[state.refine_cursor++];
        std::uintptr_t center = 0;

        if (!offset_address(source.address, state.refine_offset, center) || center < state.refine_radius) {
            continue;
        }

        const std::uintptr_t start = center - state.refine_radius;
        const std::size_t size = static_cast<std::size_t>(state.refine_radius) * 2 + width;

        if (size > std::numeric_limits<std::uintptr_t>::max() - start ||
            !platform::windows::is_committed_memory(start, size) ||
            !platform::windows::read_memory(start, std::span(state.storage->block).first(size))) {
            continue;
        }
        state.snapshot.scanned_bytes += size;
        for (std::size_t offset = 0; offset + width <= size; ++offset) {
            if (matches_value(state.storage->block.data() + offset, state.query)) {
                domain::MemoryMatch& match = state.storage->results[state.refine_matches++];

                match = {};
                match.address = start + offset;
                std::memcpy(match.value.data(), state.storage->block.data() + offset, width);
                break;
            }
        }
        if (std::chrono::steady_clock::now() >= deadline) {
            break;
        }
    }
    if (state.refine_cursor == state.refine_source_size) {
        finish_scan(state, domain::DebugStatus::Complete);
    } else {
        state.snapshot.matches = state.refine_matches;
    }
}

domain::DebugStatus write_byte(const domain::DebugRequest& request) {
    MEMORY_BASIC_INFORMATION region{};
    std::byte current{};

    if (!request.address || VirtualQuery(reinterpret_cast<const void*>(request.address),
            &region, sizeof(region)) != sizeof(region) || !readable(region)) {
        return domain::DebugStatus::WriteFailed;
    }
    const DWORD protection = region.Protect & 0xff;
    const bool writable = protection == PAGE_READWRITE || protection == PAGE_WRITECOPY ||
        protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY;

    if (!writable || !platform::windows::read_memory(request.address, std::span(&current, 1))) {
        return domain::DebugStatus::WriteFailed;
    }
    if (current != request.expected) {
        return domain::DebugStatus::Conflict;
    }
    if (!platform::windows::write_memory(request.address, std::span(&request.replacement, 1)) ||
        !platform::windows::read_memory(request.address, std::span(&current, 1)) || current != request.replacement) {
        return domain::DebugStatus::WriteFailed;
    }
    if ((protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY) &&
        !FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<const void*>(request.address), 1)) {
        return domain::DebugStatus::WriteFailed;
    }
    return domain::DebugStatus::Ready;
}

domain::DebugStatus apply_request(DebugTools& state, const domain::DebugRequest& request) {
    if (request.epoch.value != state.snapshot.epoch.value) {
        return domain::DebugStatus::SessionChanged;
    }
    switch (request.action) {
    case domain::DebugAction::Read:
        return read_view(state.snapshot, request.address, request.count)
            ? domain::DebugStatus::Ready : domain::DebugStatus::ReadFailed;
    case domain::DebugAction::Capture:
        read_view(state.snapshot, request.address, request.count);
        state.snapshot.capture_address = state.snapshot.view_address;
        state.snapshot.capture_size = state.snapshot.view_size;
        state.snapshot.capture = state.snapshot.bytes;
        state.snapshot.capture_readable = state.snapshot.readable;
        return domain::DebugStatus::Ready;
    case domain::DebugAction::ClearCapture:
        state.snapshot.capture_size = 0;
        return domain::DebugStatus::Ready;
    case domain::DebugAction::Write: {
        const domain::DebugStatus result = write_byte(request);

        read_view(state.snapshot, state.snapshot.view_address, state.snapshot.view_size / 16);
        return result;
    }
    case domain::DebugAction::Search: {
        if (!domain::memory_value_size(request.value.type)) {
            return domain::DebugStatus::InvalidRequest;
        }
        if (!state.storage) {
            state.storage = own::make_unique<MemoryScanStorage>();
        }
        SYSTEM_INFO system{};

        GetSystemInfo(&system);
        state.cursor = reinterpret_cast<std::uintptr_t>(system.lpMinimumApplicationAddress);
        state.end = reinterpret_cast<std::uintptr_t>(system.lpMaximumApplicationAddress);
        state.query = request.value;
        state.block_cursor = 0;
        state.block_candidates = 0;
        state.snapshot.matches = 0;
        state.snapshot.page = 0;
        state.snapshot.scanned_bytes = 0;
        state.snapshot.scan_type = request.value.type;
        state.snapshot.scanning = true;
        state.snapshot.refining = false;
        state.snapshot.result_limit_reached = false;
        state.snapshot.status = domain::DebugStatus::Scanning;
        return domain::DebugStatus::Ready;
    }
    case domain::DebugAction::Refine:
        if (state.snapshot.scanning || !state.storage || !state.snapshot.matches ||
            request.radius > MAXIMUM_RADIUS || !domain::memory_value_size(request.value.type)) {
            return domain::DebugStatus::InvalidRequest;
        }
        state.query = request.value;
        state.refine_cursor = 0;
        state.refine_matches = 0;
        state.refine_source_size = state.snapshot.matches;
        state.snapshot.matches = 0;
        state.refine_offset = request.offset;
        state.refine_radius = request.radius;
        state.snapshot.scanning = true;
        state.snapshot.refining = true;
        state.snapshot.result_limit_reached = false;
        state.snapshot.scanned_bytes = 0;
        state.snapshot.scan_type = request.value.type;
        state.snapshot.page = 0;
        state.snapshot.status = domain::DebugStatus::Scanning;
        return domain::DebugStatus::Ready;
    case domain::DebugAction::Cancel:
        finish_scan(state, domain::DebugStatus::Cancelled);
        return domain::DebugStatus::Ready;
    case domain::DebugAction::ResultPage:
        state.snapshot.page = request.count;
        return domain::DebugStatus::Ready;
    }
    return domain::DebugStatus::InvalidRequest;
}

void publish(DebugTools& state, services::DebugBridge& bridge, std::uint64_t now_ms) {
    const std::size_t matches = state.snapshot.refining ? state.refine_matches : state.snapshot.matches;
    const std::size_t maximum_page = matches ? (matches - 1) / domain::DEBUG_RESULT_PAGE : 0;

    state.snapshot.page = std::min(state.snapshot.page, maximum_page);
    state.snapshot.page_count = std::min(domain::DEBUG_RESULT_PAGE,
        matches - state.snapshot.page * domain::DEBUG_RESULT_PAGE);
    for (std::size_t index = 0; index < state.snapshot.page_count; ++index) {
        state.snapshot.results[index] = state.storage->results[state.snapshot.page * domain::DEBUG_RESULT_PAGE + index];
    }
    ++state.snapshot.revision;
    bridge.publish(state.snapshot);
    state.last_publish_ms = now_ms;
    state.dirty = false;
}

} // namespace

void update_debug_tools(DebugTools& state, services::DebugBridge& bridge, domain::GameEpoch epoch,
    std::uintptr_t module, std::uintptr_t game_base, std::uintptr_t player, std::uint64_t now_ms) {
    const platform::windows::RealClockScope real_clock;

    if (state.stopped) {
        return;
    }
    if (!state.initialized || state.snapshot.epoch.value != epoch.value) {
        const std::uint64_t revision = state.snapshot.revision;

        state.snapshot = {};
        state.snapshot.revision = revision;
        state.snapshot.epoch = epoch;
        state.snapshot.status = state.initialized ? domain::DebugStatus::SessionChanged : domain::DebugStatus::Ready;
        state.initialized = true;
        state.dirty = true;
    }
    state.snapshot.module = module;
    state.snapshot.game_base = game_base;
    state.snapshot.player = player;
    if (const std::optional<domain::DebugRequest> request = bridge.pop()) {
        state.snapshot.request_status = apply_request(state, *request);
        state.snapshot.completed_request = request->id;
        state.dirty = true;
    }
    if (state.snapshot.scanning) {
        const std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::now() + SCAN_BUDGET;

        if (state.snapshot.refining) {
            refine_tick(state, deadline);
        } else {
            search_tick(state, deadline);
        }
    }
    if (state.dirty || (state.snapshot.scanning && now_ms - state.last_publish_ms >= PUBLISH_INTERVAL_MS)) {
        publish(state, bridge, now_ms);
    }
}

void stop_debug_tools(DebugTools& state, services::DebugBridge& bridge) {
    if (state.stopped) {
        return;
    }
    state.stopped = true;
    finish_scan(state, domain::DebugStatus::Cancelled);
    bridge.close();
    publish(state, bridge, state.last_publish_ms);
}

} // namespace s8::runtime
