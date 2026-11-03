#pragma once

#include "../domain/trait_control.h"
#include "../services/config_save.h"

#include <array>
#include <cstddef>
#include <optional>

namespace s8::services { class UiBridge; class CommandMailbox; }

namespace s8::overlay {

struct FeatureMenu;

struct TraitTextEditor {
    std::array<own::shared_owner<const domain::TraitTextEdits>, 2> sources;
    std::array<int, 2> selected{-1, -1};
    std::array<char, 128> filter{};
    bool debug_enabled = false;
    own::shared_owner<const domain::TraitTextEdits> save_rows;
    bool save_embedded = false;
    bool apply_external = false;
    bool save_pending = false;
    std::optional<services::ConfigSaveStatus> save_status;
};

void update_trait_text_editor(TraitTextEditor& editor, const domain::TraitControlView& view,
    services::UiBridge& bridge);
void draw_trait_debug_window(FeatureMenu& menu);

} // namespace s8::overlay
