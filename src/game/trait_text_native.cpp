#include "trait_text_native.h"
#include "../platform/windows/code_memory.h"
#include "../platform/windows/process_memory.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <array>
#include <atomic>
#include <cstring>
#include <cwchar>
#include <limits>
#include <new>
#include <string>
#include <utility>
#include <vector>

namespace s8::game {

struct NativeTraitNameEdits {
    struct Entry {
        std::array<wchar_t, 513> original{};
        std::array<wchar_t, 6> replacement{};
    };

    std::array<Entry, 72> entries{};
    std::size_t count = 0;
    const NativeTraitNameEdits* previous = nullptr;
};

namespace {

constexpr std::uintptr_t NORMAL_SITE = 0x170ace0;
constexpr std::uintptr_t SPECIAL_SITE = 0x146e5;
constexpr std::size_t IMAGE_HEADER = sizeof(std::uintptr_t) * 2;
constexpr std::array<std::byte, 10> NORMAL_ORIGINAL = native_hex("66833F000F84AE000000");
constexpr std::array<std::byte, 5> SPECIAL_ORIGINAL = native_hex("6645391C24");
constexpr std::array<std::byte, 6> SPECIAL_TAIL = native_hex("0F84B1040000");
std::atomic<const NativeTraitNameEdits*> published_names{nullptr};

struct WideDescriptionEdit {
    std::wstring original;
    std::wstring replacement;
};

struct PendingImage {
    std::uintptr_t allocation = 0;
    std::size_t bytes = 0;
    bool retained = false;

    ~PendingImage() {
        if (allocation && !retained) {
            platform::windows::free_code(allocation);
        }
    }

    PendingImage() = default;
    PendingImage(const PendingImage&) = delete;
    PendingImage& operator=(const PendingImage&) = delete;
};

bool convert_text(const std::string& text, std::wstring& output, int maximum = 512) {
    if (text.empty()) {
        output.clear();
        return true;
    }

    const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
        static_cast<int>(text.size()), nullptr, 0);

    if (size <= 0 || size > maximum) {
        return false;
    }
    output.resize(static_cast<std::size_t>(size));
    return MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
        static_cast<int>(text.size()), output.data(), size) == size;
}

NativeTraitNameEdits* prepare_names(std::span<const domain::TraitTextEdit> rows) {
    void* allocation = VirtualAlloc(nullptr, sizeof(NativeTraitNameEdits),
        MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

    if (!allocation) {
        return nullptr;
    }

    NativeTraitNameEdits* names = new (allocation) NativeTraitNameEdits{};

    for (const domain::TraitTextEdit& row : rows) {
        if (row.name.empty() || row.name == row.original_name) {
            continue;
        }

        if (names->count == names->entries.size() || row.original_name.empty()) {
            VirtualFree(names, 0, MEM_RELEASE);
            return nullptr;
        }

        NativeTraitNameEdits::Entry& entry = names->entries[names->count++];
        const int original_count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
            row.original_name.data(), static_cast<int>(row.original_name.size()), entry.original.data(), 512);
        const int replacement_count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
            row.name.data(), static_cast<int>(row.name.size()), entry.replacement.data(), 5);

        if (original_count <= 0 || replacement_count <= 0) {
            VirtualFree(names, 0, MEM_RELEASE);
            return nullptr;
        }
    }
    return names;
}

class CodeWriter {
public:
    explicit CodeWriter(std::vector<std::byte>& bytes) : bytes_(bytes) {}

    void byte(std::uint8_t value) {
        bytes_[position_++] = static_cast<std::byte>(value);
    }

    void word(std::uint16_t value) {
        byte(static_cast<std::uint8_t>(value));
        byte(static_cast<std::uint8_t>(value >> 8));
    }

    void dword(std::uint32_t value) {
        for (std::size_t index = 0; index < sizeof(value); ++index) {
            byte(static_cast<std::uint8_t>(value >> (index * 8)));
        }
    }

    void bytes(std::span<const std::byte> value) {
        std::memcpy(bytes_.data() + position_, value.data(), value.size());
        position_ += value.size();
    }

    bool internal(std::size_t target) {
        const std::size_t field = position_;

        dword(0);
        return put_native_rel32(bytes_, field, field, target);
    }

    bool external(std::uintptr_t image, std::uintptr_t target) {
        const std::size_t field = position_;

        dword(0);
        return put_native_rel32(bytes_, field, image + field, target);
    }

    void seek(std::size_t position) { position_ = position; }

private:
    std::vector<std::byte>& bytes_;
    std::size_t position_ = 0;
};

