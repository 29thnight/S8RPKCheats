#pragma once

#include "../domain/error.h"
#include "../domain/trait_text_edit.h"

#include <span>
#include <string>
#include <string_view>

namespace s8::services {

domain::Result<domain::TraitTextEdits> read_legacy_trait_text_config(std::string_view source);
domain::Result<std::string> write_legacy_trait_text_config(std::span<const domain::TraitTextEdit> rows);
domain::Result<std::string> update_embedded_trait_text_config(std::string_view source,
    std::span<const domain::TraitTextEdit> rows);

} // namespace s8::services
