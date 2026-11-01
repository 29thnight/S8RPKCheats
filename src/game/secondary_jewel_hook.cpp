#include "secondary_jewel_hook.h"

#include "../platform/windows/hook.h"
#include "../platform/windows/process_memory.h"
#include "../../third_party/minhook/include/MinHook.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <array>
#include <atomic>
#include <cstring>
#include <filesystem>
#include <span>

namespace s8::game {
namespace {

using Judge = bool (__fastcall*)(std::uint16_t);
std::array<std::atomic<std::uint64_t>, 8> selected{};
std::atomic<std::uintptr_t> game_base_address{0};
std::atomic<std::uintptr_t> cache_base{0};
std::array<std::atomic<std::uint8_t>, 501> cache{};
std::array<std::atomic<std::uint64_t>, 501> last_probe{};
platform::windows::Hook hook{};
bool installed = false;

bool __fastcall secondary_jewel_detour(std::uint16_t id) {
    if (id <= 500 && (selected[id >> 6].load(std::memory_order_relaxed) & (std::uint64_t{1} << (id & 63)))) {
        std::uintptr_t game_base = 0;
        const auto address = game_base_address.load(std::memory_order_acquire);
        if (address && platform::windows::read_memory(address, std::as_writable_bytes(std::span(&game_base, 1))) && game_base) {
            const auto raw_id = static_cast<std::uint16_t>(id + 0x40);
            const auto byte_index = raw_id >> 3;
            std::uint8_t bitmap = 0;
            if (byte_index < 34 && platform::windows::read_memory(game_base + 0x71e0 + byte_index,
                    std::as_writable_bytes(std::span(&bitmap, 1))) && (bitmap & (1u << (raw_id & 7)))) {
                if (cache_base.exchange(game_base, std::memory_order_acq_rel) != game_base) {
                    reset_secondary_jewel_cache();
                    cache_base.store(game_base, std::memory_order_release);
                }
                auto status = cache[id].load(std::memory_order_relaxed);
                const auto now = GetTickCount64();
                if (status == 0 || (status == 1 && now - last_probe[id].load(std::memory_order_relaxed) >= 250)) {
                    std::uintptr_t first = 0, second = 0;
                    const auto slot = game_base + sizeof(std::uintptr_t) * (static_cast<std::uintptr_t>(id) + 0xb11de);
                    const bool available = platform::windows::read_memory(slot, std::as_writable_bytes(std::span(&first, 1))) &&
                        first && platform::windows::read_memory(first, std::as_writable_bytes(std::span(&second, 1))) && second;
                    last_probe[id].store(now, std::memory_order_relaxed);
                    status = available ? 2 : 1;
                    cache[id].store(status, std::memory_order_relaxed);
                }
                if (status == 2) {
                    return true;
                }
            }
        }
    }
    return reinterpret_cast<Judge>(hook.original)(id);
}

} // namespace

void reset_secondary_jewel_cache() {
    for (auto& value : cache) {
        value.store(0, std::memory_order_relaxed);
    }
    for (auto& value : last_probe) {
        value.store(0, std::memory_order_relaxed);
    }
}

bool set_secondary_jewels(const domain::SecondaryJewelMask& mask, std::uintptr_t module_base,
                          std::uintptr_t game_base_pointer) {
    bool any = false;
    for (const auto word : mask.words) {
        any = any || word != 0;
    }
    if (!installed) {
        if (!module_base) {
            return false;
        }
        const auto target = module_base + 0x17abbf0;
        constexpr std::array EXPECTED{std::byte{0x48}, std::byte{0x89}, std::byte{0x5c}, std::byte{0x24}, std::byte{0x08}};
        std::array<std::byte, 5> current{};
        if (!platform::windows::read_memory(target, current)) {
            return false;
        }
        if (current != EXPECTED) {
            // VERSION.dll v1.4.7 already owns this judge. Use its locked mask
            // accessors instead of installing another hook over its trampoline.
            if (current[0] != std::byte{0xe9}) {
                return false;
            }

            std::int32_t displacement = 0;
            std::memcpy(&displacement, current.data() + 1, sizeof(displacement));
            const std::uintptr_t relay = static_cast<std::uintptr_t>(
                static_cast<std::intptr_t>(target + 5) + displacement);
            std::array<std::byte, 14> jump{};

            if (!platform::windows::read_memory(relay, jump) || jump[0] != std::byte{0xff} ||
                jump[1] != std::byte{0x25} || jump[2] != std::byte{} || jump[3] != std::byte{} ||
                jump[4] != std::byte{} || jump[5] != std::byte{}) {
                return false;
            }

            std::uintptr_t judge = 0;
            HMODULE owner = nullptr;
            std::memcpy(&judge, jump.data() + 6, sizeof(judge));
            if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                    reinterpret_cast<LPCWSTR>(judge), &owner) ||
                judge != reinterpret_cast<std::uintptr_t>(owner) + 0xb42c0) {
                return false;
            }

            std::array<wchar_t, 32768> game_path{};
            std::array<wchar_t, 32768> owner_path{};
            if (!GetModuleFileNameW(reinterpret_cast<HMODULE>(module_base), game_path.data(),
                    static_cast<DWORD>(game_path.size())) ||
                !GetModuleFileNameW(owner, owner_path.data(), static_cast<DWORD>(owner_path.size()))) {
                return false;
            }
            const std::filesystem::path expected_owner =
                std::filesystem::path(game_path.data()).parent_path() / L"version.dll";
            if (CompareStringOrdinal(owner_path.data(), -1, expected_owner.c_str(), -1, TRUE) != CSTR_EQUAL) {
                return false;
            }

            const std::uintptr_t external_base = reinterpret_cast<std::uintptr_t>(owner);
            constexpr std::array<std::uint8_t, 16> SETTER_BYTES{
                0x48, 0x89, 0x5c, 0x24, 0x08, 0x57, 0x48, 0x83, 0xec, 0x20, 0x8d, 0x59, 0xff, 0xb8, 0xf3, 0x01};
            constexpr std::array<std::uint8_t, 16> GETTER_BYTES{
                0x40, 0x57, 0x48, 0x83, 0xec, 0x20, 0x8d, 0x79, 0xff, 0xb8, 0xf3, 0x01, 0x00, 0x00, 0x66, 0x3b};
            std::array<std::uint8_t, 16> setter_bytes{};
            std::array<std::uint8_t, 16> getter_bytes{};

            if (!platform::windows::read_memory(external_base + 0xb3c00, std::as_writable_bytes(std::span(setter_bytes))) ||
                !platform::windows::read_memory(external_base + 0xb3b60, std::as_writable_bytes(std::span(getter_bytes))) ||
                setter_bytes != SETTER_BYTES || getter_bytes != GETTER_BYTES) {
                return false;
            }

            using ExternalSet = void (__fastcall*)(std::uint16_t, bool);
            using ExternalGet = bool (__fastcall*)(std::uint16_t);
            const ExternalSet set = reinterpret_cast<ExternalSet>(external_base + 0xb3c00);
            const ExternalGet get = reinterpret_cast<ExternalGet>(external_base + 0xb3b60);

            for (const std::uint16_t id : domain::SECONDARY_JEWEL_IDS) {
                const bool enabled = domain::secondary_jewel_selected(mask, id);

                set(id, enabled);
                if (get(id) != enabled) {
                    return false;
                }
            }
            return true;
        }
        if (!any) {
            return true;
        }
        const auto initialized = platform::windows::initialize_hooks();
        if (initialized != MH_OK && initialized != MH_ERROR_ALREADY_INITIALIZED) {
            return false;
        }
        const auto created = platform::windows::create_hook(reinterpret_cast<void*>(target),
                                                             reinterpret_cast<void*>(&secondary_jewel_detour));
        if (!created) {
            return false;
        }
        hook = *created;
        const auto status = platform::windows::enable_hook(hook);
        if (status != MH_OK && status != MH_ERROR_ENABLED) {
            platform::windows::remove_hook(hook);
            hook = {};
            return false;
        }
        installed = true;
    }
    game_base_address.store(game_base_pointer, std::memory_order_release);
    reset_secondary_jewel_cache();
    cache_base.store(0, std::memory_order_release);
    for (std::size_t i = 0; i < selected.size(); ++i) {
        selected[i].store(mask.words[i], std::memory_order_release);
    }
    return true;
}

} // namespace s8::game
