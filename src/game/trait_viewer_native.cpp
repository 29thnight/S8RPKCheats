#include "trait_viewer_native.h"
#include "../platform/windows/code_memory.h"
#include "../platform/windows/process_memory.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <span>

namespace s8::game {
namespace {

enum class ViewerAddress { Module, Cave };
enum class ViewerPatchKind { Bytes, Jump, Call, Pointer };

struct ViewerBytes {
    std::uintptr_t offset;
    std::span<const std::byte> bytes;
};

struct ViewerCopy {
    std::size_t offset;
    std::uintptr_t source;
    std::size_t size;
};

struct ViewerPointerGuard {
    std::uintptr_t offset;
    std::uintptr_t target;
};

struct ViewerRelocation {
    std::size_t offset;
    std::uintptr_t target;
    ViewerAddress base;
    bool pointer;
};

struct ViewerPatch {
    std::uintptr_t offset;
    ViewerPatchKind kind;
    std::span<const std::byte> original;
    std::span<const std::byte> replacement;
    std::uintptr_t target;
    ViewerAddress base;
};

struct ViewerSpec {
    std::uintptr_t anchor;
    std::size_t allocation_size;
    bool has_live_objects;
    std::span<const ViewerBytes> pieces;
    std::span<const ViewerBytes> guards;
    std::span<const ViewerCopy> copies;
    std::span<const ViewerPointerGuard> pointer_guards;
    std::span<const ViewerRelocation> relocations;
    std::span<const ViewerPatch> patches;
};

#include "trait_viewer_data.inc"

constexpr std::size_t IMAGE_CAPACITY = 0x6000;
constexpr std::uintptr_t MAX_MODULE_OFFSET = 0x3C00000;

bool match_image(std::uintptr_t address, std::span<const std::byte> expected) {
    std::array<std::byte, 256> current{};

    for (std::size_t offset = 0; offset < expected.size();) {
        const std::size_t size = std::min(current.size(), expected.size() - offset);

        if (!platform::windows::read_memory(address + offset, std::span(current.data(), size)) ||
            !std::equal(expected.begin() + offset, expected.begin() + offset + size, current.begin())) {
            return false;
        }
        offset += size;
    }
    return true;
}

bool verify_spec(std::uintptr_t module_base, const ViewerSpec& spec) {
    for (const ViewerBytes& guard : spec.guards) {
        if (!match_image(module_base + guard.offset, guard.bytes)) {
            return false;
        }
    }
    for (const ViewerPointerGuard& guard : spec.pointer_guards) {
        std::uintptr_t value = 0;

        if (!platform::windows::read_memory(module_base + guard.offset,
                std::as_writable_bytes(std::span(&value, 1))) || value != module_base + guard.target) {
            return false;
        }
    }
    return true;
}

bool image_range(std::size_t offset, std::size_t size, std::size_t allocation) {
    return offset <= IMAGE_CAPACITY && size <= IMAGE_CAPACITY - offset &&
        offset <= allocation && size <= allocation - offset;
}

bool build_image(TraitViewerBlock& block, const ViewerSpec& spec) {
    std::array<std::byte, IMAGE_CAPACITY> image{};
    std::size_t used = 0;

    for (const ViewerBytes& piece : spec.pieces) {
        if (!image_range(piece.offset, piece.bytes.size(), spec.allocation_size)) {
            return false;
        }
        std::copy(piece.bytes.begin(), piece.bytes.end(), image.begin() + piece.offset);
        used = std::max(used, piece.offset + piece.bytes.size());
    }
    for (const ViewerCopy& copy : spec.copies) {
        if (!image_range(copy.offset, copy.size, spec.allocation_size) ||
            !platform::windows::read_memory(block.module_base + copy.source,
                std::span(image.data() + copy.offset, copy.size))) {
            return false;
        }
        used = std::max(used, copy.offset + copy.size);
    }
    for (const ViewerRelocation& relocation : spec.relocations) {
        const std::size_t size = relocation.pointer ? sizeof(std::uintptr_t) : sizeof(std::int32_t);
        const std::uintptr_t base = relocation.base == ViewerAddress::Module ? block.module_base : block.code;
        const std::uintptr_t target = base + relocation.target;

        if (!image_range(relocation.offset, size, spec.allocation_size)) {
            return false;
        }
        if (relocation.pointer) {
            if (!put_native_pointer(image, relocation.offset, target)) {
                return false;
            }
        } else if (!put_native_rel32(image, relocation.offset, block.code + relocation.offset, target)) {
            return false;
        }
        used = std::max(used, relocation.offset + size);
    }
    return used != 0 && platform::windows::write_code_memory(block.code,
        std::span<const std::byte>(image.data(), used));
}

bool append_spec_patch(TraitViewerBlock& block, const ViewerPatch& patch) {
    const std::uintptr_t address = block.module_base + patch.offset;
    const std::uintptr_t destination =
        (patch.base == ViewerAddress::Module ? block.module_base : block.code) + patch.target;

    if (patch.kind == ViewerPatchKind::Bytes) {
        return append_native_patch(block.patches, address, patch.original, patch.replacement);
    }
    if (patch.kind == ViewerPatchKind::Jump || patch.kind == ViewerPatchKind::Call) {
        return append_native_branch(block.patches, address, destination, patch.original,
            patch.kind == ViewerPatchKind::Call ? std::byte{0xe8} : std::byte{0xe9});
    }

    std::uintptr_t original = 0;

    // The roster title is an asset pointer; capture its exact value only after
    // all list, column and function guards match. Format/sort pointers also have
    // explicit module-relative guards in the source specification.
    if (!platform::windows::read_memory(address, std::as_writable_bytes(std::span(&original, 1)))) {
        return false;
    }
    return append_native_patch(block.patches, address, std::as_bytes(std::span(&original, 1)),
        std::as_bytes(std::span(&destination, 1)));
}

bool prepare_group(TraitViewerBlock& block, const ViewerSpec& spec, std::uintptr_t module_base,
    TraitViewerStatus& status) {
    if (block.prepared) {
        if (block.module_base != module_base) {
            status = TraitViewerStatus::UnsupportedVersion;
            return false;
        }
        return true;
    }
    if (!verify_spec(module_base, spec)) {
        status = TraitViewerStatus::UnsupportedVersion;
        return false;
    }
    block.module_base = module_base;
    block.patches = {};
    if (spec.allocation_size != 0) {
        if (!block.code) {
            block.code = platform::windows::allocate_code_near(module_base + spec.anchor, spec.allocation_size);
        }
        if (!block.code) {
            status = TraitViewerStatus::AllocationFailed;
            return false;
        }
        if (!build_image(block, spec)) {
            status = TraitViewerStatus::UnsupportedVersion;
            return false;
        }
    }
    for (const ViewerPatch& patch : spec.patches) {
        if (!append_spec_patch(block, patch)) {
            status = TraitViewerStatus::UnsupportedVersion;
            return false;
        }
    }
    block.prepared = true;
    return true;
}

bool has_live_objects(const TraitViewerBlock& block, const ViewerSpec& spec) {
    if (!block.code || !spec.has_live_objects) {
        return false;
    }

    std::uint64_t objects = 0;

    return !platform::windows::read_memory(block.code, std::as_writable_bytes(std::span(&objects, 1))) ||
        objects != 0;
}

} // namespace

TraitViewerBlock::~TraitViewerBlock() {
    if (code && !exposed) {
        platform::windows::free_code(code);
    }
}

bool trait_viewer_native_applied(const TraitViewerNative& state) {
    for (const TraitViewerBlock& group : state.groups) {
        if (!group.patches.applied || group.patches.uncertain) {
            return false;
        }
    }
    return true;
}

bool set_trait_viewer_native(TraitViewerNative& state, std::uintptr_t module_base, bool enabled) {
    std::array<std::size_t, TraitViewerNative::GROUP_COUNT> changed{};
    std::size_t changed_count = 0;

    state.failed_group = TraitViewerNative::GROUP_COUNT;
    if (!module_base || module_base > std::numeric_limits<std::uintptr_t>::max() - MAX_MODULE_OFFSET) {
        state.status = TraitViewerStatus::UnsupportedVersion;
        return false;
    }
    // Prepare all nine groups before exposing the first image. OFF first checks
    // all object counters, avoiding a predictable partial teardown.
    for (std::size_t index = 0; index < state.groups.size(); ++index) {
        TraitViewerBlock& group = state.groups[index];
        const ViewerSpec& spec = *VIEWER_SPECS[index];

        if (enabled && !prepare_group(group, spec, module_base, state.status)) {
            state.failed_group = index;
            return false;
        }
        if (!enabled && (group.patches.applied || group.patches.uncertain) && has_live_objects(group, spec)) {
            state.status = TraitViewerStatus::LiveObjects;
            state.failed_group = index;
            return false;
        }
    }
    for (std::size_t step = 0; step < state.groups.size(); ++step) {
        const std::size_t index = enabled ? step : state.groups.size() - step - 1;
        TraitViewerBlock& group = state.groups[index];

        if (group.patches.applied == enabled && !group.patches.uncertain) {
            continue;
        }
        if (enabled && !verify_spec(module_base, *VIEWER_SPECS[index])) {
            state.status = TraitViewerStatus::UnsupportedVersion;
        } else {
            // Even failed writes may briefly publish a branch. Never recycle
            // its image based on a zero object counter or rollback alone.
            group.exposed = group.code != 0 || group.exposed;
            if (set_native_patch_group(group.patches, enabled)) {
                changed[changed_count++] = index;
                continue;
            }
            state.status = TraitViewerStatus::PatchFailed;
        }
        state.failed_group = index;
        while (changed_count != 0) {
            const std::size_t previous_index = changed[--changed_count];
            TraitViewerBlock& previous = state.groups[previous_index];

            if ((enabled && has_live_objects(previous, *VIEWER_SPECS[previous_index])) ||
                !set_native_patch_group(previous.patches, !enabled)) {
                state.status = TraitViewerStatus::RollbackFailed;
            }
        }
        return false;
    }
    state.status = enabled ? TraitViewerStatus::Applied : TraitViewerStatus::Disabled;
    return true;
}

} // namespace s8::game
