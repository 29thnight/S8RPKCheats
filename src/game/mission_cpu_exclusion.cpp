#include "mission_cpu_exclusion.h"
#include "bytecode_buffer.h"
#include "../platform/windows/code_memory.h"
#include "../platform/windows/process_memory.h"

namespace s8::game {
namespace {

// Game ABI: RBX is the officer, R15 supplies the displaced LEA operand.
// Only a volunteer protagonist in status 4..9 bypasses CPU registration.
constexpr std::array ORIGINAL = native_hex("498D7F20BE05000000");
constexpr std::array GUARD = native_hex("483B98E0000000");

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

    BytecodeBuffer<256> code;
    code.emit({0x9c, 0x50}); // save flags and RAX
    code.emit({0x80, 0xbb, 0x50, 0x03, 0, 0, 0}); // volunteer marker
    code.emit({0x0f, 0x85});
    const std::size_t not_volunteer = code.branch();
    code.emit({0x48, 0x8b, 0x43, 0x10, 0x48, 0x85, 0xc0}); // status object
    code.emit({0x0f, 0x84});
    const std::size_t no_status = code.branch();
    code.emit({0x0f, 0xb6, 0x40, 0x08, 0x83, 0xe8, 0x04, 0x83, 0xf8, 0x05});
    code.emit({0x0f, 0x87}); // unsigned (kind - 4) > 5
    const std::size_t other_status = code.branch();
    code.emit({0x48, 0xa1});
    code.address(base + 0x02e98bc8);
    code.emit({0x48, 0x85, 0xc0, 0x0f, 0x84});
    const std::size_t no_root = code.branch();
    code.emit({0x48, 0x3b, 0x98, 0xe0, 0, 0, 0, 0x0f, 0x85});
    const std::size_t other_officer = code.branch();
    code.emit({0x58, 0x9d, 0xff, 0x25, 0, 0, 0, 0});
    code.address(base + 0x01453870);

    for (const std::size_t branch : {not_volunteer, no_status, other_status, no_root, other_officer})
        code.patch(branch);
    code.emit({0x58, 0x9d});
    code.append(ORIGINAL);
    code.emit({0xff, 0x25, 0, 0, 0, 0});
    code.address(base + 0x0145380a);
    NativePatchGroup patches{};

    if (!code.valid() ||
        !append_native_branch(patches, base + 0x01453801, cave, ORIGINAL) ||
        !platform::windows::write_code_memory(cave, code.bytes())) {
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
