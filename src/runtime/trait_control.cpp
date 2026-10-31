#include "trait_control.h"

#include "../services/ui_bridge.h"

#include <span>
#include <utility>

namespace s8::runtime {
namespace {

constexpr std::uint64_t RETRY_INTERVAL_MS = 100;

domain::TraitApplyStatus runtime_status(game::TraitRuntimeStatus status) {

    switch (status) {
    case game::TraitRuntimeStatus::Disabled: return domain::TraitApplyStatus::Disabled;
    case game::TraitRuntimeStatus::WaitingForTable: return domain::TraitApplyStatus::WaitingForTable;
    case game::TraitRuntimeStatus::Applied: return domain::TraitApplyStatus::Applied;
    case game::TraitRuntimeStatus::UnsupportedVersion: return domain::TraitApplyStatus::UnsupportedVersion;
    case game::TraitRuntimeStatus::InvalidText: return domain::TraitApplyStatus::InvalidText;
    case game::TraitRuntimeStatus::AllocationFailed: return domain::TraitApplyStatus::AllocationFailed;
    case game::TraitRuntimeStatus::HookFailed: return domain::TraitApplyStatus::HookFailed;
    case game::TraitRuntimeStatus::MemoryFailed: return domain::TraitApplyStatus::MemoryFailed;
    case game::TraitRuntimeStatus::Conflict: return domain::TraitApplyStatus::Conflict;
    case game::TraitRuntimeStatus::CallbackBusy: return domain::TraitApplyStatus::CallbackBusy;
    case game::TraitRuntimeStatus::ExternalRuntime: return domain::TraitApplyStatus::ExternalRuntime;
    default: return domain::TraitApplyStatus::UnsupportedVersion;
    }
}

domain::TraitApplyStatus viewer_status(game::TraitViewerStatus status) {

    switch (status) {
    case game::TraitViewerStatus::Disabled: return domain::TraitApplyStatus::Disabled;
    case game::TraitViewerStatus::Applied: return domain::TraitApplyStatus::Applied;
    case game::TraitViewerStatus::UnsupportedVersion: return domain::TraitApplyStatus::UnsupportedVersion;
    case game::TraitViewerStatus::AllocationFailed: return domain::TraitApplyStatus::AllocationFailed;
    case game::TraitViewerStatus::LiveObjects: return domain::TraitApplyStatus::LiveObjects;
    case game::TraitViewerStatus::PatchFailed: return domain::TraitApplyStatus::MemoryFailed;
    case game::TraitViewerStatus::RollbackFailed: return domain::TraitApplyStatus::RollbackFailed;
    default: return domain::TraitApplyStatus::UnsupportedVersion;
    }
}

bool viewer_busy(const game::TraitViewerNative& viewer) {

    if (viewer.status == game::TraitViewerStatus::LiveObjects) {
        return true;
    }

    if (viewer.status == game::TraitViewerStatus::PatchFailed && viewer.failed_group < viewer.groups.size()) {

        const game::NativePatchGroup& patches = viewer.groups[viewer.failed_group].patches;

        return !patches.uncertain && patches.last_result == platform::windows::CodePatchResult::Busy;
    }

    return false;
}

bool legacy_overrides(const domain::TraitTextEdits& rows) {

    for (const domain::TraitTextEdit& row : rows) {

        if (!row.name.empty() || !row.description.empty()) {
            return true;
        }
    }

    return false;
}

void queue_catalog(TraitControl& control, own::shared_owner<const domain::TraitRuntimeCatalog> catalog, bool source) {

    control.catalog = catalog;
    control.pending_catalog = std::move(catalog);
    control.catalog_retry_ms = 0;
    control.view.catalog = domain::TraitApplyStatus::Pending;
    if (source) {
        control.source_catalog = control.catalog;
        control.view.embedded_rows = own::make_shared<domain::TraitTextEdits>(
            domain::make_embedded_trait_text_rows(*control.source_catalog));
    }

    ++control.view.source_generation;
}

void publish_status(TraitControl& control, const char* label, domain::TraitApplyStatus status) {

    control.notice = std::string(label) + ": " + domain::trait_apply_status_text(status);
}

domain::Result<void> failure(bool busy, std::uint32_t code) {

    return std::unexpected(domain::Error{
        busy ? domain::ErrorCode::QueueFull : domain::ErrorCode::PlatformFailure, code});
}

} // namespace

void consume_trait_configuration(TraitControl& control, services::UiBridge& bridge) {

    if (control.stopping) {
        return;
    }

    own::shared_owner<const domain::TraitRuntimeCatalog> catalog = bridge.take_trait_catalog();
    own::shared_owner<const domain::TraitTextEdits> legacy = bridge.take_legacy_trait_texts();

    if (catalog) {
        queue_catalog(control, std::move(catalog), true);
    }

    if (legacy) {
        control.view.legacy_rows = legacy;
        control.pending_legacy = std::move(legacy);
        control.legacy_retry_ms = 0;
        control.view.legacy_text = domain::TraitApplyStatus::Pending;
        ++control.view.source_generation;
    }
}

void request_trait_viewer(TraitControl& control, bool enabled) {

    control.viewer_requested = enabled;
    control.viewer_pending = true;
    control.viewer_retry_ms = 0;
    control.view.viewer = domain::TraitApplyStatus::Pending;
}

domain::Result<void> request_trait_text_apply(TraitControl& control, const domain::ApplyTraitTextEdits& request) {

    if (control.stopping || !request.rows) {
        return std::unexpected(domain::Error{domain::ErrorCode::Cancelled});
    }

    for (const domain::TraitTextEdit& row : *request.rows) {

        if (row.embedded != request.embedded || domain::validate_trait_text_edit(row) != domain::TraitTextValidation::Valid) {
            return std::unexpected(domain::Error{domain::ErrorCode::InvalidData});
        }
    }

    if (request.embedded) {

        if (!control.source_catalog || control.external_runtime) {
            return std::unexpected(domain::Error{domain::ErrorCode::UnsupportedVersion});
        }

        domain::Result<domain::TraitRuntimeCatalog> updated = domain::apply_embedded_trait_text_edits(
            *control.source_catalog, std::span<const domain::TraitTextEdit>(*request.rows));

        if (!updated) {
            return std::unexpected(updated.error());
        }

        queue_catalog(control, own::make_shared<domain::TraitRuntimeCatalog>(std::move(*updated)), false);
        control.view.embedded_rows = request.rows;
    } else {
        control.view.legacy_rows = request.rows;
        control.pending_legacy = request.rows;
        control.legacy_retry_ms = 0;
        control.view.legacy_text = domain::TraitApplyStatus::Pending;
        ++control.view.source_generation;
    }

    return {};
}

void reconcile_trait_control(TraitControl& control, std::uintptr_t module_base,
    std::uintptr_t game_root, std::uint64_t epoch, std::uint64_t now_ms, bool external_runtime) {

    if (control.stopping) {
        return;
    }

    if (control.external_runtime != external_runtime) {
        control.external_runtime = external_runtime;

        if (control.catalog) {
            control.pending_catalog = control.catalog;
            control.catalog_retry_ms = 0;
            control.view.catalog = domain::TraitApplyStatus::Pending;
        }
    }

    if (control.viewer_pending && now_ms >= control.viewer_retry_ms) {

        const bool applied = game::set_trait_viewer_native(control.viewer, module_base, control.viewer_requested);
        const bool retry = !applied && viewer_busy(control.viewer);

        control.view.viewer_applied = game::trait_viewer_native_applied(control.viewer);
        control.view.viewer = retry ? domain::TraitApplyStatus::CallbackBusy : viewer_status(control.viewer.status);
        control.viewer_pending = retry;
        control.viewer_retry_ms = now_ms + RETRY_INTERVAL_MS;

        if (!retry) {
            publish_status(control, "기재 3칸 표시", control.view.viewer);
        }

        return;
    }

    if (control.pending_catalog && now_ms >= control.catalog_retry_ms) {

        const bool applied = game::set_trait_runtime_catalog(control.runtime, module_base,
            control.pending_catalog, external_runtime);
        const bool retry = !applied && control.runtime.status == game::TraitRuntimeStatus::CallbackBusy;

        control.view.catalog = runtime_status(control.runtime.status);
        control.catalog_retry_ms = now_ms + RETRY_INTERVAL_MS;

        if (!retry) {
            control.pending_catalog = {};
            publish_status(control, "확장 기재", control.view.catalog);

            // An empty catalog or external runtime may disable the shared name
            // entry. Legacy text overrides own an independent request to it.
            if (control.view.legacy_rows && legacy_overrides(*control.view.legacy_rows)) {
                control.pending_legacy = control.view.legacy_rows;
                control.legacy_retry_ms = 0;
                control.view.legacy_text = domain::TraitApplyStatus::Pending;
            }
        }

        return;
    }

    if (control.pending_legacy && now_ms >= control.legacy_retry_ms) {

        const bool overrides = legacy_overrides(*control.pending_legacy);
        bool applied = false;

        if (!overrides) {
            applied = game::clear_legacy_trait_text_edits(control.legacy_text);
        } else if (game::ensure_trait_runtime_name_hook(control.runtime, module_base)) {
            applied = game::apply_legacy_trait_text_edits(control.legacy_text, module_base,
                std::span<const domain::TraitTextEdit>(*control.pending_legacy));
        } else {
            control.view.legacy_text = runtime_status(control.runtime.status);
            control.pending_legacy = {};
            publish_status(control, "기본 기재 텍스트", control.view.legacy_text);
            return;
        }

        const bool retry = !applied &&
            (control.legacy_text.patches.last_result == platform::windows::CodePatchResult::Busy ||
             control.legacy_text.recovery.last_result == platform::windows::CodePatchResult::Busy);

        control.legacy_retry_ms = now_ms + RETRY_INTERVAL_MS;
        control.view.legacy_text = applied ? (overrides ? domain::TraitApplyStatus::Applied : domain::TraitApplyStatus::Disabled) :
            retry ? domain::TraitApplyStatus::CallbackBusy : domain::TraitApplyStatus::MemoryFailed;

        if (!retry) {
            control.pending_legacy = {};
            publish_status(control, "기본 기재 텍스트", control.view.legacy_text);
        }

        return;
    }

    if (control.runtime.catalog && !control.external_runtime) {

        const domain::TraitApplyStatus previous = control.view.catalog;

        game::tick_trait_runtime_native(control.runtime, game_root, epoch, now_ms);
        control.view.catalog = runtime_status(control.runtime.status);

        if (control.view.catalog != previous && control.view.catalog != domain::TraitApplyStatus::WaitingForTable) {
            publish_status(control, "확장 기재", control.view.catalog);
        }
    }
}

domain::Result<void> stop_trait_control(TraitControl& control, std::uintptr_t module_base,
    std::uintptr_t game_root, std::uint64_t epoch) {

    control.stopping = true;
    control.pending_catalog = {};
    control.pending_legacy = {};
    control.viewer_pending = false;

    if (!game::clear_legacy_trait_text_edits(control.legacy_text)) {

        const platform::windows::CodePatchResult result = control.legacy_text.recovery_pending
            ? control.legacy_text.recovery.last_result : control.legacy_text.patches.last_result;

        return failure(result == platform::windows::CodePatchResult::Busy, 0x2000u + static_cast<std::uint32_t>(result));
    }

    if (!game::stop_trait_runtime_native(control.runtime, game_root, epoch)) {
        return failure(control.runtime.status == game::TraitRuntimeStatus::CallbackBusy,
            0x2100u + static_cast<std::uint32_t>(control.runtime.status));
    }

    if (!game::set_trait_viewer_native(control.viewer, module_base, false)) {
        return failure(viewer_busy(control.viewer), 0x2200u + static_cast<std::uint32_t>(control.viewer.status));
    }

    control.view.legacy_text = domain::TraitApplyStatus::Disabled;
    control.view.catalog = domain::TraitApplyStatus::Disabled;
    control.view.viewer = domain::TraitApplyStatus::Disabled;
    control.view.viewer_applied = false;
    return {};
}

} // namespace s8::runtime
