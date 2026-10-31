#include "stratagem5_runtime.h"

namespace s8::game {

Stratagem5SettingsResult set_stratagem5_settings(
    Stratagem5Runtime& runtime, domain::Stratagem5Settings input,
    const domain::StratagemRecord* current, WriteStratagem5 write, void* context) {
    runtime.settings = domain::normalize_stratagem5(input);
    if (!runtime.session.applied_table || !current)
        return Stratagem5SettingsResult::stored;
    if (!update_applied_stratagem5(runtime.session, *current, runtime.settings, write, context))
        return Stratagem5SettingsResult::write_failed;
    return Stratagem5SettingsResult::updated;
}

Stratagem5ApplyResult apply_stratagem5_snapshot(
    Stratagem5Runtime& runtime, StratagemTableId table,
    std::span<const std::byte, STRATAGEM_TABLE_SNAPSHOT_SIZE> snapshot,
    WriteStratagem5 write, void* context) {
    runtime.session.requested = true;
    return apply_stratagem5_table(runtime.session, table, decode_stratagem_table(snapshot),
                                   runtime.settings, write, context);
}

bool stratagem5_ready(
    const Stratagem5Runtime& runtime, StratagemTableId table,
    const domain::StratagemRecord& current) {
    return is_stratagem5_ready(runtime.session, table, current, runtime.settings);
}

} // namespace s8::game
