#include "native_officer_name_service.h"

#include "ui_bridge.h"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace s8::services {

void poll_native_officer_names(NativeOfficerNameService& state, UiBridge& bridge) {
    const own::shared_owner<const domain::OfficerNames> overrides = bridge.officer_name_overrides_view();

    if (own::identity(overrides) != own::identity(state.overrides)) {
        state.overrides = overrides;
        state.changed = true;
        state.publish_ready = true;
    }
    for (std::size_t index = 0; index < 8; ++index) {
        const std::optional<domain::NativeOfficerNameBatch> batch = bridge.native_officer_names.pop();

        if (!batch) {
            break;
        }
        if (batch->generation != state.generation || batch->epoch != state.epoch || batch->reset) {
            state.generation = batch->generation;
            state.epoch = batch->epoch;
            for (std::string& name : state.native) {
                name.clear();
            }
            state.changed = true;
            state.publish_ready = true;
        }
        state.publish_ready = state.publish_ready || batch->publish;
        for (std::size_t entry_index = 0; entry_index < batch->count; ++entry_index) {
            const domain::NativeOfficerName& entry = batch->entries[entry_index];
            const std::array<char, domain::NATIVE_NAME_TEXT_CAPACITY>::const_iterator end =
                std::find(entry.text.begin(), entry.text.end(), '\0');

            if (entry.id == 0 || entry.id > 5102 || end == entry.text.begin() || end == entry.text.end()) {
                continue;
            }
            const std::string_view text(entry.text.data(), static_cast<std::size_t>(end - entry.text.begin()));
            std::string& current = state.native[entry.id];

            if (current != text) {
                current.assign(text);
                state.changed = true;
            }
        }
    }
    if (!state.changed) {
        state.publish_ready = false;
        return;
    }
    if (!state.publish_ready) {
        return;
    }
    domain::OfficerNames merged = state.overrides ? *state.overrides : domain::OfficerNames{};

    // Nonempty file names are the original's authoritative overrides. Empty
    // reserved-child rows are omitted by read_officer_names. Native results
    // never become overrides, so generated names can change on later polls.
    for (std::size_t id = 1; id < state.native.size(); ++id) {
        if (!state.native[id].empty()) {
            merged.try_emplace(static_cast<int>(id), state.native[id]);
        }
    }
    state.changed = !bridge.publish_resolved_officer_names(std::move(merged), state.overrides);
    state.publish_ready = state.changed;
}

} // namespace s8::services
