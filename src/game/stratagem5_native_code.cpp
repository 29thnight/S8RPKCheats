#include "stratagem5_native_code.h"
#include "../platform/windows/code_memory.h"
#include "../platform/windows/process_memory.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <algorithm>
#include <cstring>
#include <limits>
#include <optional>

namespace s8::game::stratagem5_native_detail {
namespace {

// Instruction sites and the UI ABI are from the effective original
// StratagemSlotProbe.cpp at 068ca1d. No diagnostic probe is installed.
constexpr std::uintptr_t LAYOUT_INIT = 0x01daf350;
constexpr std::uintptr_t MAKER_INIT = 0x01d13e60;
constexpr std::uintptr_t DIALOG_INIT = 0x01df3f20;
constexpr std::uintptr_t DIALOG_OPEN = 0x01df3cb0;
constexpr std::uintptr_t GET_BUTTON = 0x01daf060;
constexpr std::uintptr_t RESET_BUTTONS = 0x01dae9f0;
constexpr std::size_t CAVE_SIZE = 512;
static_assert(static_cast<std::uint8_t>(Admission::Open) == 1);

struct Emitter {
    std::array<std::byte, CAVE_SIZE> bytes{};
    std::size_t size = 0;
    std::uintptr_t address = 0;
    bool valid = true;

    void append(std::span<const std::byte> input) {
        if (!valid || input.size() > bytes.size() - size) {
            valid = false;
            return;
        }
        std::copy(input.begin(), input.end(), bytes.begin() + size);
        size += input.size();
    }

    void byte(std::uint8_t value) {
        const std::byte input = static_cast<std::byte>(value);

        append(std::span(&input, 1));
    }

    void u32(std::uint32_t value) {
        append(std::as_bytes(std::span(&value, 1)));
    }

    void pointer(std::uintptr_t value) {
        append(std::as_bytes(std::span(&value, 1)));
    }

    std::size_t branch(std::uint8_t condition) {
        byte(0x0f);
        byte(condition);

        const std::size_t field = size;

        u32(0);
        return field;
    }

    void bind(std::size_t field) {
        valid = valid && put_native_rel32(bytes, field, address + field, address + size);
    }

    void jump(std::uintptr_t target, std::uint8_t opcode = 0xe9) {
        byte(opcode);

        const std::size_t field = size;

        u32(0);
        valid = valid && put_native_rel32(bytes, field, address + field, target);
    }

