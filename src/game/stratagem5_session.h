#pragma once

#include "stratagem_table.h"
#include "../domain/ids.h"

#include <cstdint>
#include <optional>

namespace s8::game {

// The caller resolves this identity and performs writes in the game context.
struct StratagemTableId {
    domain::GameEpoch epoch;
    std::uint64_t value;
    bool operator==(const StratagemTableId&) const = default;
};

enum class Stratagem5RestoreStatus { None, Restored, Failed, Abandoned };

struct Stratagem5Session {
    bool requested = false;
    std::optional<StratagemTableId> applied_table;
    domain::StratagemRecord original{};
    domain::StratagemRecord last_written{};
    domain::StratagemRecord attempted{};
    bool uncertain = false;
    Stratagem5RestoreStatus last_restore = Stratagem5RestoreStatus::None;
};

using WriteStratagem5 = bool (*)(void* context, StratagemTableId table, const StratagemPayload& payload);

enum class Stratagem5ApplyResult { applied, already_matches, invalid_table, write_failed };

Stratagem5ApplyResult apply_stratagem5_table(
    Stratagem5Session& session, StratagemTableId table,
    const std::array<domain::StratagemRecord, 5>& records,
    const domain::Stratagem5Settings& settings, WriteStratagem5 write, void* context);

bool update_applied_stratagem5(
    Stratagem5Session& session, const domain::StratagemRecord& current,
    const domain::Stratagem5Settings& settings, WriteStratagem5 write, void* context);
bool is_stratagem5_ready(
    const Stratagem5Session& session, StratagemTableId table,
    const domain::StratagemRecord& current, const domain::Stratagem5Settings& settings);

// The binding revalidates object identity before a retry. A failed restoration
// keeps original/attempted bytes; a session change explicitly abandons them.
bool restore_stratagem5(Stratagem5Session& session, WriteStratagem5 write, void* context);
bool set_stratagem5_requested(Stratagem5Session& session, bool enabled, WriteStratagem5 write, void* context);
void reset_stratagem5_session(Stratagem5Session& session);

} // namespace s8::game
