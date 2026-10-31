#include "speed_clock.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <intrin.h>
#include "../../../third_party/minhook/include/MinHook.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <limits>

namespace s8::platform::windows {
namespace {

constexpr std::uint64_t ENABLED_BIT = 0x10;
constexpr std::uint64_t GENERATION_STEP = 0x100;
constexpr std::uint64_t MULTIPLIER_MASK = 0x0f;
constexpr std::size_t CLOCK_COUNT = 4;

using QueryCounter = BOOL(WINAPI*)(LARGE_INTEGER*);
using QueryTick32 = DWORD(WINAPI*)();
using QueryTick64 = ULONGLONG(WINAPI*)();

struct ClockSample {
    SRWLOCK lock = SRWLOCK_INIT;
    std::uint64_t real = 0;
    std::uint64_t scaled = 0;
    bool initialized = false;
};

struct ClockHook {
    void* target = nullptr;
    void* original = nullptr;
};

struct SpeedClockState {
    std::array<ClockHook, CLOCK_COUNT> hooks{};
    std::array<ClockSample, CLOCK_COUNT> samples{};
    std::atomic<std::uint64_t> settings{0};
    std::atomic<std::uint32_t> excluded_thread{0};
    std::uintptr_t self_begin = 0;
    std::uintptr_t self_end = 0;
    std::uint64_t counter_cap = 33000;
    std::uint64_t counter_stall = 50000;
    HMODULE time_module = nullptr;
    bool prepared = false;
    bool hooks_enabled = false;
};

// OS callbacks require one stable address; this storage has no heap allocation.
// Hook removal is deliberately not part of normal stop.
SpeedClockState clock_state;
thread_local std::uint32_t real_clock_depth = 0;

bool should_virtualize(const void* return_address) {
    if (real_clock_depth != 0) {
        return false;
    }

    const std::uintptr_t caller = reinterpret_cast<std::uintptr_t>(return_address);

    if (caller >= clock_state.self_begin && caller < clock_state.self_end) {
        return false;
    }

    const std::uint32_t excluded = clock_state.excluded_thread.load(std::memory_order_relaxed);

    return excluded == 0 || excluded != GetCurrentThreadId();
}

std::uint64_t scaled_tick(std::size_t index, std::uint64_t real, bool wraps_32, bool counter) {
    ClockSample& sample = clock_state.samples[index];

    AcquireSRWLockExclusive(&sample.lock);

    // Keep the accumulated offset across multiplier changes and OFF. Returning
    // the raw clock after accelerated time would move the game clock backwards.
    const std::uint64_t settings = clock_state.settings.load(std::memory_order_acquire);
    const bool enabled = (settings & ENABLED_BIT) != 0;

    if (!sample.initialized) {
        sample.real = real;
        sample.scaled = real;
        sample.initialized = true;
        ReleaseSRWLockExclusive(&sample.lock);
        return real;
    }

    std::uint64_t delta = 0;
    bool stale = false;

    if (wraps_32) {
        const std::uint32_t elapsed = static_cast<std::uint32_t>(real - sample.real);

        if (elapsed <= static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max())) {
            delta = elapsed;
        } else {
            stale = true;
        }
    } else if (real >= sample.real) {
        delta = real - sample.real;
    } else {
        stale = true;
    }

    if (stale) {
        const std::uint64_t result = sample.scaled;

        ReleaseSRWLockExclusive(&sample.lock);
        return result;
    }

    const std::uint64_t stall = counter ? clock_state.counter_stall : 50;

    if (enabled && delta <= stall) {
        const std::uint64_t numerator = 2 + (settings & MULTIPLIER_MASK);

        delta = delta * numerator / 2;
    }

    if (enabled && counter) {
        delta = std::min(delta, clock_state.counter_cap);
    }

    sample.real = real;
    sample.scaled += delta;

    if (wraps_32) {
        sample.scaled = static_cast<std::uint32_t>(sample.scaled);
    }

    const std::uint64_t result = sample.scaled;

    ReleaseSRWLockExclusive(&sample.lock);
    return result;
}

BOOL WINAPI scaled_counter(LARGE_INTEGER* output) {
    const QueryCounter original = reinterpret_cast<QueryCounter>(clock_state.hooks[0].original);
    const BOOL success = original(output);

    if (success && should_virtualize(_ReturnAddress())) {
        output->QuadPart = static_cast<LONGLONG>(scaled_tick(
            0, static_cast<std::uint64_t>(output->QuadPart), false, true));
    }

    return success;
}

DWORD WINAPI scaled_tick32() {
    const QueryTick32 original = reinterpret_cast<QueryTick32>(clock_state.hooks[1].original);
    const DWORD real = original();

    return should_virtualize(_ReturnAddress())
        ? static_cast<DWORD>(scaled_tick(1, real, true, false)) : real;
}

ULONGLONG WINAPI scaled_tick64() {
    const QueryTick64 original = reinterpret_cast<QueryTick64>(clock_state.hooks[2].original);
    const ULONGLONG real = original();

    return should_virtualize(_ReturnAddress())
        ? scaled_tick(2, real, false, false) : real;
}

DWORD WINAPI scaled_multimedia_tick() {
    const QueryTick32 original = reinterpret_cast<QueryTick32>(clock_state.hooks[3].original);
    const DWORD real = original();

    return should_virtualize(_ReturnAddress())
        ? static_cast<DWORD>(scaled_tick(3, real, true, false)) : real;
}

std::expected<void, int> prepare_clock_state() {
    if (clock_state.prepared) {
        return {};
    }

    HMODULE self = nullptr;
    const LPCWSTR address = reinterpret_cast<LPCWSTR>(&configure_speed_clock);

    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           address, &self)) {
        return std::unexpected(static_cast<int>(GetLastError()));
    }

    const std::uintptr_t base = reinterpret_cast<std::uintptr_t>(self);
    const IMAGE_DOS_HEADER* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    const IMAGE_NT_HEADERS* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);

    clock_state.self_begin = base;
    clock_state.self_end = base + nt->OptionalHeader.SizeOfImage;

    LARGE_INTEGER frequency{};

    if (!QueryPerformanceFrequency(&frequency) || frequency.QuadPart <= 0) {
        return std::unexpected(static_cast<int>(ERROR_INVALID_DATA));
    }

    clock_state.counter_cap = static_cast<std::uint64_t>(frequency.QuadPart) * 33 / 1000;
    clock_state.counter_stall = static_cast<std::uint64_t>(frequency.QuadPart) * 50 / 1000;
    clock_state.time_module = LoadLibraryExW(L"winmm.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);

    if (!clock_state.time_module) {
        return std::unexpected(static_cast<int>(GetLastError()));
    }

    clock_state.prepared = true;
    return {};
}

