#include "governor_prisoner_disposal.h"
#include "../platform/windows/code_memory.h"
#include "../platform/windows/process_memory.h"

namespace s8::game {
namespace {

// Instruction blob, signatures and relocations from WOOSEOK99/S8RPKCheats
// 068ca1d881edf870f7cf06f935cf50e286004af8, War/GovernorPrisonerDisposal.cpp.
constexpr std::uintptr_t MAIN_RVA = 0x01e53bb2;
constexpr std::uintptr_t COLLECT_RVA = 0x01e5ce39;
constexpr std::uintptr_t RELATIONS_RVA = 0x01e5de47;
constexpr std::size_t CONSUME_FLAG_OFFSET = 0x3e0;
constexpr std::array MAIN_ORIGINAL = native_hex("4C3BF60F85FC040000");
constexpr std::array COLLECT_ORIGINAL = native_hex("E8C2338EFF");
constexpr std::array RELATIONS_ORIGINAL = native_hex("E8A43094FF");
constexpr std::array QUESTION_SIGNATURE = native_hex("488BC4554154415541564157");
constexpr std::array CONSUME_SIGNATURE = native_hex("48895C2410574883EC");
constexpr std::array CAVE_TEMPLATE = native_hex(
    "4939F60F84F20000005052488B46104885C00F84DC000000807808020F85D2000000488B45E8483946180F85C4000000"
    "488B0500000000488B004885C00F84B10000000FB69030E01E0083FA010F87A10000004869D2A8000000488B841008DF"
    "1E004885C00F8489000000488B80900000004885C0747D488B561848395010757348397020756D488B05000000008038"
    "007418488B0500000000488B004885C0745280B8EA0000000074494D85ED7444488B0500000000488338007437418B55"
    "00488B05000000003B107328488B0500000000488B004885C0741948833CD0007412488B442460E82100000085C07404"
    "5A58EB075A584939F6EB09488B0500000000FFE0488B0500000000FFE0515241504151415241534881ECA8000000F30F"
    "7F442440F30F7F4C2450F30F7F542460F30F7F5C2470F30F7FA42480000000F30F7FAC2490000000C744243000000000"
    "48894424384885C00F84C00000004889C1488B00FF504884C00F84AF000000488B442438488B48104885C90F849D0000"
    "0048894C2438488B01FF504884C00F848A000000488B4424380FB640082C0C3C02767B488D0D00000000488B05000000"
    "008038007407488D0D00000000BAD004000041B8AC0400004531C9C7442420FFFFFF7F48C744242800000000488B0500"
    "000000FFD085C07435488B05000000008038007421488B0500000000488B004885C0741A80B8EA000000007411488B05"
    "00000000FFD0C744243001000000F30F6F442440F30F6F4C2450F30F6F542460F30F6F5C2470F30F6FA42480000000F3"
    "0F6FAC24900000008B4424304881C4A8000000415B415A415941585A59C3E817000000488B0500000000FFE0E8090000"
    "00488B0500000000FFE09C505241524153488B0500000000488B004885C074774C8B90E00000004D85D2746B4D8B5A10"
    "4D85DB746241807B0802755B0FB69030E01E0083FA01774F4869D2A80000004C8B9C1010DF1E004D85DB743B4D395A18"
    "753549398BC0000000752C488B841008DF1E004885C0741F4C8B98900000004D85DB7413498B42184939431075094D39"
    "532075034C89D1415B415A5A589DC3900000000000000000000000000000000000000000000000000000000000000000"
    "000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
    "000000000000000074C788BC200004C82CD258C72000ECD35CB87CB92000C1C911C8200098CC84BD58D5DCC2A0ACB5C2"
    "C8B24CAE3F000000B9D28CAD200031001CAC7CB920008CC144BE58D5ECC5200074C788BC200004C82CD258C72000ECD3"
    "5CB87CB92000C1C911C8200098CC84BD58D5DCC2A0ACB5C2C8B24CAE3F000000");

struct Relocation {
    std::size_t field = 0;
    std::size_t target = 0;
};

constexpr std::array<Relocation, 18> LOCAL_RELOCATIONS{{
    {0x033, 0x310}, {0x08a, 0x360}, {0x096, 0x310}, {0x0b3, 0x318},
    {0x0c4, 0x320}, {0x0cf, 0x328}, {0x0fe, 0x340}, {0x107, 0x348},
    {0x1a6, 0x368}, {0x1ad, 0x360}, {0x1b9, 0x398}, {0x1df, 0x330},
    {0x1ec, 0x360}, {0x1f8, 0x310}, {0x210, 0x338}, {0x266, 0x350},
    {0x274, 0x358}, {0x284, 0x310}
}};

struct AddressRelocation {
    std::size_t field = 0;
    std::uintptr_t rva = 0;
};

constexpr std::array<AddressRelocation, 10> ADDRESS_RELOCATIONS{{
    {0x310, 0x02e98bc8}, {0x318, 0x034c8628}, {0x320, 0x034c8660},
    {0x328, 0x034c8630}, {0x330, 0x00016710}, {0x338, 0x017275e0},
    {0x340, 0x01e53bbb}, {0x348, 0x01e540b7}, {0x350, 0x01740200},
    {0x358, 0x017a0ef0}
}};

bool prepare_governor_disposal(GovernorPrisonerDisposal& state, std::uintptr_t module_base) {
    state.patches.last_result = platform::windows::CodePatchResult::ExpectedBytesMismatch;

    if (!native_bytes_match(module_base + MAIN_RVA, MAIN_ORIGINAL) ||
        !native_bytes_match(module_base + COLLECT_RVA, COLLECT_ORIGINAL) ||
        !native_bytes_match(module_base + RELATIONS_RVA, RELATIONS_ORIGINAL) ||
        !native_bytes_match(module_base + 0x00016710, QUESTION_SIGNATURE) ||
        !native_bytes_match(module_base + 0x017275e0, CONSUME_SIGNATURE)) {
        return false;
    }

    state.patches.last_result = platform::windows::CodePatchResult::WriteFailed;

    const std::uintptr_t cave = platform::windows::allocate_code_near(module_base + MAIN_RVA, 0x1000);

    if (!cave) {
        return false;
    }

    std::array<std::byte, CAVE_TEMPLATE.size()> code = CAVE_TEMPLATE;
    bool valid = true;

    for (const Relocation& relocation : LOCAL_RELOCATIONS) {
        valid = put_native_rel32(code, relocation.field, cave + relocation.field,
            cave + relocation.target) && valid;
    }
    for (const AddressRelocation& relocation : ADDRESS_RELOCATIONS) {
        valid = put_native_pointer(code, relocation.field, module_base + relocation.rva) && valid;
    }
    valid = put_native_pointer(code, 0x360, cave + CONSUME_FLAG_OFFSET) && valid;

    NativePatchGroup patches{};

    valid = append_native_branch(patches, module_base + MAIN_RVA, cave, MAIN_ORIGINAL) && valid;
    valid = append_native_branch(patches, module_base + COLLECT_RVA, cave + 0x25e,
        COLLECT_ORIGINAL, std::byte{0xe8}) && valid;
    valid = append_native_branch(patches, module_base + RELATIONS_RVA, cave + 0x26c,
        RELATIONS_ORIGINAL, std::byte{0xe8}) && valid;
    if (!valid || !platform::windows::write_code_memory(cave, code)) {
        // The cave has not been published in a game instruction yet.
        platform::windows::free_code(cave);
        return false;
    }
    state.module_base = module_base;
    state.cave = cave;
    state.patches = patches;
    return true;
}

} // namespace

bool set_governor_prisoner_disposal(GovernorPrisonerDisposal& state,
    std::uintptr_t module_base, bool enabled, bool consume_privilege) {
    if (!module_base || (state.module_base && state.module_base != module_base)) {
        state.patches.last_result = platform::windows::CodePatchResult::InvalidInput;
        return false;
    }
    if (!enabled) {
        return set_native_patch_group(state.patches, false);
    }
    if (!state.cave && !prepare_governor_disposal(state, module_base)) {
        return false;
    }

    const std::array flag{consume_privilege ? std::byte{1} : std::byte{0}};
    const std::array previous{state.consume_privilege ? std::byte{1} : std::byte{0}};

    // The flag shares an executable cave page; changing that page to RW would
    // temporarily remove execute permission from an in-flight instruction.
    if (!platform::windows::write_memory(state.cave + CONSUME_FLAG_OFFSET, flag)) {
        state.patches.last_result = platform::windows::CodePatchResult::WriteFailed;
        return false;
    }
    if (!set_native_patch_group(state.patches, true)) {
        if (!platform::windows::write_memory(state.cave + CONSUME_FLAG_OFFSET, previous)) {
            state.patches.uncertain = true;
            state.patches.last_result = platform::windows::CodePatchResult::RollbackFailed;
        }
        return false;
    }
    state.consume_privilege = consume_privilege;
    return true;
}

} // namespace s8::game
