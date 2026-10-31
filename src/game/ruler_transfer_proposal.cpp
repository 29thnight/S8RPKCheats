#include "ruler_transfer_proposal.h"

#include "../platform/windows/code_memory.h"
#include "../platform/windows/process_memory.h"

#include <atomic>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

namespace s8::game {
namespace {

// WOOSEOK99/S8RPKCheats@068ca1d, War/RulerTransferProposal.cpp and
// CouncilCompactSectionBase.h. The original CALL takes RBX in RCX and its
// consumer tests AL. Pass RDI/RSI in volatile argument registers instead of
// publishing them through shared capture variables before entering C++.
constexpr std::uintptr_t CALLSITE = 0x019021f5;
constexpr std::uintptr_t NATIVE = 0x000153e0;
constexpr std::uintptr_t GLOBAL_POINTER = 0x02e98bc8;
constexpr std::array ORIGINAL = native_hex("E8E63171FE");
constexpr std::array PREFIX = native_hex("488BCBE8E63171FE84C075B3");
constexpr std::array PREVIOUS = native_hex("E8E5F4E0FF0FB6F83C01741E");
constexpr std::array THUNK = native_hex("4889FA4989F048B80000000000000000FFE0");
static_assert(THUNK.size() == 18);

std::atomic<std::uintptr_t> callback_base{0};
std::atomic<bool> callback_enabled{false};

template<typename T>
bool read_value(std::uintptr_t address, T& value) {
    return platform::windows::read_memory(address, std::as_writable_bytes(std::span(&value, 1)));
}

std::uint8_t __fastcall ruler_transfer_proposal(std::uintptr_t ruler,
    std::uintptr_t captured_role, std::uintptr_t captured_player) {
    const std::uintptr_t module = callback_base.load(std::memory_order_acquire);

    if (module == 0) {
        return 1;
    }
    using NativeFunction = std::uint8_t(__fastcall*)(std::uintptr_t);
    const NativeFunction native = reinterpret_cast<NativeFunction>(module + NATIVE);
    std::uint8_t original = 1;

    __try {
        original = native(ruler);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 1;
    }
    if (original == 0 || !callback_enabled.load(std::memory_order_acquire)) {
        return original;
    }
    const std::uint8_t role = static_cast<std::uint8_t>(captured_role & 0xff);

    if (role != 2 && role != 4) {
        return original;
    }
    std::uintptr_t root = 0;
    std::uintptr_t player = 0;
    std::uintptr_t owner = 0;
    std::uintptr_t owner_ruler = 0;

    if (!read_value(module + GLOBAL_POINTER, root) || root == 0 ||
        !read_value(root + 0xe0, player) || player == 0 || player != captured_player ||
        !read_value(player + 0x18, owner) || owner == 0 ||
        !read_value(owner + 0x10, owner_ruler) || owner_ruler != ruler) {
        return original;
    }
    return 0;
}

bool prepare_ruler_transfer(RulerTransferProposal& state, std::uintptr_t module_base) {
    state.patches.last_result = platform::windows::CodePatchResult::ExpectedBytesMismatch;

    if (!native_bytes_match(module_base + 0x019021f2, PREFIX) ||
        !native_bytes_match(module_base + 0x019021e6, PREVIOUS)) {
        return false;
    }
    state.patches.last_result = platform::windows::CodePatchResult::WriteFailed;
    const std::uintptr_t cave = platform::windows::allocate_code_near(module_base + CALLSITE, 128);

    if (cave == 0) {
        return false;
    }
    std::array<std::byte, THUNK.size()> code = THUNK;
    NativePatchGroup patches{};

    if (!put_native_pointer(code, 8, reinterpret_cast<std::uintptr_t>(&ruler_transfer_proposal)) ||
        !append_native_branch(patches, module_base + CALLSITE, cave, ORIGINAL, std::byte{0xe8}) ||
        !platform::windows::write_code_memory(cave, code) ||
        !platform::windows::retain_callback_module(reinterpret_cast<std::uintptr_t>(&ruler_transfer_proposal))) {
        platform::windows::free_code(cave);
        return false;
    }
    state.module_base = module_base;
    state.cave = cave;
    state.patches = patches;
    callback_base.store(module_base, std::memory_order_release);
    return true;
}

} // namespace

bool set_ruler_transfer_proposal(RulerTransferProposal& state,
    std::uintptr_t module_base, bool enabled) {
    if (module_base == 0 || (state.module_base != 0 && state.module_base != module_base)) {
        state.patches.last_result = platform::windows::CodePatchResult::InvalidInput;
        return false;
    }
    if (enabled && state.cave == 0 && !prepare_ruler_transfer(state, module_base)) {
        return false;
    }
    callback_enabled.store(enabled, std::memory_order_release);

    // The published thunk and pinned callback remain resident after OFF;
    // a callback already admitted before CALL restoration can still return.
    return set_native_patch_group(state.patches, enabled);
}

} // namespace s8::game
