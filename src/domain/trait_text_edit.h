#pragma once

#include "error.h"
#include "trait_runtime.h"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace s8::domain {

struct TraitTextEdit {
    std::uint16_t trait_id = 0;
    bool embedded = false;
    std::string original_name;
    std::string original_description;
    std::string name;
    std::string description;
};

using TraitTextEdits = std::vector<TraitTextEdit>;

enum class TraitTextValidation {
    Valid,
    InvalidId,
    InvalidUtf8,
    NameTooLong,
    DescriptionTooLong,
    FormatMismatch
};

TraitTextEdits make_legacy_trait_text_rows();
TraitTextEdits make_embedded_trait_text_rows(const TraitRuntimeCatalog& catalog);
TraitTextValidation validate_trait_text_edit(const TraitTextEdit& row);
// Preserve validated value tokens and escape literal percentages for the
// native printf-style description path. Called only when applying an edit.
std::string prepare_legacy_trait_description(std::string_view description);
Result<TraitRuntimeCatalog> apply_embedded_trait_text_edits(const TraitRuntimeCatalog& catalog,
    std::span<const TraitTextEdit> rows);

} // namespace s8::domain
