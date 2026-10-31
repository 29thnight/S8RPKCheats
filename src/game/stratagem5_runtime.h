#pragma once

#include "stratagem5_session.h"

namespace s8::game {

struct Stratagem5Runtime {
    domain::Stratagem5Settings settings{};
    Stratagem5Session session{};
};

enum class Stratagem5SettingsResult { stored, updated, write_failed };

// current is supplied only when the applied record was read in the game context.
Stratagem5SettingsResult set_stratagem5_settings(
    Stratagem5Runtime& runtime, domain::Stratagem5Settings input,
    const domain::StratagemRecord* current, WriteStratagem5 write, void* context);

Stratagem5ApplyResult apply_stratagem5_snapshot(
    Stratagem5Runtime& runtime, StratagemTableId table,
    std::span<const std::byte, STRATAGEM_TABLE_SNAPSHOT_SIZE> snapshot,
    WriteStratagem5 write, void* context);

bool stratagem5_ready(
    const Stratagem5Runtime& runtime, StratagemTableId table,
    const domain::StratagemRecord& current);

} // namespace s8::game