    bool write() const {
        return valid && size && platform::windows::write_code_memory(address,
            std::span<const std::byte>(bytes.data(), size));
    }
};

template<typename T>
bool image_value(std::span<const std::byte> image, std::size_t offset, T& result) {
    if (offset > image.size() || sizeof(T) > image.size() - offset) {
        return false;
    }
    std::memcpy(&result, image.data() + offset, sizeof(T));
    return true;
}

bool image_matches(std::span<const std::byte> image, std::size_t offset,
    std::span<const std::byte> expected) {
    return offset <= image.size() && expected.size() <= image.size() - offset &&
        std::equal(expected.begin(), expected.end(), image.begin() + offset);
}

std::optional<std::size_t> unique_bytes(std::span<const std::byte> image,
    std::span<const std::byte> expected, std::size_t begin, std::size_t end) {
    std::optional<std::size_t> result;

    end = std::min(end, image.size());
    if (begin > end || expected.empty() || expected.size() > end - begin) {
        return std::nullopt;
    }
    for (std::size_t offset = begin; offset <= end - expected.size(); ++offset) {
        if (image_matches(image, offset, expected)) {
            if (result) {
                return std::nullopt;
            }
            result = offset;
        }
    }
    return result;
}

void save_volatile(Emitter& code, bool flags = true, std::uint32_t frame = 0x80) {
    if (flags) {
        code.byte(0x9c);
    }
    code.append(native_hex("50515241504151415241534881EC"));
    code.u32(frame);
    for (std::uint8_t index = 0; index < 6; ++index) {
        code.append(native_hex("F30F7F"));
        code.byte(static_cast<std::uint8_t>(0x44 + index * 8));
        code.byte(0x24);
        code.byte(static_cast<std::uint8_t>(0x20 + index * 0x10));
    }
}

void restore_volatile(Emitter& code, bool flags = true, std::uint32_t frame = 0x80) {
    for (std::uint8_t index = 0; index < 6; ++index) {
        code.append(native_hex("F30F6F"));
        code.byte(static_cast<std::uint8_t>(0x44 + index * 8));
        code.byte(0x24);
        code.byte(static_cast<std::uint8_t>(0x20 + index * 0x10));
    }
    code.append(native_hex("4881C4"));
    code.u32(frame);
    code.append(native_hex("415B415A415941585A5958"));
    if (flags) {
        code.byte(0x9d);
    }
}

void call_saved(Emitter& code, std::uintptr_t callback,
    std::span<const std::byte> argument, bool flags = true, std::uint32_t frame = 0x80) {
    // These are in-function sites with the original RSP aligned before CALL.
    // Eight pushes (including flags) + 0x80 preserve alignment, 0x20 shadow
    // bytes, and XMM0..5. Nested native callees retain every live volatile value.
    save_volatile(code, flags, frame);
    code.append(argument);
    code.append(native_hex("48B8"));
    code.pointer(callback);
    code.append(native_hex("FFD0"));
    restore_volatile(code, flags, frame);
}

bool finish_branch(Code& state, std::size_t slot, Emitter& code,
    std::uintptr_t site, std::span<const std::byte> expected, std::byte opcode = std::byte{0xe9}) {
    if (!code.write() || !append_native_branch(state.patches, site, state.caves[slot], expected, opcode)) {
        state.patches.last_result = platform::windows::CodePatchResult::WriteFailed;
        return false;
    }
    return true;
}

bool prepare_init(Code& state, std::span<const std::byte> image, const EntryPoints& entries) {
    std::optional<std::size_t> site;
    constexpr std::array COUNT = native_hex("41B807000000");

    for (std::size_t offset = LAYOUT_INIT + 0xb20; offset + 5 <= LAYOUT_INIT + 0xb80; ++offset) {
        std::int32_t relative = 0;

        if (offset >= image.size() || image[offset] != std::byte{0xe8} ||
            !image_value(image, offset + 1, relative) ||
            static_cast<std::intptr_t>(offset + 5) + relative != MAKER_INIT) {
            continue;
        }
        if (site || !unique_bytes(image, COUNT, offset - 24, offset)) {
            return false;
        }
        site = offset;
    }
    if (!site) {
        return false;
    }
    Emitter code{{}, 0, state.caves[0]};

    // The patched CALL supplies the game's return address. Tail jump preserves
    // its shadow space and stack arguments, so this bridge needs no fake frame.
    code.append(native_hex("48B8"));
    code.pointer(entries.init_layouts);
    code.append(native_hex("FFE0"));
    state.original_init_layouts = state.module + MAKER_INIT;
    return finish_branch(state, 0, code, state.module + *site, image.subspan(*site, 5), std::byte{0xe8});
}

bool prepare_events(Code& state, const EntryPoints& entries) {
    constexpr std::array POST = native_hex("E88A4E26FE");
    constexpr std::array PRE = native_hex("418BFC488D442430");
    constexpr std::array RESET = native_hex("488B5C2430");
    const std::uintptr_t post = state.module + LAYOUT_INIT + 0xcf1;
    const std::uintptr_t pre = state.module + DIALOG_INIT + 0x1c4;
    const std::uintptr_t reset = state.module + RESET_BUTTONS + 0x1f2;
    std::int32_t original_relative = 0;

    std::memcpy(&original_relative, POST.data() + 1, sizeof(original_relative));
    Emitter post_code{{}, 0, state.caves[1]};

    // Keep the original CALL return address on the stack, restore its complete
    // arguments, then tail-jump to the displaced callee. Its returns/exceptions
    // reach the original game frame, not an unregistered synthetic cave frame.
    call_saved(post_code, entries.post_buttons, native_hex("488BCE"), true, 0x88);
    post_code.jump(post + 5 + static_cast<std::intptr_t>(original_relative));

    Emitter pre_code{{}, 0, state.caves[2]};

    call_saved(pre_code, entries.before_callbacks, native_hex("488BCE"));
    pre_code.append(PRE);
    pre_code.jump(pre + PRE.size());

    Emitter reset_code{{}, 0, state.caves[3]};

    call_saved(reset_code, entries.after_reset, native_hex("488D8E00FEFFFF"));
    reset_code.append(RESET);
    reset_code.jump(reset + RESET.size());
    return finish_branch(state, 1, post_code, post, POST, std::byte{0xe8}) &&
        finish_branch(state, 2, pre_code, pre, PRE) &&
        finish_branch(state, 3, reset_code, reset, RESET);
}

bool prepare_callback_loop(Code& state, const EntryPoints& entries) {
    constexpr std::array LOAD = native_hex("488B4608498B1C06");
    constexpr std::array BOUND = native_hex("83FF04");
    constexpr std::array EXPANDED = native_hex("83FF05");
    const std::uintptr_t site = state.module + DIALOG_INIT + 0x203;
    const std::uintptr_t next = site + LOAD.size();
    const std::uintptr_t exit = state.module + DIALOG_INIT + 0x36b;
    Emitter code{{}, 0, state.caves[4]};

    code.append(native_hex("9C85FF"));
    const std::size_t skip_prepare = code.branch(0x85);

    call_saved(code, entries.before_callbacks, native_hex("488BCE"), false);
    code.bind(skip_prepare);
    code.append(native_hex("83FF04"));
    const std::size_t fifth = code.branch(0x84);

    code.byte(0x9d);
    code.append(LOAD);
    code.jump(next);
    code.bind(fifth);
    code.append(native_hex("488B4608"));
    code.append(native_hex("48BB"));
    code.pointer(entries.admission);
    code.append(native_hex("803B01"));
    const std::size_t closed = code.branch(0x85);

    code.append(native_hex("48BB"));
    code.pointer(entries.active_layout);
    code.append(native_hex("483B03"));
    const std::size_t wrong_layout = code.branch(0x85);

    code.append(native_hex("48BB"));
    code.pointer(entries.active_button);
    code.append(native_hex("488B1B4885DB"));
    const std::size_t missing_button = code.branch(0x84);

    code.byte(0x9d);
    code.jump(next);
    code.bind(wrong_layout);
    code.bind(missing_button);
    code.bind(closed);
    code.append(native_hex("4883C408"));
    code.jump(exit);
    return finish_branch(state, 4, code, site, LOAD) &&
        append_native_patch(state.bounds, state.module + DIALOG_INIT + 0x35b, BOUND, EXPANDED);
}

bool prepare_get_button(Code& state, const EntryPoints& entries) {
    constexpr std::array EXPECTED = native_hex("83FA04730B8BC2488B84C1E0010000C333C0C3");
    Emitter code{{}, 0, state.caves[5]};

    code.append(native_hex("83FA04"));
    const std::size_t fifth = code.branch(0x84);
    const std::size_t outside = code.branch(0x87);

    code.append(native_hex("8BC2488B84C1E0010000C3"));
    code.bind(fifth);
    code.append(native_hex("48B8"));
    code.pointer(entries.admission);
    code.append(native_hex("803801"));
    const std::size_t closed = code.branch(0x85);

    code.append(native_hex("48B8"));
    code.pointer(entries.active_layout);
    code.append(native_hex("488B004839C1"));
    const std::size_t wrong_layout = code.branch(0x85);

    code.append(native_hex("48B8"));
    code.pointer(entries.active_button);
    code.append(native_hex("488B00C3"));
    code.bind(outside);
    code.bind(wrong_layout);
    code.bind(closed);
    code.append(native_hex("33C0C3"));
    return native_bytes_match(state.module + GET_BUTTON, EXPECTED) &&
        finish_branch(state, 5, code, state.module + GET_BUTTON, std::span(EXPECTED).first(5));
}

bool prepare_open(Code& state, std::span<const std::byte> image, const EntryPoints& entries) {
    constexpr std::array LOAD = native_hex("488BB4F0E0010000");
    constexpr std::array BOUND = native_hex("83FF04");
    constexpr std::array EXPANDED = native_hex("83FF05");
    constexpr std::size_t END = DIALOG_OPEN + 0x241;
    std::optional<std::size_t> compare;
    std::size_t load = 0;
    std::uintptr_t skip = 0;

    for (std::size_t offset = DIALOG_OPEN; offset + 9 < END; ++offset) {
        if (!image_matches(image, offset, BOUND)) {
            continue;
        }
        std::size_t branch_end = 0;
        std::intptr_t target = 0;

        if (image_matches(image, offset + 3, native_hex("73"))) {
            std::int8_t relative = 0;

            if (!image_value(image, offset + 4, relative)) {
                continue;
            }
            branch_end = offset + 5;
            target = static_cast<std::intptr_t>(branch_end) + relative;
        } else if (image_matches(image, offset + 3, native_hex("0F83"))) {
            std::int32_t relative = 0;

            if (!image_value(image, offset + 5, relative)) {
                continue;
            }
            branch_end = offset + 9;
            target = static_cast<std::intptr_t>(branch_end) + relative;
        } else {
            continue;
        }
        const std::optional<std::size_t> candidate = unique_bytes(image, LOAD, branch_end,
            std::min(branch_end + 0x70, END));

        if (!candidate || target < DIALOG_OPEN || target >= END) {
            continue;
        }
        if (compare) {
            return false;
        }
        compare = offset;
        load = *candidate;
        skip = state.module + static_cast<std::uintptr_t>(target);
    }
    if (!compare) {
        return false;
    }
    Emitter code{{}, 0, state.caves[6]};

    code.append(native_hex("9C85FF"));
    const std::size_t skip_prepare = code.branch(0x85);

    call_saved(code, entries.before_open_button, native_hex("488BC8"), false);
    code.bind(skip_prepare);
    code.append(native_hex("83FF04"));
    const std::size_t original = code.branch(0x85);

    code.append(native_hex("48BE"));
    code.pointer(entries.admission);
    code.append(native_hex("803E01"));
    const std::size_t closed = code.branch(0x85);

    code.append(native_hex("48BE"));
    code.pointer(entries.active_layout);
    code.append(native_hex("483B06"));
    const std::size_t wrong_layout = code.branch(0x85);

    code.append(native_hex("48BE"));
    code.pointer(entries.active_button);
    code.append(native_hex("488B364885F6"));
    const std::size_t missing = code.branch(0x84);

    code.byte(0x9d);
    code.jump(state.module + load + LOAD.size());
    code.bind(original);
    code.byte(0x9d);
    code.append(LOAD);
    code.jump(state.module + load + LOAD.size());
    code.bind(wrong_layout);
    code.bind(missing);
    code.bind(closed);
    code.append(native_hex("4883C40883FF04"));
    code.jump(skip);
    return finish_branch(state, 6, code, state.module + load, LOAD) &&
        append_native_patch(state.bounds, state.module + *compare, BOUND, EXPANDED);
}

bool prepare_capture(Code& state, std::span<const std::byte> image, const EntryPoints& entries) {
    constexpr std::array GETTER = native_hex("488B4108488B4830488B010FB64018C3");
    constexpr std::array EXPECTED = native_hex("0FB64018C3");
    const std::byte* const end = image.data() + image.size();
    const std::byte* const match = std::search(image.data(), end, GETTER.begin(), GETTER.end());

    if (match == end) {
        return false;
    }
    // This getter uses the original's first exact match. UI call-site windows
    // are independently required to be unique by their original contracts.
    const std::size_t getter = static_cast<std::size_t>(match - image.data());
    Emitter code{{}, 0, state.caves[7]};

    code.append(native_hex("9C5280781800"));
    const std::size_t defense = code.branch(0x85);

    code.append(native_hex("48BA"));
    code.pointer(entries.attack_owner);
    code.append(native_hex("488902"));
    code.byte(0xe9);
    const std::size_t attack_done = code.size;

    code.u32(0);
    code.bind(defense);
    code.append(native_hex("80781801"));
    const std::size_t other = code.branch(0x85);

    code.append(native_hex("48BA"));
    code.pointer(entries.defense_owner);
    code.append(native_hex("488902"));
    code.bind(other);
    code.bind(attack_done);
    code.append(native_hex("5A9D"));
    code.append(EXPECTED);
    return finish_branch(state, 7, code, state.module + getter + 0x0b, EXPECTED);
}

bool prepare_selection_bound(Code& state, std::span<const std::byte> image) {
    constexpr std::size_t START = 0x01df37b0;
    constexpr std::size_t END = START + 0x4c;
    constexpr std::array DIRECT_ARRAY = native_hex("E0010000");
    unsigned direct_arrays = 0;
    unsigned getter_calls = 0;
    unsigned bounds = 0;
    std::size_t bound = 0;

    // The original supported build does not require a selection patch. Keep
    // the original's narrow optional shape: one getter call, one cmp4, and no
    // direct four-entry array access. The old runtime selection hook only logs.
    for (std::size_t offset = START; offset < END; ++offset) {
        if (image_matches(image, offset, DIRECT_ARRAY)) {
            ++direct_arrays;
        }
        if (offset + 3 <= END && image[offset] == std::byte{0x83} &&
            (std::to_integer<unsigned>(image[offset + 1]) & 0x38) == 0x38 &&
            image[offset + 2] == std::byte{0x04}) {
            ++bounds;
            bound = offset + 2;
        }
        std::int32_t relative = 0;

        if (offset + 5 <= END && image[offset] == std::byte{0xe8} &&
            image_value(image, offset + 1, relative) &&
            static_cast<std::intptr_t>(offset + 5) + relative == GET_BUTTON) {
            ++getter_calls;
        }
    }
    return direct_arrays != 0 || getter_calls != 1 || bounds != 1 ||
        append_native_patch(state.bounds, state.module + bound, native_hex("04"), native_hex("05"));
}

} // namespace

bool supported_image(std::span<const std::byte> image) {
    IMAGE_DOS_HEADER dos{};
    IMAGE_NT_HEADERS64 nt{};

    if (!image_value(image, 0, dos) || dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew <= 0 ||
        !image_value(image, static_cast<std::size_t>(dos.e_lfanew), nt) ||
        nt.Signature != IMAGE_NT_SIGNATURE || nt.FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 ||
        nt.FileHeader.TimeDateStamp != 0x69a67ed1 || nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
        nt.OptionalHeader.SizeOfImage != image.size() ||
        nt.OptionalHeader.NumberOfRvaAndSizes <= IMAGE_DIRECTORY_ENTRY_DEBUG ||
        image.size() <= 0x01e7ff40 + 0x100) {
        return false;
    }
    const IMAGE_DATA_DIRECTORY debug = nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG];