bool build_descriptions(std::span<const WideDescriptionEdit> edits, bool special,
    std::uintptr_t module, PendingImage& output) {
    if (edits.empty()) {
        return true;
    }

    std::array<std::size_t, 72> starts{};
    std::array<std::size_t, 72> strings{};
    const std::size_t comparison_size = special ? 17 : 15;
    const std::size_t matched_size = special ? 23 : 12;
    std::size_t size = 0;

    if (edits.size() > starts.size()) {
        return false;
    }
    for (std::size_t index = 0; index < edits.size(); ++index) {
        starts[index] = size;
        size += (edits[index].original.size() + 1) * comparison_size + matched_size;
    }

    const std::size_t original_block = size;

    size += special ? 10 : 15;
    size = (size + 7) & ~std::size_t{7};
    for (std::size_t index = 0; index < edits.size(); ++index) {
        strings[index] = size;
        size += (edits[index].replacement.size() + 1) * sizeof(wchar_t);
    }
    output.bytes = size + IMAGE_HEADER;
    output.allocation = platform::windows::allocate_code_near(module + (special ? SPECIAL_SITE : NORMAL_SITE), output.bytes);
    if (!output.allocation) {
        return false;
    }

    const std::uintptr_t image = output.allocation + IMAGE_HEADER;
    std::vector<std::byte> bytes(size);
    CodeWriter writer(bytes);

    for (std::size_t index = 0; index < edits.size(); ++index) {
        const WideDescriptionEdit& edit = edits[index];
        const std::size_t mismatch = index + 1 < edits.size() ? starts[index + 1] : original_block;

        writer.seek(starts[index]);
        for (std::size_t character = 0; character <= edit.original.size(); ++character) {
            writer.byte(0x66);
            if (special) {
                writer.byte(0x41); writer.byte(0x81); writer.byte(0xbc); writer.byte(0x24);
            } else {
                writer.byte(0x81); writer.byte(0xbf);
            }
            writer.dword(static_cast<std::uint32_t>(character * sizeof(wchar_t)));
            writer.word(character < edit.original.size() ? static_cast<std::uint16_t>(edit.original[character]) : 0);
            writer.byte(0x0f); writer.byte(0x85);
            if (!writer.internal(mismatch)) {
                return false;
            }
        }
        writer.byte(special ? 0x4c : 0x48); writer.byte(0x8d); writer.byte(special ? 0x25 : 0x3d);
        if (!writer.internal(strings[index])) {
            return false;
        }
        if (special) {
            writer.byte(0x4c); writer.byte(0x89); writer.byte(0xe6);
            writer.byte(0x4c); writer.byte(0x89); writer.byte(0xa4); writer.byte(0x24); writer.dword(0xd8);
        }
        writer.byte(0xe9);
        if (!writer.internal(original_block)) {
            return false;
        }
        std::memcpy(bytes.data() + strings[index], edit.replacement.c_str(),
            (edit.replacement.size() + 1) * sizeof(wchar_t));
    }
    writer.seek(original_block);
    if (special) {
        writer.bytes(SPECIAL_ORIGINAL);
        writer.byte(0xe9);
        if (!writer.external(image, module + 0x146ea)) {
            return false;
        }
    } else {
        writer.byte(0x66); writer.byte(0x83); writer.byte(0x3f); writer.byte(0);
        writer.byte(0x0f); writer.byte(0x84);
        if (!writer.external(image, module + 0x170ad98)) {
            return false;
        }
        writer.byte(0xe9);
        if (!writer.external(image, module + 0x170acea)) {
            return false;
        }
    }
    return platform::windows::write_code_memory(image, bytes);
}

bool description_patch(NativePatchGroup& group, std::uintptr_t module, bool special,
    const PendingImage& image) {
    const std::uintptr_t address = module + (special ? SPECIAL_SITE : NORMAL_SITE);
    const std::span<const std::byte> original = special ?
        std::span<const std::byte>(SPECIAL_ORIGINAL) : std::span<const std::byte>(NORMAL_ORIGINAL);

    if (!image.allocation) {
        return append_native_patch(group, address, original, original);
    }
    return append_native_branch(group, address, image.allocation + IMAGE_HEADER, original);
}

void retain_image(TraitTextNative& state, PendingImage& image) {
    if (!image.allocation) {
        return;
    }

    const std::array<std::uintptr_t, 2> header{{state.retained_images, image.bytes}};

    // The private header is never executed. Keep an explicit chain of every
    // potentially published image, even when a later OFF restores its branch.
    std::memcpy(reinterpret_cast<void*>(image.allocation), header.data(), sizeof(header));
    state.retained_images = image.allocation;
    image.retained = true;
}

struct PendingNames {
    NativeTraitNameEdits* value = nullptr;
    bool retained = false;

    ~PendingNames() {
        if (value && !retained) {
            VirtualFree(value, 0, MEM_RELEASE);
        }
    }
};

} // namespace

