#include "prisoner_capture.h"
#include "../platform/windows/code_memory.h"
#include "../platform/windows/process_memory.h"

#include <algorithm>

namespace s8::game {
namespace {

// Provenance: WOOSEOK99/S8RPKCheats@068ca1d881edf870f7cf06f935cf50e286004af8,
// War/DeputyCaptureFix.cpp, IsolatedCityCaptureFix.cpp,
// AIExecutionConditionChange.cpp and PrisonerCaptureManagement.cpp.
constexpr std::array DEPUTY_ORIGINAL = native_hex("4D89A610030000B001E9A2FEFFFF");
constexpr std::array CITY_ORIGINAL = native_hex("48894C2408555356574154415541564157");
constexpr std::array PRISON_ORIGINAL = native_hex("3BCD488B6C24600F9CC0E905FEFFFFCC");
constexpr std::array CLASSIFIER_ORIGINAL = native_hex("C7442444010100004C8D442444488BD3");
constexpr std::array DEPUTY_TEMPLATE = native_hex("4d89a6100300009c50515241504151415241534883ec204c89f14c89e2e84f0000004883c420415b415a415941585a59"
    "589db001ff25ce01000031d24885ff742d488d8648856802483b077521488b47104885c074184881c6287d6802483b30"
    "750c8a400831d2ffc83c060f96c289d0c3f30f1efa41564989c84155415455575653488b1d6f010000488b2d70010000"
    "488b8bc88be9024885c90f844b0100004889de4c89c74989d1e88cffffff85c00f84350100004889de4c89cfe879ffff"
    "ff85c00f84220100004d39c80f84190100004d3988100300000f850c0100004d8b60184d85e40f84ff000000498b4118"
    "488d93988c6802493914240f85ea0000004885c00f84e10000004839100f85d80000004939c40f84cf0000004c8d9908"
    "df1e00488db958e01e0041be700000004d8d53b8498b0a4d8b6a084939cd0f828a0000004c89ee31d24829ce4889f049"
    "f7f64885d275774881fe00380000776e4c39e973694c394108755d488b41304885c0745480781c00754e48ff45004c8d"
    "59104883c1204d8b134d39d0742f4d39d1742a4889de4c89d7e89cfeffff85c0741b4d39621875154983ba1003000000"
    "750b4d898a1003000048ff45084983c3084c39d975c0eb234883c170eb924983c2184d39d30f8559ffffff4981c3a800"
    "00004c39df0f8545ffffff5b5e5f5d415c415d415ec36690111111111111111122222222222222223333333333333333");
constexpr std::array CITY_TEMPLATE = native_hex("f30f1efa4157415641554154555756534881ec180100004c8b2db2040000488b1dc304000048894c24284d8ba5c88be9"
    "020f297424700f29bc2480000000440f29842490000000440f298c24a0000000440f299424b0000000440f299c24c000"
    "0000440f29a424d0000000440f29ac24e0000000440f29b424f0000000440f29bc24000100004d85e40f847203000048"
    "85c90f84690300004180bc2430e01e00000f855a0300004d8bb424b0df1e004d8bbc24b8df1e004d8b8c2410df1e004d"
    "85f60f8439030000498d8d38966802493b0e0f85290300004d85ff0f8420030000498d85988c68024939070f85100300"
    "004d85c90f84070300004939010f85fe0200004d39cf0f84f502000041f68788020000040f85e7020000498b86900000"
    "004885c00f84d70200004c3b78100f85cd020000498d46204d8d4650488b104885d2741f483b0a0f85b4020000488b92"
    "900000004885d2740a4c3b7a100f849e0200004883c0084939c075d0498b8c24c0de1e004d8b8424c8de1e004939c80f"
    "827c0200004d89c241bb7000000031d24929ca4c89d049f7f34885d20f855f0200004981fa003800000f87520200004c"
    "39c10f834902000080792a0274064883c170ebeb488b410848894424204885c00f842b020000498d8548856802498dbd"
    "488568024889442440488b442420483b380f850a0200004c3948180f8500020000488b40104885c00f84f3010000498d"
    "b5287d68024889742448483b300f85de0100008a4008ffc83c060f87d1010000488b542420488b7424284c89efe83502"
    "000085c00f88b701000031c048ff0348894310410fb6460848894318498d8424f8bd21004889442430498d8424786022"
    "004889442450498d842458e01e004889442458488b442430488b284885ed0f8457010000488b442440483b45000f8548"
    "0100004c397d180f853e0100004c3975200f8534010000488b45104885c00f8427010000488b742448483b300f851901"
    "00008a4008ffc83c060f870c0100004d8d9c2408df1e004531c941b870000000bf010000004d8d53b8498b0a498b7208"
    "4839ce72644889f031d24829c8488944243849f7f04885d2754f48817c24380038000077444839f1734548396908740c"
    "483969107406483969187527488b41304885c0741880781c000fb651280f95c00fb6c031d0ffc8440f45cfeb0641b901"
    "0000004883c170ebbc41b9010000004983c2184d39d375814981c3a80000004c395c24580f856bffffff4585c9755c48"
    "8b7424284889ea4c89efe8d800000085c0782e7532488b052c010000488b4c2428488d5424684c89ef48896c2468ffd0"
    "488b7424284889eae8aa000000ffc8740648ff4320eb2a488b4424204889851003000048ff430848ff43104883442430"
    "08488b7c243048397c24500f8582feffff488b05d8000000488b4c24280f287424700f28bc2480000000440f28842490"
    "000000440f288c24a0000000440f289424b0000000440f289c24c0000000440f28a424d0000000440f28ac24e0000000"
    "440f28b424f0000000440f28bc24000100004881c4180100005b5e5f5d415c415d415e415fffe0488b4608488bb73086"
    "4c034885c0743d4885f674388b0883c8ff3b8f60864c037334488b0cce31f64885c90f95c081fe50140000771184c074"
    "0d4839117412488b4908ffc6ebe10fb6c0f7d8c383c8ffc3b801000000c3669011111111111111112222222222222222"
    "33333333333333334444444444444444");
constexpr std::array PRISON_TEMPLATE = native_hex("49bb1111111111111111f049ff43200fb6475d0fb6565d29d09931d029d0ba9600000029c239d00f4fc241b864000000"
    "4129c00fb6565e83e20f488b47104885c07418807808017424488b47184885c074094839b8c000000074124531c983fa"
    "0b7d056bd203eb176bd205eb124183e81941b90100000083fa0b7c036bd2064101d0807e300175044183c00aba640000"
    "004139d0440f4fc231d24139d0440f4cc24183f864751c4585c974170fb6565e83e20f4101d04183e810807e30017503"
    "41ffc04883ec40894c2420448944242844894c242c48b83333333333333333488b084881c1c87200000fb75608440fb7"
    "470848b84444444444444444ffd00fb6c848b85555555555555555ffd08b4c2420448b442428448b4c242c4883c44045"
    "89ca84c075690fb6565e83e20f83fa0b7c5a4585c9752041b94b00000083fa0f740d83fa0e752e41b955000000eb2641"
    "b95a000000eb1e41b93200000083fa0f740d83fa0e750e41b942000000eb0641b94b0000004489c0410fafc131d241b9"
    "6400000041f7f14189c0eb034531c04585d20f84110100000fb6565e83e20f83fa0f0f84010100004585c00f84f80000"
    "004883ec60894c24204489442424488b4f1848894c24304885c90f84cc000000488b01ff504884c00f84be000000488b"
    "4e1848894c24384885c90f84ac000000483b4c24300f84a1000000488b01ff504884c00f8493000000488b442438488b"
    "54243080b8c201000000741f483990d00100007516488b88c80100004885c9740a488b01ff504884c07532488b442430"
    "488b54243880b8c201000000744e483990d00100007545488b88c80100004885c97439488b01ff504884c0742f8b4424"
    "240fb6565e83e20f83fa0e741983fa0b72106bc05a31d241b96400000041f7f1eb06d1e8eb02ffc8894424248b4c2420"
    "448b4424244883c46049bb11111111111111114439c10f9dc084c07407f049ff4330eb05f049ff4328488b6c246049bb"
    "222222222222222241ffe3");
constexpr std::array CLASSIFIER_TEMPLATE = native_hex("c7442444010100004883ec30c7442420010100004c8d4424204889da4c89f148b86666666666666666ffd084c0746141"
    "0fb6565e83e20f83fa0b7c544c89f14889da48b87777777777777777ffd084c0753e48b83333333333333333488b0848"
    "81c1c8720000410fb75608440fb7430848b84444444444444444ffd00fb6c848b85555555555555555ffd084c00f95c0"
    "4883c43049bb888888888888888841ffe3");
constexpr std::array ABSOLUTE_JUMP = native_hex("FF25000000000000000000000000");
constexpr std::size_t CITY_OFFSET = 0x1000;
constexpr std::size_t EXECUTION_OFFSET = 0x4000;
static_assert(DEPUTY_TEMPLATE.size() == 0x210);
static_assert(CITY_TEMPLATE.size() == 0x4f0);
static_assert(PRISON_TEMPLATE.size() == 731);
static_assert(CLASSIFIER_TEMPLATE.size() == 161);
static_assert(DEPUTY_ORIGINAL.size() == 14);
static_assert(CITY_ORIGINAL.size() == 17);
static_assert(PRISON_ORIGINAL.size() == 16);
static_assert(CLASSIFIER_ORIGINAL.size() == 16);

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

bool prepare_prisoner_capture(PrisonerCapture& state, std::uintptr_t base) {
    state.patches.last_result = platform::windows::CodePatchResult::ExpectedBytesMismatch;

    if (!native_bytes_match(base + 0x01e33056, DEPUTY_ORIGINAL) ||
        !native_bytes_match(base + 0x01e57250, CITY_ORIGINAL) ||
        !native_bytes_match(base + 0x01e52a8d, PRISON_ORIGINAL) ||
        !native_bytes_match(base + 0x01e54190, CLASSIFIER_ORIGINAL)) {
        return false;
    }

    state.patches.last_result = platform::windows::CodePatchResult::WriteFailed;

    const std::uintptr_t cave = platform::windows::allocate_code_near(base + 0x01e33056, 0x6000);

    if (!cave) {
        return false;
    }

    std::array<std::byte, 0x6000> code{};

    std::copy(DEPUTY_TEMPLATE.begin(), DEPUTY_TEMPLATE.end(), code.begin());
    std::fill(code.begin() + CITY_OFFSET, code.begin() + CITY_OFFSET + 0x600, std::byte{0xcc});
    std::copy(CITY_TEMPLATE.begin(), CITY_TEMPLATE.end(), code.begin() + CITY_OFFSET);
    std::copy(CITY_ORIGINAL.begin(), CITY_ORIGINAL.end(), code.begin() + CITY_OFFSET + 0x600);
    std::copy(ABSOLUTE_JUMP.begin(), ABSOLUTE_JUMP.end(),
        code.begin() + CITY_OFFSET + 0x600 + CITY_ORIGINAL.size());
    std::copy(PRISON_TEMPLATE.begin(), PRISON_TEMPLATE.end(), code.begin() + EXECUTION_OFFSET);
    std::copy(CLASSIFIER_TEMPLATE.begin(), CLASSIFIER_TEMPLATE.end(), code.begin() + EXECUTION_OFFSET + 0x800);

    const std::uintptr_t execution_stats = cave + EXECUTION_OFFSET + 0x1000;
    const std::array<Marker, 5> prison_markers{{
        {0x1111111111111111ULL, execution_stats}, {0x2222222222222222ULL, base + 0x01e528a1},
        {0x3333333333333333ULL, base + 0x02e98bc8}, {0x4444444444444444ULL, base + 0x01712670},
        {0x5555555555555555ULL, base + 0x017b0ab0}
    }};
    const std::array<Marker, 6> classifier_markers{{
        {0x3333333333333333ULL, base + 0x02e98bc8}, {0x4444444444444444ULL, base + 0x01712670},
        {0x5555555555555555ULL, base + 0x017b0ab0}, {0x6666666666666666ULL, base + 0x017b4570},
        {0x7777777777777777ULL, base + 0x017a8270}, {0x8888888888888888ULL, base + 0x01e541a8}
    }};
    NativePatchGroup patches{};
    bool valid = put_native_pointer(code, 0x1f8, base);

    valid = put_native_pointer(code, 0x200, cave + 0x300) && valid;
    valid = put_native_pointer(code, 0x208, base + 0x01e32f06) && valid;
    valid = put_native_pointer(code, CITY_OFFSET + 0x4d0, base) && valid;
    valid = put_native_pointer(code, CITY_OFFSET + 0x4d8, base + 0x0001a0c0) && valid;
    valid = put_native_pointer(code, CITY_OFFSET + 0x4e0, cave + CITY_OFFSET + 0x600) && valid;
    valid = put_native_pointer(code, CITY_OFFSET + 0x4e8, cave + CITY_OFFSET + 0x1000) && valid;
    valid = put_native_pointer(code, CITY_OFFSET + 0x600 + CITY_ORIGINAL.size() + 6,
        base + 0x01e57250 + CITY_ORIGINAL.size()) && valid;
    valid = relocate(std::span<std::byte>(code).subspan(EXECUTION_OFFSET, PRISON_TEMPLATE.size()),
        prison_markers) && valid;
    valid = relocate(std::span<std::byte>(code).subspan(EXECUTION_OFFSET + 0x800, CLASSIFIER_TEMPLATE.size()),
        classifier_markers) && valid;
    valid = append_native_absolute_branch(patches, base + 0x01e33056, cave, DEPUTY_ORIGINAL) && valid;
    valid = append_native_absolute_branch(patches, base + 0x01e57250, cave + CITY_OFFSET, CITY_ORIGINAL) && valid;
    valid = append_native_absolute_branch(patches, base + 0x01e52a8d, cave + EXECUTION_OFFSET, PRISON_ORIGINAL) && valid;
    valid = append_native_absolute_branch(patches, base + 0x01e54190, cave + EXECUTION_OFFSET + 0x800,
        CLASSIFIER_ORIGINAL) && valid;
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

bool set_prisoner_capture(PrisonerCapture& state, std::uintptr_t module_base, bool enabled) {
    if (!module_base || (state.module_base && state.module_base != module_base)) {
        state.patches.last_result = platform::windows::CodePatchResult::InvalidInput;
        return false;
    }
    if (enabled && !state.cave && !prepare_prisoner_capture(state, module_base)) {
        return false;
    }
    return set_native_patch_group(state.patches, enabled);
}

} // namespace s8::game