    if (!debug.VirtualAddress || !debug.Size || debug.Size % sizeof(IMAGE_DEBUG_DIRECTORY) ||
        debug.Size / sizeof(IMAGE_DEBUG_DIRECTORY) > 64 || debug.VirtualAddress >= image.size() ||
        debug.Size > image.size() - debug.VirtualAddress) {
        return false;
    }
    struct CodeViewId {
        DWORD signature;
        GUID guid;
        DWORD age;
    };
    static_assert(sizeof(CodeViewId) == 24);
    constexpr GUID EXPECTED{0xb18a027e, 0x19f4, 0x4c35, {0x87, 0x49, 0x5b, 0x6f, 0x0f, 0xf8, 0x14, 0xd8}};

    for (DWORD offset = 0; offset < debug.Size; offset += sizeof(IMAGE_DEBUG_DIRECTORY)) {
        IMAGE_DEBUG_DIRECTORY entry{};
        CodeViewId id{};

        if (!image_value(image, debug.VirtualAddress + offset, entry)) {
            return false;
        }
        if (entry.Type != IMAGE_DEBUG_TYPE_CODEVIEW) {
            continue;
        }
        if (!entry.AddressOfRawData || entry.SizeOfData < sizeof(id) ||
            entry.AddressOfRawData >= image.size() || entry.SizeOfData > image.size() - entry.AddressOfRawData ||
            !image_value(image, entry.AddressOfRawData, id)) {
            return false;
        }
        if (id.signature == 0x53445352 && id.age == 1 &&
            std::memcmp(&id.guid, &EXPECTED, sizeof(EXPECTED)) == 0) {
            return true;
        }
    }
    return false;
}

