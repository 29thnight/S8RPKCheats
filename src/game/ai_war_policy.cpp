#include "ai_war_policy.h"
#include "../platform/windows/code_memory.h"
#include "../platform/windows/process_memory.h"

#include <algorithm>

namespace s8::game {
namespace {

// Provenance: WOOSEOK99/S8RPKCheats@068ca1d881edf870f7cf06f935cf50e286004af8,
// War/Roadblock.cpp, AIRefusalWarFix.cpp, AIHighHonorSurrenderFix.cpp.
constexpr std::array ATTACK_ORIGINAL = native_hex("493B4D30740A488B4718493B45387522");
constexpr std::array PRE_ORIGINAL = native_hex("C644242000440FB6CD4D8B8798000000");
constexpr std::array POST_ORIGINAL = native_hex("85ED7408664109BF20010000498B5E10");
constexpr std::array SURRENDER_ORIGINAL = native_hex("48897DC048895DC8488975E8488B4620");
constexpr std::array ATTACK_TEMPLATE = native_hex("49bb1111111111111111f049ff034885c90f846c010000498b55104885d20f845f0100004839d10f84470100004883ec"
    "2048b82222222222222222ffd04883c42084c00f852b010000488b8790000000488b4010493b45307419488b4718493b"
    "4538740f49bb1111111111111111f049ff43084889fa4889f14883ec2048b83333333333333333ffd04883c4204d8b45"
    "104c8b8f900000004d8b491049ba1111111111111111410fb640083d960000000f8784000000c1e0054d8d9402000100"
    "004d390275744d394a08756e48b85555555555555555488b0049394210755b488b88e00000004885c974064c39411874"
    "490fb790d07200006bd20c0fb680d272000001c2413b52187230413b521c732ab89a99993f660f6ec80f2fc1721cb800"
    "00a040660f6ec8f30f59c149bb1111111111111111f049ff4350488b8790000000488b4010493b4530750db8cdcc8c3f"
    "660f6ec8f30f59c10f2fc676260f28f04989ff49bb1111111111111111f049ff4310eb0f49bb1111111111111111f049"
    "ff431848b84444444444444444ffe0");
constexpr std::array PRE_TEMPLATE = native_hex("4883ec6048894c242048895424284c894424304c894c243848894424404c89f948b86666666666666666ffd04989c14d"
    "8b46104d85c00f84990000004d85c90f849000000049ba1111111111111111410fb640083d96000000777ac1e0054d8d"
    "9402000100004d3902756a4d394a08756448b85555555555555555488b00493942107551488b88e00000004885c97406"
    "4c394118743f0fb790d07200006bd20c0fb680d272000001c2413b52187226413b521c732049bb1111111111111111f0"
    "49ff43484883c46049bb999999999999999941ffe3488b4c2420488b5424284c8b4424304c8b4c2438488b4424404883"
    "c460c644242000440fb6cd4d8b879800000049bb777777777777777741ffe3");
constexpr std::array POST_TEMPLATE = native_hex("4883ec604c89f948b86666666666666666ffd04989c14d8b46104d85c00f840a0100004d85c90f84010100004d39c80f"
    "84f800000048b85555555555555555488b004885c00f84e2000000488b88e00000004885c9740a4c3941180f84cc0000"
    "004c894424204c894c242848894424304c89c94c89c248b82222222222222222ffd084c00f85a300000049bb11111111"
    "11111111f049ff4338b96400000031d24531c048b8aaaaaaaaaaaaaaaaffd083f85f73794c8b4424204c8b4c2428410f"
    "b640083d960000007763c1e00549ba11111111111111114d8d940200010000488b4424300fb790d07200006bd20c0fb6"
    "88d272000001ca4d894a08498942104189521883c2034189521c4d890249bb1111111111111111f049ff434031c94c89"
    "f248b8bbbbbbbbbbbbbbbbffd04883c46085ed7408664109bf20010000498b5e1049bb888888888888888841ffe3");
constexpr std::array SURRENDER_TEMPLATE = native_hex("80f9010f852f0100004881ecd0000000488944242048894c242848895424304c894424384c894c24404c895424484c89"
    "5c2450f30f7f442460f30f7f4c2470f30f7f942480000000f30f7f9c2490000000f30f7fa424a0000000f30f7fac24b0"
    "0000004885f67476488b4e204885c9746d48b81111111111111111ffd04885c0745c488b80c00000004885c074500fb6"
    "405e83e00f83f80b724483e80b488d15b20000000fb60402898424c0000000b96400000031d24531c048b82222222222"
    "222222ffd03b8424c00000007210c644242802c64424301966c745a00219488b442420488b4c2428488b5424304c8b44"
    "24384c8b4c24404c8b5424484c8b5c2450f30f6f442460f30f6f4c2470f30f6f942480000000f30f6f9c2490000000f3"
    "0f6fa424a0000000f30f6fac24b00000004881c4d000000048897dc048895dc8488975e8488b4620ff25000000003333"
    "3333333333331e140a0501");
constexpr std::array SAFE_SKIP_TEMPLATE = native_hex("498B5E10FF25000000000000000000000000");
static_assert(ATTACK_TEMPLATE.size() == 399);
static_assert(PRE_TEMPLATE.size() == 271);
static_assert(POST_TEMPLATE.size() == 334);
static_assert(SURRENDER_TEMPLATE.size() == 0x15b);

struct Marker {
    std::uint64_t value = 0;
    std::uintptr_t replacement = 0;
};

bool relocate(std::span<std::byte> code, std::span<const Marker> markers) {
    bool valid = true;

    for (const Marker& marker : markers) {
        valid = (replace_native_marker(code, marker.value, marker.replacement) != 0) && valid;
    }
    return valid;
}

bool prepare_ai_war(AiWarPolicy& state, std::uintptr_t base) {
    state.patches.last_result = platform::windows::CodePatchResult::ExpectedBytesMismatch;

    constexpr std::array BRANCH_ORIGINAL = native_hex("751E");
    constexpr std::array BRANCH_REPLACEMENT = native_hex("9090");
    constexpr std::array SELECT_ORIGINAL = native_hex("0F45FD");
    constexpr std::array SELECT_REPLACEMENT = native_hex("909090");
    constexpr std::array ATTACK_BRANCH_ORIGINAL = native_hex("740D");
    constexpr std::array ATTACK_BRANCH_REPLACEMENT = native_hex("EB0D");
    constexpr std::array RESULT_ORIGINAL = native_hex("B801000000");
    constexpr std::array RESULT_REPLACEMENT = native_hex("B800000000");

    if (!native_bytes_match(base + 0x01464b91, BRANCH_ORIGINAL) ||
        !native_bytes_match(base + 0x01464b83, SELECT_ORIGINAL) ||
        !native_bytes_match(base + 0x0145aee6, ATTACK_BRANCH_ORIGINAL) ||
        !native_bytes_match(base + 0x0145bdab, RESULT_ORIGINAL) ||
        !native_bytes_match(base + 0x0144d248, ATTACK_ORIGINAL) ||
        !native_bytes_match(base + 0x01451ead, PRE_ORIGINAL) ||
        !native_bytes_match(base + 0x01451ecd, POST_ORIGINAL) ||
        !native_bytes_match(base + 0x0193ca30, SURRENDER_ORIGINAL)) {
        return false;
    }

    state.patches.last_result = platform::windows::CodePatchResult::WriteFailed;

    const std::uintptr_t cave = platform::windows::allocate_code_near(base + 0x0144d248, 0x4000);

    if (!cave) {
        return false;
    }

    std::array<std::byte, 0x4000> code{};

    std::copy(ATTACK_TEMPLATE.begin(), ATTACK_TEMPLATE.end(), code.begin());
    std::copy(PRE_TEMPLATE.begin(), PRE_TEMPLATE.end(), code.begin() + 0x400);
    std::copy(POST_TEMPLATE.begin(), POST_TEMPLATE.end(), code.begin() + 0x600);
    std::copy(SAFE_SKIP_TEMPLATE.begin(), SAFE_SKIP_TEMPLATE.end(), code.begin() + 0x800);
    std::copy(SURRENDER_TEMPLATE.begin(), SURRENDER_TEMPLATE.end(), code.begin() + 0x3000);

    const std::array<Marker, 5> attack_markers{{
        {0x1111111111111111ULL, cave + 0x1000}, {0x2222222222222222ULL, base + 0x017af090},
        {0x3333333333333333ULL, base + 0x017a5130}, {0x4444444444444444ULL, base + 0x0144d27a},
        {0x5555555555555555ULL, base + 0x02e98bc8}
    }};
    const std::array<Marker, 5> pre_markers{{
        {0x1111111111111111ULL, cave + 0x1000}, {0x5555555555555555ULL, base + 0x02e98bc8},
        {0x6666666666666666ULL, base + 0x017121d0}, {0x7777777777777777ULL, base + 0x01451ebd},
        {0x9999999999999999ULL, cave + 0x800}
    }};
    const std::array<Marker, 7> post_markers{{
        {0x1111111111111111ULL, cave + 0x1000}, {0x2222222222222222ULL, base + 0x017af090},
        {0x5555555555555555ULL, base + 0x02e98bc8}, {0x6666666666666666ULL, base + 0x017121d0},
        {0xaaaaaaaaaaaaaaaaULL, base + 0x00125aa0}, {0xbbbbbbbbbbbbbbbbULL, base + 0x0144ca10},
        {0x8888888888888888ULL, base + 0x01451edd}
    }};
    const std::array<Marker, 3> surrender_markers{{
        {0x1111111111111111ULL, base + 0x017121d0}, {0x2222222222222222ULL, base + 0x00125aa0},
        {0x3333333333333333ULL, base + 0x0193ca40}
    }};
    NativePatchGroup patches{};
    bool valid = relocate(std::span<std::byte>(code).subspan(0, ATTACK_TEMPLATE.size()), attack_markers);

    valid = relocate(std::span<std::byte>(code).subspan(0x400, PRE_TEMPLATE.size()), pre_markers) && valid;
    valid = relocate(std::span<std::byte>(code).subspan(0x600, POST_TEMPLATE.size()), post_markers) && valid;
    valid = relocate(std::span<std::byte>(code).subspan(0x3000, SURRENDER_TEMPLATE.size()), surrender_markers) && valid;
    // The original safe-skip repair replays MOV RBX,[R14+10] before resuming
    // after the POST hook; jumping to +1451ed9 would land inside that hook.
    valid = put_native_pointer(code, 0x80a, base + 0x01451edd) && valid;
    valid = append_native_patch(patches, base + 0x01464b91, BRANCH_ORIGINAL, BRANCH_REPLACEMENT) && valid;
    valid = append_native_patch(patches, base + 0x01464b83, SELECT_ORIGINAL, SELECT_REPLACEMENT) && valid;
    valid = append_native_patch(patches, base + 0x0145aee6, ATTACK_BRANCH_ORIGINAL, ATTACK_BRANCH_REPLACEMENT) && valid;
    valid = append_native_patch(patches, base + 0x0145bdab, RESULT_ORIGINAL, RESULT_REPLACEMENT) && valid;
    valid = append_native_absolute_branch(patches, base + 0x0144d248, cave, ATTACK_ORIGINAL) && valid;
    valid = append_native_absolute_branch(patches, base + 0x01451ead, cave + 0x400, PRE_ORIGINAL) && valid;
    valid = append_native_absolute_branch(patches, base + 0x01451ecd, cave + 0x600, POST_ORIGINAL) && valid;
    valid = append_native_absolute_branch(patches, base + 0x0193ca30, cave + 0x3000, SURRENDER_ORIGINAL) && valid;
    if (!valid || !platform::windows::write_code_memory(cave, code)) {
        platform::windows::free_code(cave);
        return false;
    }
    state.module_base = base;
    state.cave = cave;
    state.patches = patches;
    return true;
}

} // namespace

bool set_ai_war_policy(AiWarPolicy& state, std::uintptr_t module_base, bool enabled) {
    if (!module_base || (state.module_base && state.module_base != module_base)) {
        state.patches.last_result = platform::windows::CodePatchResult::InvalidInput;
        return false;
    }
    if (enabled && !state.cave && !prepare_ai_war(state, module_base)) {
        return false;
    }
    return set_native_patch_group(state.patches, enabled);
}

} // namespace s8::game
