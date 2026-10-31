#include "tengi_filter.h"

#include "tengi.h"
#include "../platform/windows/hook.h"
#include "../platform/windows/process_memory.h"
#include "../../third_party/minhook/include/MinHook.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <span>

namespace s8::game {
namespace {

constexpr std::array<std::uintptr_t, 3> HOOK_RVAS{0x132e340, 0x132e5a0, 0x132e920};
constexpr std::uintptr_t CLEAR_LIST_RVA = 0x10a20;
using CanTrigger = bool(__fastcall*)(void*, bool);
using ListTriggerable = void(__fastcall*)(void*, bool, bool);
using Trigger = bool(__fastcall*)(void*, bool);
using ClearList = void(__fastcall*)(void*);

struct FilterPreferences {
    std::uint32_t mask = domain::ALL_TENGI_EVENTS;
    std::array<int, domain::TENGI_EVENT_COUNT> cooldowns{
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
    bool custom_cooldown = false;
};

struct CooldownOverride {
    SRWLOCK lock = SRWLOCK_INIT;
    std::uintptr_t address = 0;
    std::uint8_t original = 0;
    std::uint8_t written = 0;
    bool active = false;
};

struct FilterBridge {
    SRWLOCK preferences_lock = SRWLOCK_INIT;
    FilterPreferences preferences{};
    std::array<CooldownOverride, domain::TENGI_EVENT_COUNT> cooldowns{};
    std::array<platform::windows::Hook, HOOK_RVAS.size()> hooks{};
    std::atomic<bool> ready{false};
    std::uintptr_t module_base = 0;
    ClearList clear_list = nullptr;
    bool hooks_enabled = false;
};

struct ListContext {
    FilterPreferences preferences{};
    std::uint32_t accepted = 0;
};

// Native callback storage outlives the runtime owner. Disabled trampolines are
// retained because a game thread may already have entered one during shutdown.
FilterBridge bridge;
thread_local ListContext* list_context = nullptr;

FilterPreferences read_preferences() {

    FilterPreferences preferences{};

    AcquireSRWLockShared(&bridge.preferences_lock);
    preferences = bridge.preferences;
    ReleaseSRWLockShared(&bridge.preferences_lock);
    return preferences;
}

void publish_preferences(const domain::TengiSettings& settings) {

    FilterPreferences preferences{};

    preferences.mask = domain::tengi_allowed_mask(settings);

    for (std::size_t index = 0; index < domain::TENGI_EVENT_COUNT; ++index) {

        const domain::TengiEventSettings& event = settings.events[index];

        preferences.cooldowns[index] = event.allowed ? event.cooldown_years : -1;
        preferences.custom_cooldown = preferences.custom_cooldown || preferences.cooldowns[index] >= 0;
    }

    AcquireSRWLockExclusive(&bridge.preferences_lock);
    bridge.preferences = preferences;
    ReleaseSRWLockExclusive(&bridge.preferences_lock);
}

bool read_candidate_id(void* data, std::uint8_t& id) {

    if (reinterpret_cast<std::uintptr_t>(data) <= 0x10000) {
        return false;
    }

    __try {

        id = *(static_cast<const std::uint8_t*>(data) + 8);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool write_cooldown(void* data, std::uint8_t id, int years, CooldownOverride& saved) {

    __try {

        std::uint8_t* const bytes = static_cast<std::uint8_t*>(data);

        if (bytes[8] != id) {
            return false;
        }

        const std::uintptr_t address = reinterpret_cast<std::uintptr_t>(bytes + 0x35);

        if (saved.address != address) {
            saved.address = address;
            saved.original = 0;
            saved.written = 0;
            saved.active = false;
        }

        if (years < 0) {

            if (saved.active && bytes[0x35] == saved.written) {
                bytes[0x35] = saved.original;
            }

            saved.active = false;
            return true;
        }

        const std::uint8_t desired = static_cast<std::uint8_t>(years);
        const std::uint8_t original = !saved.active || bytes[0x35] != saved.written
            ? bytes[0x35] : saved.original;

        bytes[0x35] = desired;
        saved.original = original;
        saved.written = desired;
        saved.active = true;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool apply_candidate_cooldown(void* data, std::uint8_t id, const FilterPreferences& preferences) {

    for (std::size_t index = 0; index < domain::TENGI_EVENT_COUNT; ++index) {

        if (domain::TENGI_EVENT_IDS[index] != id) {
            continue;
        }

        CooldownOverride& saved = bridge.cooldowns[index];

        AcquireSRWLockExclusive(&saved.lock);

        if (!bridge.ready.load(std::memory_order_acquire) ||
            (preferences.cooldowns[index] < 0 && !saved.active)) {
            ReleaseSRWLockExclusive(&saved.lock);
            return true;
        }

        const bool applied = write_cooldown(data, id, preferences.cooldowns[index], saved);

        ReleaseSRWLockExclusive(&saved.lock);
        return applied;
    }

    return true;
}

bool __fastcall can_trigger_detour(void* data, bool ignore_momentum) {

    const CanTrigger original = reinterpret_cast<CanTrigger>(bridge.hooks[0].original);

    if (!bridge.ready.load(std::memory_order_acquire)) {
        return original(data, ignore_momentum);
    }

    ListContext* const context = list_context;
    const FilterPreferences preferences = context ? context->preferences : read_preferences();
    std::uint8_t id = 0;
    const bool readable = read_candidate_id(data, id);

    if (readable && !apply_candidate_cooldown(data, id, preferences)) {
        return false;
    }

    if (!context) {
        return original(data, ignore_momentum);
    }

    if (!readable || !domain::tengi_id_allowed(id, preferences.mask)) {
        return false;
    }

    const bool eligible = original(data, ignore_momentum);

    if (eligible) {
        ++context->accepted;
    }

    return eligible;
}

void __fastcall list_triggerable_detour(void* list, bool ignore_momentum, bool sort) {

    const ListTriggerable original = reinterpret_cast<ListTriggerable>(bridge.hooks[1].original);
    const FilterPreferences preferences = read_preferences();

    if (!bridge.ready.load(std::memory_order_acquire) ||
        (preferences.mask == domain::ALL_TENGI_EVENTS && !preferences.custom_cooldown)) {
        original(list, ignore_momentum, sort);
        return;
    }

    ListContext context{preferences, 0};
    ListContext* const previous = list_context;

    list_context = &context;

    __try {
        original(list, ignore_momentum, sort);
    } __finally {
        list_context = previous;
    }

    if (context.accepted == 0 && bridge.ready.load(std::memory_order_acquire)) {
        // The native list inserts ID 23 without CanTrigger when it is empty.
        // Its own clear() preserves allocator ownership and rejects that fallback.
        bridge.clear_list(list);
    }
}

bool __fastcall trigger_detour(void* data, bool special) {

    const Trigger original = reinterpret_cast<Trigger>(bridge.hooks[2].original);

    if (!bridge.ready.load(std::memory_order_acquire)) {
        return original(data, special);
    }

    const FilterPreferences preferences = read_preferences();
    std::uint8_t id = 0;

    if (preferences.mask != domain::ALL_TENGI_EVENTS &&
        (!read_candidate_id(data, id) || !domain::tengi_id_allowed(id, preferences.mask))) {
        return false;
    }

    return original(data, special);
}

bool matches_code(std::uintptr_t address, std::span<const std::byte> expected) {

    MEMORY_BASIC_INFORMATION memory{};

    if (VirtualQuery(reinterpret_cast<const void*>(address), &memory, sizeof(memory)) != sizeof(memory) ||
        memory.State != MEM_COMMIT || (memory.Protect & PAGE_GUARD) != 0 ||
        (memory.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE |
                           PAGE_EXECUTE_WRITECOPY)) == 0 || expected.size() > 8) {
        return false;
    }

    std::array<std::byte, 8> actual{};
    const std::span<std::byte> bytes = std::span(actual).first(expected.size());

    if (!platform::windows::read_memory(address, bytes)) {
        return false;
    }

    for (std::size_t index = 0; index < expected.size(); ++index) {

        if (actual[index] != expected[index]) {
            return false;
        }
    }

    return true;
}

bool verify_hook_entries(std::uintptr_t module_base) {

    constexpr std::array<std::byte, 6> CAN_TRIGGER{
        std::byte{0x48}, std::byte{0x8b}, std::byte{0xc4}, std::byte{0x57}, std::byte{0x41}, std::byte{0x54}};
    constexpr std::array<std::byte, 5> LIST{
        std::byte{0x48}, std::byte{0x89}, std::byte{0x5c}, std::byte{0x24}, std::byte{0x08}};
    constexpr std::array<std::byte, 6> TRIGGER{
        std::byte{0x40}, std::byte{0x53}, std::byte{0x48}, std::byte{0x83}, std::byte{0xec}, std::byte{0x30}};
    constexpr std::array<std::byte, 6> CLEAR{
        std::byte{0x40}, std::byte{0x53}, std::byte{0x48}, std::byte{0x83}, std::byte{0xec}, std::byte{0x20}};
    constexpr std::array<std::byte, 5> LIST_CALL{
        std::byte{0xe8}, std::byte{0x4e}, std::byte{0xf7}, std::byte{0xff}, std::byte{0xff}};
    const std::array<std::span<const std::byte>, 3> entries{CAN_TRIGGER, LIST, TRIGGER};

    for (std::size_t index = 0; index < entries.size(); ++index) {

        if (!bridge.hooks[index].original && !matches_code(module_base + HOOK_RVAS[index], entries[index])) {
            return false;
        }
    }

    return matches_code(module_base + CLEAR_LIST_RVA, CLEAR) &&
        matches_code(module_base + 0x132ee4d, LIST_CALL);
}

int disable_filter_hooks() {

    bridge.ready.store(false, std::memory_order_release);

    int failure = MH_OK;

    for (const platform::windows::Hook& hook : bridge.hooks) {

        if (!hook.original) {
            continue;
        }

        const int status = platform::windows::disable_hook(hook);

        if (status != MH_OK && status != MH_ERROR_DISABLED && failure == MH_OK) {
            failure = status;
        }
    }

    bridge.hooks_enabled = false;
    return failure;
}

std::expected<void, int> install_filter_hooks(std::uintptr_t module_base) {

    if (bridge.hooks_enabled) {
        return {};
    }

    if ((bridge.module_base != 0 && bridge.module_base != module_base) ||
        !verified_tengi_layout(module_base) || !verify_hook_entries(module_base)) {
        return std::unexpected(static_cast<int>(ERROR_REVISION_MISMATCH));
    }

    const int initialization = platform::windows::initialize_hooks();

    if (initialization != MH_OK && initialization != MH_ERROR_ALREADY_INITIALIZED) {
        return std::unexpected(initialization);
    }

    const std::array<void*, 3> detours{
        reinterpret_cast<void*>(&can_trigger_detour), reinterpret_cast<void*>(&list_triggerable_detour),
        reinterpret_cast<void*>(&trigger_detour)};

    bridge.module_base = module_base;
    bridge.clear_list = reinterpret_cast<ClearList>(module_base + CLEAR_LIST_RVA);

    for (std::size_t index = 0; index < bridge.hooks.size(); ++index) {

        platform::windows::Hook& hook = bridge.hooks[index];

        if (hook.original) {
            continue;
        }

        const std::expected<platform::windows::Hook, int> created = platform::windows::create_hook(
            reinterpret_cast<void*>(module_base + HOOK_RVAS[index]), detours[index]);

        if (!created) {
            return std::unexpected(created.error());
        }

        hook = *created;
    }

    for (const platform::windows::Hook& hook : bridge.hooks) {

        const int status = platform::windows::enable_hook(hook);

        if (status != MH_OK && status != MH_ERROR_ENABLED) {
            disable_filter_hooks();
            return std::unexpected(status);
        }
    }

    bridge.hooks_enabled = true;
    bridge.ready.store(true, std::memory_order_release);
    return {};
}

bool restore_cooldowns(std::uintptr_t module_base) {

    std::uintptr_t manager = 0;

    if (!module_base || !platform::windows::read_memory(
            module_base + 0x2e98bc8, std::as_writable_bytes(std::span(&manager, 1))) || manager <= 0x10000) {
        return true; // A released scenario does not authorize writes to old addresses.
    }

    bool success = true;

    for (std::size_t index = 0; index < domain::TENGI_EVENT_COUNT; ++index) {

        CooldownOverride& saved = bridge.cooldowns[index];
        const std::uint8_t id = domain::TENGI_EVENT_IDS[index];
        std::uintptr_t object = 0;
        const bool found = platform::windows::read_memory(
            manager + 0x58a670 + std::uintptr_t{id} * 8, std::as_writable_bytes(std::span(&object, 1)));

        AcquireSRWLockExclusive(&saved.lock);

        if (saved.active && found && object > 0x10000 && saved.address == object + 0x35) {
            success = write_cooldown(reinterpret_cast<void*>(object), id, -1, saved) && success;
        } else if (!found || saved.address != object + 0x35) {
            saved.active = false;
        }

        ReleaseSRWLockExclusive(&saved.lock);
    }

    return success;
}

} // namespace

std::expected<void, int> configure_tengi_filter(std::uintptr_t module_base,
                                               const domain::TengiSettings& requested) {

    const domain::TengiSettings settings = domain::normalize_tengi_settings(requested);

    publish_preferences(settings);

    const FilterPreferences preferences = read_preferences();

    if (!settings.force_quarterly && preferences.mask == domain::ALL_TENGI_EVENTS &&
        !preferences.custom_cooldown && !bridge.hooks_enabled) {
        return {};
    }

    return install_filter_hooks(module_base);
}

int stop_tengi_filter(std::uintptr_t module_base) {

    bridge.ready.store(false, std::memory_order_release);

    const bool restored = restore_cooldowns(module_base);
    const int disabled = disable_filter_hooks();

    return disabled != MH_OK ? disabled : restored ? MH_OK : static_cast<int>(ERROR_WRITE_FAULT);
}

bool tengi_filter_has_retained_hooks() {

    return bridge.hooks[0].original != nullptr;
}

} // namespace s8::game