bool apply_legacy_trait_text_edits(TraitTextNative& state, std::uintptr_t module_base,
    std::span<const domain::TraitTextEdit> rows) {
    if (rows.size() > 72 || state.recovery_pending || state.recovery.uncertain || state.patches.uncertain || !module_base ||
        module_base > std::numeric_limits<std::uintptr_t>::max() - NORMAL_SITE - NORMAL_ORIGINAL.size() ||
        (state.module_base && state.module_base != module_base)) {
        return false;
    }
    for (const domain::TraitTextEdit& row : rows) {
        if (row.embedded || domain::validate_trait_text_edit(row) != domain::TraitTextValidation::Valid) {
            return false;
        }
    }
    if (!native_bytes_match(module_base + SPECIAL_SITE + SPECIAL_ORIGINAL.size(), SPECIAL_TAIL)) {
        return false;
    }

    PendingNames names{prepare_names(rows)};
    std::vector<WideDescriptionEdit> normal_edits{};
    std::vector<WideDescriptionEdit> special_edits{};

    if (!names.value) {
        return false;
    }
    normal_edits.reserve(rows.size());
    special_edits.reserve(2);
    for (const domain::TraitTextEdit& row : rows) {
        if (row.description.empty() || row.description == row.original_description) {
            continue;
        }

        WideDescriptionEdit edit{};
        const std::string replacement = domain::prepare_legacy_trait_description(row.description);

        if (!convert_text(row.original_description, edit.original) || !convert_text(replacement, edit.replacement, 1024)) {
            return false;
        }
        if (row.original_name == "괴물" || row.original_name == "잔병첩보") {
            special_edits.push_back(std::move(edit));
        } else {
            normal_edits.push_back(std::move(edit));
        }
    }

    PendingImage normal{};
    PendingImage special{};
    NativePatchGroup desired{};
    NativePatchGroup transaction{};

    if (!build_descriptions(normal_edits, false, module_base, normal) ||
        !build_descriptions(special_edits, true, module_base, special) ||
        !description_patch(desired, module_base, false, normal) ||
        !description_patch(desired, module_base, true, special)) {
        return false;
    }
    for (std::size_t index = 0; index < desired.count; ++index) {
        const NativePatch& next = desired.entries[index];
        const NativePatch& before = state.patches.applied ? state.patches.entries[index] : next;
        const std::span<const std::byte> expected = state.patches.applied ?
            std::span<const std::byte>(before.replacement.data(), before.size) :
            std::span<const std::byte>(next.original.data(), next.size);

        if (!append_native_patch(transaction, next.address, expected,
                std::span<const std::byte>(next.replacement.data(), next.size))) {
            return false;
        }
    }
    // These two branches are committed together. Every potentially observed
    // destination remains mapped; an uncertain rollback also keeps its recipe.
    const bool applied = set_native_patch_group(transaction, true);
    const platform::windows::CodePatchResult result = transaction.last_result;

    if (applied || result == platform::windows::CodePatchResult::WriteFailed ||
        result == platform::windows::CodePatchResult::RollbackFailed ||
        result == platform::windows::CodePatchResult::ResumeFailed) {
        retain_image(state, normal);
        retain_image(state, special);
    }
    if (!applied) {
        if (transaction.uncertain) {
            state.recovery = transaction;
            state.recovery_pending = true;
        }
        return false;
    }
    desired.applied = true;
    state.patches = desired;
    state.module_base = module_base;
    names.value->previous = state.names;
    state.names = names.value;
    names.retained = true;
    published_names.store(state.names, std::memory_order_release);
    state.applied = state.names->count != 0 || !normal_edits.empty() || !special_edits.empty();
    return true;
}

bool clear_legacy_trait_text_edits(TraitTextNative& state) {
    if (state.recovery_pending || state.recovery.uncertain) {
        if (!set_native_patch_group(state.recovery, false)) {
            return false;
        }
        state.recovery_pending = false;
    }
    if ((state.patches.applied || state.patches.uncertain) && !set_native_patch_group(state.patches, false)) {
        return false;
    }
    published_names.store(nullptr, std::memory_order_release);
    state.applied = false;
    return true;
}

const wchar_t* trait_text_name_override(const wchar_t* original) noexcept {
    const NativeTraitNameEdits* names = published_names.load(std::memory_order_acquire);

    if (!names || !original) {
        return original;
    }
    for (std::size_t index = 0; index < names->count; ++index) {
        const NativeTraitNameEdits::Entry& entry = names->entries[index];

        if (std::wcscmp(original, entry.original.data()) == 0) {
            return entry.replacement.data();
        }
    }
    return original;
}

} // namespace s8::game
