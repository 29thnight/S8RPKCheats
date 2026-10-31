#include "stratagem_gauge.h"
#include "../platform/windows/code_memory.h"
#include "../platform/windows/process_memory.h"

#include <optional>

namespace s8::game {
namespace {

// The side-specific processing point and emitted instructions are from
// WOOSEOK99/S8RPKCheats@068ca1d, War/StratagemGaugeMax.cpp. RBX is side+0xf0;
// side is [rbx-0xd8], and its gauge is [rbx+0x64]. The final original CMP
// restores the flags after the conditional write of 10000.
constexpr std::array ORIGINAL = native_hex("817B6410270000");
constexpr std::array PATTERN = native_hex("817B64102700007400B000488B");
constexpr std::array CAVE_TEMPLATE = native_hex(
    "5080BB28FFFFFF00751848B800000000000000008038007428C7436410270000EB1F"
    "80BB28FFFFFF01751648B800000000000000008038007407C743641027000058817B6410270000E900000000");
constexpr std::size_t ATTACK_FLAG_OFFSET = 144;
constexpr std::size_t DEFENSE_FLAG_OFFSET = 145;
static_assert(CAVE_TEMPLATE.size() == 78);

std::optional<std::size_t> find_gauge_hook(std::span<const std::byte> image) {
    std::optional<std::size_t> found{};

    for (std::size_t offset = 0; offset <= image.size() && PATTERN.size() <= image.size() - offset; ++offset) {
        bool matches = true;

        for (std::size_t index = 0; index < PATTERN.size(); ++index) {
            if (index != 8 && index != 10 && image[offset + index] != PATTERN[index]) {
                matches = false;
                break;
            }
        }
        if (matches) {
            if (found.has_value()) {
                return std::nullopt;
            }
            found = offset;
        }
    }
    return found;
}

bool prepare_gauge(StratagemGauge& state, std::uintptr_t module_base,
    std::span<const std::byte> image) {
    state.patches.last_result = platform::windows::CodePatchResult::ExpectedBytesMismatch;

    const std::optional<std::size_t> offset = find_gauge_hook(image);

    if (!offset.has_value() || !native_bytes_match(module_base + *offset, ORIGINAL)) {
        return false;
    }

    const std::uintptr_t hook = module_base + *offset;
    state.patches.last_result = platform::windows::CodePatchResult::WriteFailed;

    const std::uintptr_t cave = platform::windows::allocate_code_near(hook, 160);

    if (!cave) {
        return false;
    }

    std::array<std::byte, CAVE_TEMPLATE.size()> code = CAVE_TEMPLATE;
    NativePatchGroup patches{};

    if (!put_native_pointer(code, 12, cave + ATTACK_FLAG_OFFSET) ||
        !put_native_pointer(code, 45, cave + DEFENSE_FLAG_OFFSET) ||
        !put_native_rel32(code, 74, cave + 74, hook + ORIGINAL.size()) ||
        !append_native_branch(patches, hook, cave, ORIGINAL) ||
        !platform::windows::write_code_memory(cave, code)) {
        platform::windows::free_code(cave);
        return false;
    }
    state.module_base = module_base;
    state.cave = cave;
    state.patches = patches;
    return true;
}

} // namespace

bool set_stratagem_gauge(StratagemGauge& state, std::uintptr_t module_base,
    std::span<const std::byte> image, bool attack, bool defense) {
    if (!module_base || (state.module_base && state.module_base != module_base)) {
        state.patches.last_result = platform::windows::CodePatchResult::InvalidInput;
        return false;
    }
    if (!attack && !defense) {
        if (!set_native_patch_group(state.patches, false)) {
            return false;
        }
        state.attack = false;
        state.defense = false;
        return true;
    }
    if (!state.cave && !prepare_gauge(state, module_base, image)) {
        return false;
    }

    const std::array flags{attack ? std::byte{1} : std::byte{0}, defense ? std::byte{1} : std::byte{0}};
    const std::array previous{state.attack ? std::byte{1} : std::byte{0},
        state.defense ? std::byte{1} : std::byte{0}};

    // The allocation remains RWX. Do not remove execute permission from the
    // shared page while another game thread may be executing this cave.
    if (!platform::windows::write_memory(state.cave + ATTACK_FLAG_OFFSET, flags)) {
        state.patches.last_result = platform::windows::CodePatchResult::WriteFailed;
        return false;
    }
    if (!set_native_patch_group(state.patches, true)) {
        if (!platform::windows::write_memory(state.cave + ATTACK_FLAG_OFFSET, previous)) {
            state.patches.uncertain = true;
            state.patches.last_result = platform::windows::CodePatchResult::RollbackFailed;
        }
        return false;
    }
    state.attack = attack;
    state.defense = defense;
    return true;
}

} // namespace s8::game
