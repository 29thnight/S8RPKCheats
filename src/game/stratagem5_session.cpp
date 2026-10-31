#include "stratagem5_session.h"

namespace s8::game {

Stratagem5ApplyResult apply_stratagem5_table(
    Stratagem5Session& session, StratagemTableId table,
    const std::array<domain::StratagemRecord, 5>& records,
    const domain::Stratagem5Settings& settings, WriteStratagem5 write, void* context) {
    if (!is_expected_stratagem_table(records)) {
        return Stratagem5ApplyResult::invalid_table;
    }

    if (session.applied_table && *session.applied_table == table &&
        domain::matches_stratagem5(records[4], settings)) {
        session.last_written = records[4];
        session.attempted = records[4];
        session.uncertain = false;
        return Stratagem5ApplyResult::already_matches;
    }

    if (session.applied_table && *session.applied_table != table) {
        // The binding must restore a live table before replacing its address.
        // A different epoch is the explicit proof that the old lifetime ended.
        if (session.applied_table->epoch == table.epoch) {
            return Stratagem5ApplyResult::write_failed;
        }
        session.applied_table.reset();
        session.original = {};
        session.last_written = {};
        session.attempted = {};
        session.uncertain = false;
        session.last_restore = Stratagem5RestoreStatus::Abandoned;
    }

    if (!session.applied_table) {
        session.original = records[4];
        session.last_written = records[4];
    }

    const domain::StratagemRecord updated = domain::apply_stratagem5(records[1], settings);

    if (session.uncertain && encode_stratagem(session.attempted) != encode_stratagem(updated)) {
        return Stratagem5ApplyResult::write_failed;
    }
    session.applied_table = table;
    session.attempted = updated;
    session.uncertain = true;
    if (!write(context, table, encode_stratagem(updated))) {
        return Stratagem5ApplyResult::write_failed;
    }

    session.last_written = updated;
    session.uncertain = false;
    session.last_restore = Stratagem5RestoreStatus::None;
    return Stratagem5ApplyResult::applied;
}

bool update_applied_stratagem5(
    Stratagem5Session& session, const domain::StratagemRecord& current,
    const domain::Stratagem5Settings& settings, WriteStratagem5 write, void* context) {
    if (!session.applied_table) {
        return false;
    }
    const domain::StratagemRecord updated = domain::apply_stratagem5(current, settings);

    if (session.uncertain && encode_stratagem(session.attempted) != encode_stratagem(updated)) {
        return false;
    }
    session.attempted = updated;
    session.uncertain = true;
    if (!write(context, *session.applied_table, encode_stratagem(updated))) {
        return false;
    }
    session.last_written = updated;
    session.uncertain = false;
    return true;
}

bool is_stratagem5_ready(
    const Stratagem5Session& session, StratagemTableId table,
    const domain::StratagemRecord& current, const domain::Stratagem5Settings& settings) {
    return session.requested && !session.uncertain && session.applied_table && *session.applied_table == table &&
           domain::matches_stratagem5(current, settings);
}

bool restore_stratagem5(Stratagem5Session& session, WriteStratagem5 write, void* context) {
    if (!session.applied_table) {
        return true;
    }

    const bool restored = write(context, *session.applied_table, encode_stratagem(session.original));

    session.last_restore = restored ? Stratagem5RestoreStatus::Restored : Stratagem5RestoreStatus::Failed;
    if (restored) {
        session.applied_table.reset();
        session.original = {};
        session.last_written = {};
        session.attempted = {};
        session.uncertain = false;
    }
    return restored;
}

bool set_stratagem5_requested(Stratagem5Session& session, bool enabled, WriteStratagem5 write, void* context) {
    session.requested = enabled;
    if (enabled || !session.applied_table) {
        return true;
    }
    return restore_stratagem5(session, write, context);
}

void reset_stratagem5_session(Stratagem5Session& session) {
    if (session.applied_table || session.last_restore == Stratagem5RestoreStatus::Failed) {
        session.last_restore = Stratagem5RestoreStatus::Abandoned;
    }
    session.applied_table.reset();
    session.original = {};
    session.last_written = {};
    session.attempted = {};
    session.uncertain = false;
}

} // namespace s8::game