std::expected<void, int> install_clock_hooks() {
    if (clock_state.hooks_enabled) {
        return {};
    }

    const std::expected<void, int> prepared = prepare_clock_state();

    if (!prepared) {
        return prepared;
    }

    const MH_STATUS initialization = MH_Initialize();

    if (initialization != MH_OK && initialization != MH_ERROR_ALREADY_INITIALIZED) {
        return std::unexpected(static_cast<int>(initialization));
    }

    constexpr std::array<const char*, CLOCK_COUNT> NAMES{
        "QueryPerformanceCounter", "GetTickCount", "GetTickCount64", "timeGetTime"};
    const std::array<void*, CLOCK_COUNT> detours{
        reinterpret_cast<void*>(&scaled_counter), reinterpret_cast<void*>(&scaled_tick32),
        reinterpret_cast<void*>(&scaled_tick64), reinterpret_cast<void*>(&scaled_multimedia_tick)};
    const HMODULE kernel = GetModuleHandleW(L"kernel32.dll");

    for (std::size_t index = 0; index < CLOCK_COUNT; ++index) {
        ClockHook& hook = clock_state.hooks[index];

        if (hook.original) {
            continue;
        }

        const HMODULE module = index == 3 ? clock_state.time_module : kernel;
        void* const target = reinterpret_cast<void*>(GetProcAddress(module, NAMES[index]));

        if (!target) {
            return std::unexpected(static_cast<int>(GetLastError()));
        }

        const MH_STATUS created = MH_CreateHook(target, detours[index], &hook.original);

        if (created != MH_OK) {
            return std::unexpected(static_cast<int>(created));
        }

        hook.target = target;
    }

    for (const ClockHook& hook : clock_state.hooks) {
        const MH_STATUS enabled = MH_EnableHook(hook.target);

        if (enabled != MH_OK && enabled != MH_ERROR_ENABLED) {
            // No acceleration has been published during a failed first install.
            // Keep every trampoline, but roll back any newly enabled entries.
            for (const ClockHook& created : clock_state.hooks) {

                if (created.original) {
                    MH_DisableHook(created.target);
                }
            }

            return std::unexpected(static_cast<int>(enabled));
        }
    }

    clock_state.hooks_enabled = true;
    return {};
}

} // namespace

std::expected<void, int> configure_speed_clock(domain::SpeedHackSettings settings,
                                               std::uint32_t excluded_thread) {
    settings.multiplier = domain::normalize_speed_multiplier(settings.multiplier);

    if (settings.enabled) {
        const std::expected<void, int> installed = install_clock_hooks();

        if (!installed) {
            return installed;
        }
    }

    clock_state.excluded_thread.store(excluded_thread, std::memory_order_relaxed);

    const std::uint64_t current = clock_state.settings.load(std::memory_order_relaxed);
    const std::uint64_t half_steps = static_cast<std::uint64_t>((settings.multiplier - 1.0f) * 2.0f);
    const std::uint64_t desired = half_steps | (settings.enabled ? ENABLED_BIT : 0);

    if ((current % GENERATION_STEP) != desired) {
        const std::uint64_t next = (current / GENERATION_STEP + 1) * GENERATION_STEP | desired;

        clock_state.settings.store(next, std::memory_order_release);
    }

    return {};
}

int stop_speed_clock() {
    clock_state.settings.fetch_and(~ENABLED_BIT, std::memory_order_release);
    // Native callers continue at 1x with their accumulated offset. Removing the
    // detours would expose raw time and violate monotonicity after acceleration.
    return static_cast<int>(MH_OK);
}

bool speed_clock_has_retained_hooks() {
    return clock_state.hooks[0].original != nullptr;
}

RealClockScope::RealClockScope() {

    ++real_clock_depth;
}

RealClockScope::~RealClockScope() {

    --real_clock_depth;
}

float real_clock_delta(RealClockFrame& frame) {
    constexpr float DEFAULT_DELTA = 1.0f / 60.0f;

    if (frame.frequency <= 0) {
        LARGE_INTEGER frequency{};

        if (!QueryPerformanceFrequency(&frequency) || frequency.QuadPart <= 0) {
            return DEFAULT_DELTA;
        }

        frame.frequency = frequency.QuadPart;
    }

    LARGE_INTEGER now{};

    if (!QueryPerformanceCounter(&now)) {
        return DEFAULT_DELTA;
    }

    const std::int64_t previous = frame.previous;

    frame.previous = now.QuadPart;

    if (previous == 0 || now.QuadPart <= previous) {
        return DEFAULT_DELTA;
    }

    const double elapsed = static_cast<double>(now.QuadPart - previous) / static_cast<double>(frame.frequency);

    return std::min(static_cast<float>(elapsed), 0.1f);
}

} // namespace s8::platform::windows
