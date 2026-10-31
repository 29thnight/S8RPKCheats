#pragma once

#include "../domain/trait_control.h"
#include "../game/trait_runtime_native.h"
#include "../game/trait_text_native.h"
#include "../game/trait_viewer_native.h"

#include <cstdint>
#include <optional>
#include <string>

namespace s8::services { class UiBridge; }

namespace s8::runtime {

struct TraitControl {
    game::TraitRuntimeNative runtime;
    game::TraitTextNative legacy_text;
    game::TraitViewerNative viewer;
    domain::TraitControlView view;
    own::shared_owner<const domain::TraitRuntimeCatalog> catalog;
    own::shared_owner<const domain::TraitRuntimeCatalog> source_catalog;
    own::shared_owner<const domain::TraitRuntimeCatalog> pending_catalog;
    own::shared_owner<const domain::TraitTextEdits> pending_legacy;
    std::optional<std::string> notice;
    std::uint64_t catalog_retry_ms = 0;
    std::uint64_t legacy_retry_ms = 0;
    std::uint64_t viewer_retry_ms = 0;
    bool viewer_requested = true;
    bool viewer_pending = true;
    bool external_runtime = false;
    bool stopping = false;
};

void consume_trait_configuration(TraitControl& control, services::UiBridge& bridge);
void request_trait_viewer(TraitControl& control, bool enabled);
domain::Result<void> request_trait_text_apply(TraitControl& control, const domain::ApplyTraitTextEdits& request);
// The caller yields this turn when another native patch group was committed.
// Each call applies at most one viewer/catalog/text operation, then uses later
// boundaries for the table's 500 ms validation and deferred restoration.
void reconcile_trait_control(TraitControl& control, std::uintptr_t module_base,
    std::uintptr_t game_root, std::uint64_t epoch, std::uint64_t now_ms, bool external_runtime);
domain::Result<void> stop_trait_control(TraitControl& control, std::uintptr_t module_base,
    std::uintptr_t game_root, std::uint64_t epoch);

} // namespace s8::runtime