void discard_unpublished_code(Code& state) {
    if (state.published || state.patches.applied || state.patches.uncertain || state.bounds.applied || state.bounds.uncertain) {
        return;
    }
    const platform::windows::CodePatchResult result = state.patches.last_result;

    for (std::uintptr_t cave : state.caves) {
        if (cave) {
            platform::windows::free_code(cave);
        }
    }
    state = {};
    state.patches.last_result = result;
}

bool prepare_code(Code& state, std::uintptr_t module_base,
    std::span<const std::byte> image, const EntryPoints& entries) {
    if (state.module == module_base && state.patches.count) {
        return true;
    }
    if (!module_base || !supported_image(image) || state.published || state.patches.applied || state.patches.uncertain) {
        state.patches.last_result = platform::windows::CodePatchResult::ExpectedBytesMismatch;
        return false;
    }
    state.module = module_base;
    state.patches.last_result = platform::windows::CodePatchResult::WriteFailed;

    // All caves are allocated before either all-or-rollback patch transaction.
    // No executable page is allocated or patched from a native UI callback.
    constexpr std::array<std::uintptr_t, 8> NEAR{
        LAYOUT_INIT + 0xb20, LAYOUT_INIT + 0xcf1, DIALOG_INIT + 0x1c4,
        RESET_BUTTONS + 0x1f2, DIALOG_INIT + 0x203, GET_BUTTON, DIALOG_OPEN, 0x01000000
    };

    for (std::size_t index = 0; index < state.caves.size(); ++index) {
        state.caves[index] = platform::windows::allocate_code_near(module_base + NEAR[index], CAVE_SIZE);
        if (!state.caves[index]) {
            discard_unpublished_code(state);
            return false;
        }
    }
    if (!platform::windows::retain_callback_module(entries.init_layouts)) {
        discard_unpublished_code(state);
        return false;
    }
    if (!native_bytes_match(module_base + 0x01e7fec0, native_hex("48894C240853")) ||
        !native_bytes_match(module_base + 0x01e7ff40, native_hex("48895C241048")) ||
        !native_bytes_match(module_base + 0x01e7fd90, native_hex("48895C240857"))) {
        state.patches.last_result = platform::windows::CodePatchResult::ExpectedBytesMismatch;
        discard_unpublished_code(state);
        return false;
    }
    state.patches.last_result = platform::windows::CodePatchResult::ExpectedBytesMismatch;
    if (!prepare_init(state, image, entries) || !prepare_events(state, entries) ||
        !prepare_callback_loop(state, entries) || !prepare_get_button(state, entries) ||
        !prepare_open(state, image, entries) || !prepare_capture(state, image, entries) ||
        !prepare_selection_bound(state, image)) {
        discard_unpublished_code(state);
        return false;
    }
    return true;
}

} // namespace s8::game::stratagem5_native_detail
