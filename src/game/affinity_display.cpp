#include "affinity_display.h"
#include "../platform/windows/code_memory.h"
#include "../platform/windows/process_memory.h"

#include <algorithm>
#include <atomic>
#include <cstring>

namespace s8::game {
namespace {

// WOOSEOK99/S8RPKCheats@068ca1d, Officer/AffinityDisplay.h and its effective
// AffinityDisplayVisibilityFix.h wrapper. Native signatures and UI layouts
// come from those files. Captures use stack arguments instead of four globals.
constexpr wchar_t AFFINITY_TITLE[] = L"상성";
constexpr wchar_t QUESTION[] = L"?";
std::atomic<std::uintptr_t> callback_base{0};

using UiInit = std::uintptr_t(__fastcall*)(std::uintptr_t, std::uintptr_t, std::uint32_t, std::uintptr_t);
using UiUpdate = std::uintptr_t(__fastcall*)(std::uintptr_t, std::uint32_t, std::uintptr_t, std::uint32_t);
using AffinityFormat = std::uintptr_t(__fastcall*)(std::uintptr_t);
using AffinitySort = std::uintptr_t(__fastcall*)(std::uintptr_t, std::uintptr_t);
using Visible = std::uint8_t(__fastcall*)(std::uintptr_t);
using NoArgument = std::uintptr_t(__fastcall*)();
using OneArgument = void(__fastcall*)(std::uintptr_t);
using CreateLine = std::uintptr_t(__fastcall*)(std::uintptr_t, std::uint32_t, std::uintptr_t);
using LineLayout = std::uintptr_t(__fastcall*)(std::uintptr_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uintptr_t);
using AttachLine = std::uintptr_t(__fastcall*)(std::uintptr_t, std::uint32_t, std::uintptr_t, std::uint32_t, std::uintptr_t);

struct UiScratch {
    std::array<std::byte, 0x13e0> info{};
    std::array<std::byte, 0xc20> gallery{};
    bool info_active = false;
    bool gallery_active = false;
};

thread_local UiScratch ui_scratch{};

struct ScratchBorrow {
    bool& active;

