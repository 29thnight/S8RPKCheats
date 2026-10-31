#pragma once

#include "native_patch.h"
#include "../domain/trait_text_edit.h"

#include <cstdint>
#include <span>

namespace s8::game {

struct NativeTraitNameEdits;

struct TraitTextNative {
    NativePatchGroup patches{};
    NativePatchGroup recovery{};
    std::uintptr_t module_base = 0;
    std::uintptr_t retained_images = 0;
    const NativeTraitNameEdits* names = nullptr;
    bool recovery_pending = false;
    bool applied = false;
};

// The 72-row legacy editor shares TraitRuntimeNative's name getter. Both
// description sites are replaced together after all text validates.
bool apply_legacy_trait_text_edits(TraitTextNative& state, std::uintptr_t module_base,
    std::span<const domain::TraitTextEdit> rows);
bool clear_legacy_trait_text_edits(TraitTextNative& state);

// Callback-only lookup: immutable publication, no allocation or locks.
const wchar_t* trait_text_name_override(const wchar_t* original) noexcept;

} // namespace s8::game
