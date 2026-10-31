#include "council_continue.h"
#include "../platform/windows/code_memory.h"
#include "../platform/windows/process_memory.h"

#include <algorithm>

namespace s8::game {
namespace {

// Exact six-entry template from WOOSEOK99/S8RPKCheats@068ca1d,
// War/CouncilContinueAfterMove.h. Keeps action-completed bit 0 and clears
// council-ending bit 1 for the original move/assign/battle conditions.

constexpr std::array MOVE_ORIGINAL = native_hex(
    "488B4358BA3C000000B903000000");

constexpr std::array FLAGS_ORIGINAL = native_hex(
    "838B2003000003660F1F840000000000");

constexpr std::array WAR_ORIGINAL = native_hex(
    "488B0BBA030000004883C1104889742438");

constexpr std::array CAPTURE_ORIGINAL = native_hex(
    "C644244000C6442438004488642430");

constexpr std::array PHASE_ORIGINAL = native_hex(
    "F68320030000020F85E1000000488B4B10");

constexpr std::array ASSIGN_ORIGINAL = native_hex(
    "488B4350BA3C000000B903000000");

constexpr std::array CAVE_TEMPLATE = native_hex(
    "488b4358ba3c000000b903000000415241539c4885c074334c8b50204d85d2742a4c3b536874244c8b1d520400004d8b"
    "1b4d85db7415493983e0000000750c80a020030000fdba030000009d415b415aff256a040000cccccccccccccccccccc"
    "9c50515241504151415241534839f30f84d50000004c8b15040400004d8b124d85d20f84c20000004939b2e00000000f"
    "85b50000004c396e200f85ab0000004c396b200f85a10000004c8b15d8030000498b124885d20f848e0000004883fa40"
    "0f87840000004c8b15c3030000498b0a4885c97475488b014885c074644c8b15b40300004c39107558488b8078040000"
    "4885c07455488b40084885c0744c488b40404885c074434c8b15920300004c391074204c8b158e0300004c3910752b48"
    "39705075254c396858741f4c3968387519eb304839705875114c396868740beb224883c10848ffca758b415b415a4159"
    "41585a59589d838b2003000003ff2565030000415b415a415941585a59589dff2553030000cccccccccccccccccccccc"
    "9c50415241534c8b15230300004c39177561488b05e7020000488b004885c07452488b80e00000004885c074464c8b5f"
    "384d85db743d48837f300074364c395f3074304c395820752a4c8b50184d85d2742141534d8b9b900000004d85db7411"
    "4d395310415b750b80a020030000fdeb02415b415b415a589d488b0bba030000004883c1104889742438ff25c0020000"
    "9c5041524153488b0563020000488b004885c00f849b000000483998e00000000f858e00000048c7050f020000000000"
    "004c8d9030e01e004c39d67577803e007572f680e4710000017569f683200300000275604d85ed745b4c39a8b0df1e00"
    "75524c8b53204d85d274494d39ea74444c8b5b184d85db743b4c399810df1e00753248891dbf0100004c8915c0010000"
    "4c892dc10100004c891dc2010000440fb798aa73000044891dbb0100004889058c010000415b415a589dc644244000c6"
    "442438004488642430ff25f9010000cc9c5041524153488b05630100004885c00f84cd00000048391d5b0100000f85c0"
    "00000048c70542010000000000004839c50f85ac000000483998e00000000f859f0000004c8b158d0100004c39160f85"
    "8f00000083be78040000040f8582000000440fb790aa730000443915280100007571f680e47100000175684c8b15fe00"
    "00004c39968804000075584c8b15fe0000004c395318754b4c39968004000075424c8b1de00000004c395b2075354d8b"
    "9b900000004d85db74294d39531075234c8b5b104d85db741a41807b080d741380a320030000fd415b415a589dff251d"
    "010000415b415a589df68320030000027406ff2510010000488b4b10ff250e010000cccccccccccccccccccccccccccc"
    "488b4350ba3c000000b903000000415241539c4885c074394c8b50204d85d274304c3b5358742a4c3b533875244c8b1d"
    "5c0000004d8b1b4d85db7415493983e0000000750c80a020030000fdba030000009d415b415aff25ac000000cccccccc"
    "000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
    "111111111111111122222222222222223333333333333333444444444444444455555555555555556666666666666666"
    "777777777777777788888888888888889999999999999999aaaaaaaaaaaaaaaabbbbbbbbbbbbbbbbcccccccccccccccc"
    "ddddddddddddddddeeeeeeeeeeeeeeeeababababababababcdcdcdcdcdcdcdcd");

static_assert(CAVE_TEMPLATE.size() == 0x500);

struct AddressRelocation {
    std::size_t field = 0;
    std::uintptr_t rva = 0;
};

constexpr std::array<AddressRelocation, 16> RELOCATIONS{{
    {0x480, 0x02e98bc8}, {0x488, 0x02e99220}, {0x490, 0x02e99230},
    {0x498, 0x0269d3d0}, {0x4a0, 0x026a7fc8}, {0x4a8, 0x026a7f98},
    {0x4b0, 0x025bff20}, {0x4b8, 0x026cf3d8}, {0x4c0, 0x019622fb},
    {0x4c8, 0x017b1a80}, {0x4d0, 0x0144148a}, {0x4d8, 0x01e4dc70},
    {0x4e0, 0x01bd32be}, {0x4e8, 0x01bd3334}, {0x4f0, 0x01bd3257},
    {0x4f8, 0x0196466e}
}};

bool prepare_council_continue(CouncilContinue& state, std::uintptr_t base) {
    state.patches.last_result = platform::windows::CodePatchResult::ExpectedBytesMismatch;

    constexpr std::array<std::uintptr_t, 6> sites{{
        0x019622ed, 0x017b1a70, 0x01441479, 0x01e4dc61, 0x01bd3246, 0x01964660
    }};
    constexpr std::array<std::size_t, 6> entries{{0, 0x60, 0x180, 0x210, 0x2e0, 0x3f0}};
    const std::array<std::span<const std::byte>, 6> originals{{
        MOVE_ORIGINAL, FLAGS_ORIGINAL, WAR_ORIGINAL, CAPTURE_ORIGINAL, PHASE_ORIGINAL, ASSIGN_ORIGINAL
    }};

    for (std::size_t index = 0; index < sites.size(); ++index) {
        if (!native_bytes_match(base + sites[index], originals[index])) {
            return false;
        }
    }

    state.patches.last_result = platform::windows::CodePatchResult::WriteFailed;

    const std::uintptr_t cave = platform::windows::allocate_code_near(base + sites[0], 0x1000);

    if (!cave) {
        return false;
    }

    std::array<std::byte, CAVE_TEMPLATE.size()> code = CAVE_TEMPLATE;
    NativePatchGroup patches{};
    bool valid = true;

    std::fill(code.begin() + 0x450, code.begin() + 0x480, std::byte{0});
    for (const AddressRelocation& relocation : RELOCATIONS) {
        valid = put_native_pointer(code, relocation.field, base + relocation.rva) && valid;
    }
    for (std::size_t index = 0; index < sites.size(); ++index) {
        valid = append_native_absolute_branch(patches, base + sites[index], cave + entries[index],
            originals[index]) && valid;
    }
    if (!valid || !platform::windows::write_code_memory(cave, code)) {
        platform::windows::free_code(cave);
        return false;
    }
    state.module_base = base;
    state.cave = cave;
    state.patches = patches;
    return true;
}

bool commit_with_pending_reset(CouncilContinue& state, bool install_hooks) {
    if (state.patches.count != 6) {
        state.patches.last_result = platform::windows::CodePatchResult::InvalidInput;
        return false;
    }

    std::array<std::byte, 0x30> previous{};
    constexpr std::array<std::byte, 0x30> cleared{};
    std::array<platform::windows::CodePatch, 8> patches{};
    std::size_t count = 0;

    if (!platform::windows::read_memory(state.cave + 0x450, previous)) {
        state.patches.last_result = platform::windows::CodePatchResult::ExpectedBytesMismatch;
        return false;
    }
    if (install_hooks) {
        for (std::size_t index = 0; index < state.patches.count; ++index) {
            const NativePatch& patch = state.patches.entries[index];

            patches[count++] = {patch.address, std::span<const std::byte>(patch.original.data(), patch.size),
                std::span<const std::byte>(patch.replacement.data(), patch.size)};
        }
    }
    patches[count++] = {state.cave + 0x450, std::span<const std::byte>(previous).first(0x18),
        std::span<const std::byte>(cleared).first(0x18)};
    patches[count++] = {state.cave + 0x468, std::span<const std::byte>(previous).subspan(0x18),
        std::span<const std::byte>(cleared).subspan(0x18)};
    state.patches.last_result = platform::windows::commit_code_patches(
        std::span<const platform::windows::CodePatch>(patches.data(), count));
    if (!platform::windows::code_patch_succeeded(state.patches.last_result)) {
        if (state.patches.last_result == platform::windows::CodePatchResult::RollbackFailed ||
            state.patches.last_result == platform::windows::CodePatchResult::ResumeFailed) {
            state.patches.uncertain = true;
        }
        return false;
    }
    if (install_hooks) {
        state.patches.applied = true;
        state.patches.uncertain = false;
    }
    return true;
}

} // namespace

bool set_council_continue(CouncilContinue& state, std::uintptr_t module_base, bool enabled) {
    if (!module_base || (state.module_base && state.module_base != module_base)) {
        state.patches.last_result = platform::windows::CodePatchResult::InvalidInput;
        return false;
    }
    if (!enabled) {
        return set_native_patch_group(state.patches, false);
    }
    if (!state.cave && !prepare_council_continue(state, module_base)) {
        return false;
    }
    return state.patches.applied && !state.patches.uncertain
        ? set_native_patch_group(state.patches, true) : commit_with_pending_reset(state, true);
}

bool reset_council_continue(CouncilContinue& state) {
    return !state.cave || commit_with_pending_reset(state, false);
}

} // namespace s8::game