    ~ScratchBorrow() {
        active = false;
    }
};

template<typename T>
bool read_value(std::uintptr_t address, T& value) {
    return platform::windows::read_memory(address, std::as_writable_bytes(std::span<T>(&value, 1)));
}

void put_u32(std::span<std::byte> bytes, std::size_t offset, std::uint32_t value) {
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

bool visible_officer(std::uintptr_t officer) {
    const std::uintptr_t base = callback_base.load(std::memory_order_acquire);
    std::array<std::byte, 0x18> visibility{};
    std::uint32_t value = 0;

    if (!base || !read_value(base + 0x02c701b8, value)) {
        return false;
    }
    put_u32(visibility, 8, value);
    put_native_pointer(visibility, 0x10, officer);
    return reinterpret_cast<Visible>(base + 0x01b0d5b0)(
        reinterpret_cast<std::uintptr_t>(visibility.data())) != 0;
}

std::uintptr_t __fastcall roster_format(std::uintptr_t officer) {
    if (!visible_officer(officer)) {
        return reinterpret_cast<std::uintptr_t>(QUESTION);
    }
    const std::uintptr_t base = callback_base.load(std::memory_order_acquire);

    return reinterpret_cast<AffinityFormat>(base + 0x01ad9e50)(officer);
}

std::uintptr_t __fastcall roster_sort(std::uintptr_t left, std::uintptr_t right) {
    const std::uintptr_t base = callback_base.load(std::memory_order_acquire);
    const bool visible = visible_officer(right);
    const std::uintptr_t result = reinterpret_cast<AffinitySort>(base + 0x01ad9e10)(left, right);

    if (!visible && result) {
        constexpr std::uint32_t LAST = 0x7fffffff;

        platform::windows::write_memory(result + 4, std::as_bytes(std::span<const std::uint32_t>(&LAST, 1)));
    }
    return result;
}

std::uintptr_t __fastcall info_initialize(std::uintptr_t owner, std::uintptr_t definition,
    std::uint32_t count, std::uintptr_t context) {
    const std::uintptr_t base = callback_base.load(std::memory_order_acquire);
    const UiInit original = reinterpret_cast<UiInit>(base + 0x01d13e60);

    if (count != 0x2f || ui_scratch.info_active) {
        return original(owner, definition, count, context);
    }
    ui_scratch.info_active = true;

    const ScratchBorrow borrow{ui_scratch.info_active};
    std::span<std::byte> buffer(ui_scratch.info);

    std::fill(buffer.begin(), buffer.end(), std::byte{0});
    if (!platform::windows::read_memory(definition, buffer.first(0x11a0)) ||
        !platform::windows::read_memory(definition + 0xc60, buffer.subspan(0x11a0, 0xc0)) ||
        !platform::windows::read_memory(definition + 0xc00, buffer.subspan(0x1260, 0x60))) {
        return original(owner, definition, count, context);
    }
    put_u32(buffer, 0x11a8, 0xa3);
    put_u32(buffer, 0x1208, 0xa3);
    put_u32(buffer, 0x1268, 0x9b);

    const std::uintptr_t result = original(owner, reinterpret_cast<std::uintptr_t>(buffer.data()), 0x32, context);
    const std::uintptr_t root = reinterpret_cast<NoArgument>(base + 0x00014ed0)();
    std::uintptr_t line_owner = 0;
    std::uintptr_t vtable = 0;
    std::uintptr_t create_address = 0;

    if (!root || !read_value(root + 0x118, line_owner) || !line_owner ||
        !read_value(line_owner, vtable) || !vtable || !read_value(vtable + 0x28, create_address) || !create_address) {
        return result;
    }

    struct LineDescription {
        std::uint32_t id;
        std::uint32_t padding;
        std::uintptr_t pointer;
    } description{0x35, 0, 0};
    const CreateLine create = reinterpret_cast<CreateLine>(create_address);
    const std::uintptr_t line = create(line_owner, 0x1a0, reinterpret_cast<std::uintptr_t>(&description));

    if (!line) {
        return result;
    }
    reinterpret_cast<OneArgument>(base + 0x01ede250)(line);

    const std::uintptr_t line_vtable = base + 0x025f7b18;

    if (!platform::windows::write_memory(line, std::as_bytes(std::span<const std::uintptr_t>(&line_vtable, 1)))) {
        return result;
    }
    reinterpret_cast<LineLayout>(base + 0x016977c0)(line, 0x1ec, 0x9b, 0x10c, context);
    reinterpret_cast<AttachLine>(base + 0x01d16aa0)(owner, 0x31, line, 0x14, 1);
    return result;
}

std::uintptr_t __fastcall info_update(std::uintptr_t owner, std::uint32_t field,
    std::uintptr_t value, std::uint32_t mode, std::uintptr_t captured_r13, std::uintptr_t captured_r14) {
    const std::uintptr_t base = callback_base.load(std::memory_order_acquire);
    const UiUpdate original = reinterpret_cast<UiUpdate>(base + 0x01d17190);
    const std::uintptr_t result = original(owner, field, value, mode);
    std::uint32_t count = 0;

    if (!captured_r13 || !read_value(captured_r13 + 0x150, count) || count != 0x32) {
        return result;
    }
    original(captured_r13 + 0x140, 0x2f, reinterpret_cast<std::uintptr_t>(AFFINITY_TITLE), 1);
    original(captured_r13 + 0x140, 0x30, roster_format(captured_r14), 1);
    return result;
}

std::uintptr_t __fastcall gallery_initialize(std::uintptr_t owner, std::uintptr_t definition,
    std::uint32_t count, std::uintptr_t context) {
    const std::uintptr_t base = callback_base.load(std::memory_order_acquire);
    const UiInit original = reinterpret_cast<UiInit>(base + 0x01d13e60);

    if (count != 0x1e || ui_scratch.gallery_active) {
        return original(owner, definition, count, context);
    }
    ui_scratch.gallery_active = true;

    const ScratchBorrow borrow{ui_scratch.gallery_active};
    std::span<std::byte> buffer(ui_scratch.gallery);

    std::fill(buffer.begin(), buffer.end(), std::byte{0});
    if (!platform::windows::read_memory(definition, buffer.first(0xb40)) ||
        !platform::windows::read_memory(definition + 0x660, buffer.subspan(0xb40, 0x60)) ||
        !platform::windows::read_memory(definition + 0x480, buffer.subspan(0xba0, 0x60))) {
        return original(owner, definition, count, context);
    }

    std::uint32_t copied_layout = 0;

    std::memcpy(&copied_layout, buffer.data() + 0xb48, sizeof(copied_layout));
    put_u32(buffer, 0xb44, 0x5bc);
    put_u32(buffer, 0xb4c, 0x64);
    put_u32(buffer, 0xba4, 0x62c);
    put_u32(buffer, 0xba8, copied_layout);
    put_u32(buffer, 0xbac, 0x64);
    return original(owner, reinterpret_cast<std::uintptr_t>(buffer.data()), 0x20, context);
}

std::uintptr_t __fastcall gallery_update(std::uintptr_t owner, std::uint32_t field,
    std::uintptr_t value, std::uint32_t mode, std::uintptr_t captured_rdi, std::uintptr_t captured_r14) {
    const std::uintptr_t base = callback_base.load(std::memory_order_acquire);
    const UiUpdate original = reinterpret_cast<UiUpdate>(base + 0x01d17190);
    const std::uintptr_t result = original(owner, field, value, mode);
    std::uint32_t count = 0;

    if (!captured_rdi || !read_value(captured_rdi + 0x10, count) || count != 0x20) {
        return result;
    }
    original(captured_rdi, 0x1e, reinterpret_cast<std::uintptr_t>(AFFINITY_TITLE), 1);

    const std::uintptr_t affinity = reinterpret_cast<AffinityFormat>(base + 0x01ad9e50)(captured_r14);

    original(captured_rdi, 0x1f, affinity, 1);
    return result;
}

constexpr std::array<std::uint32_t, 14> ARRAY1_OFF{{
    0x00, 0x0C, 0x0D, 0xBF, 0x0E, 0x11, 0x12, 0x13, 0x14, 0x16, 0x17, 0xE7, 0xFFFFFFFF, 0xFFFFFFFF
}};

constexpr std::array<std::uint32_t, 14> ARRAY1_ON{{
    0x00, 0xA8, 0x0C, 0x0D, 0xBF, 0x0E, 0x11, 0x12, 0x13, 0x14, 0x16, 0x17, 0xE7, 0xFFFFFFFF
}};

constexpr std::array<std::uint32_t, 13> ARRAY4_OFF{{
    0xCE, 0x0C, 0x0D, 0xBF, 0x0E, 0x11, 0x12, 0x13, 0x14, 0x16, 0x17, 0xFFFFFFFF, 0xFFFFFFFF
}};

constexpr std::array<std::uint32_t, 13> ARRAY4_ON{{
    0xCE, 0xA8, 0x0C, 0x0D, 0xBF, 0x0E, 0x11, 0x12, 0x13, 0x14, 0x16, 0x17, 0xFFFFFFFF
}};

constexpr std::array<std::uint32_t, 13> ARRAY23_OFF{{
    0x00, 0x23, 0x34, 0x93, 0x9A, 0xD1, 0xD2, 0xD3, 0x14, 0x8B, 0x8C, 0xFFFFFFFF, 0xFFFFFFFF
}};

constexpr std::array<std::uint32_t, 13> ARRAY23_ON{{
    0x00, 0xA8, 0x23, 0x34, 0x93, 0x9A, 0xD1, 0xD2, 0xD3, 0x14, 0x8B, 0x8C, 0xFFFFFFFF
}};

constexpr std::array INFO_INIT_ORIGINAL = native_hex("E8E8120400");

constexpr std::array INFO_UPDATE_ORIGINAL = native_hex("E813660400");

constexpr std::array GALLERY_INIT_ORIGINAL = native_hex("E8C1479900");

constexpr std::array GALLERY_UPDATE_ORIGINAL = native_hex("E8038F9900");

constexpr std::array JUMP_TEMPLATE = native_hex("FF25000000000000000000000000");
// Win64 entry RSP is 8 mod 16. SUB 0x38 supplies aligned shadow space plus
// arguments 5/6 at [rsp+20]/[rsp+28]; CALL adds the return address for our C++
// callback. The original captures are R13/R14 and RDI/R14 respectively.
constexpr std::array INFO_UPDATE_TEMPLATE = native_hex(
    "4883EC384C896C24204C8974242848B80000000000000000FFD04883C438C3");
constexpr std::array GALLERY_UPDATE_TEMPLATE = native_hex(
    "4883EC3848897C24204C8974242848B80000000000000000FFD04883C438C3");
static_assert(INFO_UPDATE_TEMPLATE.size() == 31);
static_assert(GALLERY_UPDATE_TEMPLATE.size() == 31);


bool valid_affinity_context(std::uintptr_t base) {
    std::uintptr_t first = 0;
    std::uintptr_t second = 0;
    std::uintptr_t third = 0;
    std::uint32_t descriptor = 0;
    constexpr std::array TAIL = native_hex("FFFF000001000000");

    return read_value(base + 0x0362aaa8, first) && first == base + 0x03a6c348 &&
        read_value(base + 0x03632c20, second) && second == base + 0x03b36800 &&
        read_value(base + 0x036331a8, third) && third == base + 0x03b3f248 &&
        read_value(base + 0x02c72e70, descriptor) && descriptor == 0xa8 &&
        native_bytes_match(base + 0x02c72eb8, TAIL);
}

bool append_data_chunks(NativePatchGroup& group, std::uintptr_t address,
    std::span<const std::byte> original, std::span<const std::byte> replacement) {
    if (original.size() != replacement.size()) {
        return false;
    }
    for (std::size_t offset = 0; offset < original.size();) {
        const std::size_t size = std::min(original.size() - offset, platform::windows::CODE_PATCH_BYTE_CAPACITY);

        if (!append_native_patch(group, address + offset, original.subspan(offset, size),
            replacement.subspan(offset, size))) {
            return false;
        }
        offset += size;
    }
    return true;
}

template<typename T>
bool append_scalar(NativePatchGroup& group, std::uintptr_t address, const T& original, const T& replacement) {
    return append_native_patch(group, address, std::as_bytes(std::span<const T>(&original, 1)),
        std::as_bytes(std::span<const T>(&replacement, 1)));
}

bool prepare_affinity(AffinityDisplay& state, std::uintptr_t base) {
    state.patches.last_result = platform::windows::CodePatchResult::ExpectedBytesMismatch;
    if (!native_bytes_match(base + 0x01cd2b73, INFO_INIT_ORIGINAL) ||
        !native_bytes_match(base + 0x01cd0b78, INFO_UPDATE_ORIGINAL) ||
        !native_bytes_match(base + 0x0137f69a, GALLERY_INIT_ORIGINAL) ||
        !native_bytes_match(base + 0x0137e288, GALLERY_UPDATE_ORIGINAL)) {
        return false;
    }
    state.patches.last_result = platform::windows::CodePatchResult::WriteFailed;

    const std::uintptr_t cave = platform::windows::allocate_code_near(base + 0x01cd2b73, 0x100);

    if (!cave) {
        return false;
    }

    std::array<std::byte, 0x80> code{};
    NativePatchGroup patches{};

    std::copy(JUMP_TEMPLATE.begin(), JUMP_TEMPLATE.end(), code.begin());
    std::copy(INFO_UPDATE_TEMPLATE.begin(), INFO_UPDATE_TEMPLATE.end(), code.begin() + 0x20);
    std::copy(JUMP_TEMPLATE.begin(), JUMP_TEMPLATE.end(), code.begin() + 0x40);
    std::copy(GALLERY_UPDATE_TEMPLATE.begin(), GALLERY_UPDATE_TEMPLATE.end(), code.begin() + 0x60);

    bool valid = put_native_pointer(code, 6, reinterpret_cast<std::uintptr_t>(&info_initialize));
    const std::uintptr_t old_title = base + 0x02099dd8;
    const std::uintptr_t new_title = reinterpret_cast<std::uintptr_t>(AFFINITY_TITLE);
    const std::uintptr_t old_format = base + 0x01ad9e50;
    const std::uintptr_t new_format = reinterpret_cast<std::uintptr_t>(&roster_format);
    const std::uintptr_t old_sort = base + 0x01ad9e10;
    const std::uintptr_t new_sort = reinterpret_cast<std::uintptr_t>(&roster_sort);

    valid = put_native_pointer(code, 0x30, reinterpret_cast<std::uintptr_t>(&info_update)) && valid;
    valid = put_native_pointer(code, 0x46, reinterpret_cast<std::uintptr_t>(&gallery_initialize)) && valid;
    valid = put_native_pointer(code, 0x70, reinterpret_cast<std::uintptr_t>(&gallery_update)) && valid;
    valid = append_native_branch(patches, base + 0x01cd2b73, cave, INFO_INIT_ORIGINAL, std::byte{0xe8}) && valid;
    valid = append_native_branch(patches, base + 0x01cd0b78, cave + 0x20, INFO_UPDATE_ORIGINAL, std::byte{0xe8}) && valid;
    valid = append_native_branch(patches, base + 0x0137f69a, cave + 0x40, GALLERY_INIT_ORIGINAL, std::byte{0xe8}) && valid;
    valid = append_native_branch(patches, base + 0x0137e288, cave + 0x60, GALLERY_UPDATE_ORIGINAL, std::byte{0xe8}) && valid;
    valid = append_scalar(patches, base + 0x02c72e78, old_title, new_title) && valid;
    valid = append_scalar(patches, base + 0x02c72e80, std::uint32_t{0x68}, std::uint32_t{0x4c}) && valid;
    valid = append_scalar(patches, base + 0x02c72e84, std::uint32_t{1}, std::uint32_t{2}) && valid;
    valid = append_scalar(patches, base + 0x02c72e88, old_format, new_format) && valid;
    valid = append_scalar(patches, base + 0x02c72e98, old_sort, new_sort) && valid;
    valid = append_data_chunks(patches, base + 0x03a6c348, std::as_bytes(std::span(ARRAY1_OFF)),
        std::as_bytes(std::span(ARRAY1_ON))) && valid;
    valid = append_data_chunks(patches, base + 0x03b1bfc8, std::as_bytes(std::span(ARRAY4_OFF)),
        std::as_bytes(std::span(ARRAY4_ON))) && valid;
    valid = append_data_chunks(patches, base + 0x03b36800, std::as_bytes(std::span(ARRAY23_OFF)),
        std::as_bytes(std::span(ARRAY23_ON))) && valid;
    valid = append_data_chunks(patches, base + 0x03b3f248, std::as_bytes(std::span(ARRAY23_OFF)),
        std::as_bytes(std::span(ARRAY23_ON))) && valid;
    if (!valid) {
        platform::windows::free_code(cave);
        return false;
    }
    for (std::size_t index = 0; index < patches.count; ++index) {
        const NativePatch& patch = patches.entries[index];

        if (!native_bytes_match(patch.address, std::span<const std::byte>(patch.original.data(), patch.size))) {
            state.patches.last_result = platform::windows::CodePatchResult::ExpectedBytesMismatch;
            platform::windows::free_code(cave);
            return false;
        }
    }
    if (!platform::windows::write_code_memory(cave, code) ||
        !platform::windows::retain_callback_module(reinterpret_cast<std::uintptr_t>(&roster_format))) {
        platform::windows::free_code(cave);
        return false;
    }

    std::uintptr_t expected_base = 0;

    if (!callback_base.compare_exchange_strong(expected_base, base, std::memory_order_acq_rel) && expected_base != base) {
        platform::windows::free_code(cave);
        return false;
    }
    state.module_base = base;
    state.cave = cave;
    state.patches = patches;
    return true;
}

} // namespace

bool set_affinity_display(AffinityDisplay& state, std::uintptr_t module_base, bool enabled) {
    if (!module_base || (state.module_base && state.module_base != module_base)) {
        state.patches.last_result = platform::windows::CodePatchResult::InvalidInput;
        return false;
    }
    if (enabled && !valid_affinity_context(module_base)) {
        state.patches.last_result = platform::windows::CodePatchResult::ExpectedBytesMismatch;
        return false;
    }
    if (enabled && !state.cave && !prepare_affinity(state, module_base)) {
        return false;
    }
    return set_native_patch_group(state.patches, enabled);
}

} // namespace s8::game
