#include "mission_cpu_exclusion.h"
#include "../platform/windows/code_memory.h"
#include "../platform/windows/process_memory.h"

namespace s8::game {
namespace {

// WOOSEOK99/S8RPKCheats@068ca1d, Civilian/MissionCpuHeroExclusion.cpp.
// Preserve RFLAGS/RAX; skip forced CPU registration only for the protagonist,
// status kind 4..9, with the volunteer marker at +0x350 equal to zero.
constexpr std::array ORIGINAL = native_hex("498D7F20BE05000000");
constexpr std::array GUARD = native_hex("483B98E0000000");
constexpr std::array CAVE_TEMPLATE = native_hex(
    "9C5048B80000000000000000488B004885C07439483B98E00000007530488B43104885C074270FB6400883F804721E"
    "83F809771980BB50030000007510589DFF25000000000000000000000000589D498D7F20BE05000000FF25000000000000000000000000");
static_assert(CAVE_TEMPLATE.size() == 102);

bool prepare_mission_cpu_exclusion(MissionCpuExclusion& state, std::uintptr_t base) {
    state.patches.last_result = platform::windows::CodePatchResult::ExpectedBytesMismatch;

    if (!native_bytes_match(base + 0x01453801, ORIGINAL)) {
        return false;
    }

    state.patches.last_result = platform::windows::CodePatchResult::WriteFailed;

    const std::uintptr_t cave = platform::windows::allocate_code_near(base + 0x01453801, 0x100);

    if (!cave) {
        return false;
    }

    std::array<std::byte, CAVE_TEMPLATE.size()> code = CAVE_TEMPLATE;
    NativePatchGroup patches{};

    if (!put_native_pointer(code, 4, base + 0x02e98bc8) ||
        !put_native_pointer(code, 69, base + 0x01453870) ||
        !put_native_pointer(code, 94, base + 0x0145380a) ||
        !append_native_branch(patches, base + 0x01453801, cave, ORIGINAL) ||
        !platform::windows::write_code_memory(cave, code)) {
        platform::windows::free_code(cave);
        return false;
    }
    state.module_base = base;
    state.cave = cave;
    state.patches = patches;
    return true;
}

} // namespace

bool set_mission_cpu_exclusion(MissionCpuExclusion& state, std::uintptr_t module_base, bool enabled) {
    if (!module_base || (state.module_base && state.module_base != module_base)) {
        state.patches.last_result = platform::windows::CodePatchResult::InvalidInput;
        return false;
    }
    if (enabled && !native_bytes_match(module_base + 0x01453790, GUARD)) {
        state.patches.last_result = platform::windows::CodePatchResult::ExpectedBytesMismatch;
        return false;
    }
    if (enabled && !state.cave && !prepare_mission_cpu_exclusion(state, module_base)) {
        return false;
    }
    return set_native_patch_group(state.patches, enabled);
}

} // namespace s8::game
