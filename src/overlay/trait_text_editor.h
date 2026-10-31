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
    bool open = false;
    std::array<domain::TraitTextEdits, 2> rows;
    std::array<own::shared_owner<const domain::TraitTextEdits>, 2> sources;
    std::array<bool, 2> dirty{};
    std::array<int, 2> selected{-1, -1};
    std::array<char, 128> filter{};
    std::array<char, 128> name{};
    std::array<char, 2049> description{};
    int loaded_tab = -1;
    int loaded_row = -1;
    bool reload_requested = false;
    own::shared_owner<const domain::TraitTextEdits> save_rows;
    bool save_embedded = false;
    bool save_pending = false;
    std::optional<services::ConfigSaveStatus> save_status;
    domain::TraitTextValidation validation = domain::TraitTextValidation::Valid;
    std::size_t invalid_row = 0;
};

void update_trait_text_editor(TraitTextEditor& editor, const domain::TraitControlView& view,
    services::UiBridge& bridge);
void draw_trait_text_editor(FeatureMenu& menu, services::CommandMailbox& mailbox);

} // namespace s8::overlay
