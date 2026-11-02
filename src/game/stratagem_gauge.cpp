#include "stratagem_gauge.h"
#include "bytecode_buffer.h"
#include "../platform/windows/code_memory.h"
#include "../platform/windows/process_memory.h"

#include <optional>

namespace s8::game {
namespace {

// Game ABI: RBX addresses side+0xf0. Side byte -0xd8 selects the toggle;
// gauge +0x64 is compared with 10000 by the displaced game instruction.
constexpr std::array ORIGINAL = native_hex("817B6410270000");
constexpr std::array PATTERN = native_hex("817B64102700007400B000488B");
constexpr std::size_t ATTACK_FLAG_OFFSET = 144;

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

    BytecodeBuffer<144> code;
    code.emit({0x50, 0x51}); // preserve RAX and RCX
    code.emit({0x0f, 0xb6, 0x83, 0x28, 0xff, 0xff, 0xff, 0x83, 0xf8, 0x01});
    code.emit({0x0f, 0x87});
    const std::size_t unknown_side = code.branch();
    code.emit({0x48, 0xb9});
    code.address(cave + ATTACK_FLAG_OFFSET);
    code.emit({0x80, 0x3c, 0x01, 0, 0x0f, 0x84}); // toggle[side]
    const std::size_t disabled = code.branch();
    code.emit({0xc7, 0x43, 0x64, 0x10, 0x27, 0, 0});
    code.patch(unknown_side);
    code.patch(disabled);
    code.emit({0x59, 0x58});
    code.append(ORIGINAL);
    code.emit({0xff, 0x25, 0, 0, 0, 0});
    code.address(hook + ORIGINAL.size());
    NativePatchGroup patches{};

    if (!code.valid() ||
        !append_native_branch(patches, hook, cave, ORIGINAL) ||
        !platform::windows::write_code_memory(cave, code.bytes())) {
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
