#include "stratagem5_session.h"

namespace s8::game {

Stratagem5ApplyResult apply_stratagem5_table(
    Stratagem5Session& session, StratagemTableId table,
    const std::array<domain::StratagemRecord, 5>& records,
    const domain::Stratagem5Settings& settings, WriteStratagem5 write, void* context) {
    if (!is_expected_stratagem_table(records))
        return Stratagem5ApplyResult::invalid_table;

    if (session.applied_table && *session.applied_table == table &&
        domain::matches_stratagem5(records[4], settings))
        return Stratagem5ApplyResult::already_matches;

    if (session.applied_table && *session.applied_table != table) {
        session.applied_table.reset();
        session.original = {};
    }

    if (!session.applied_table)
        session.original = records[4];

    const auto updated = domain::apply_stratagem5(records[1], settings);
    if (!write(context, table, encode_stratagem(updated)))
        return Stratagem5ApplyResult::write_failed;

    session.applied_table = table;
    return Stratagem5ApplyResult::applied;
}

bool update_applied_stratagem5(
    const Stratagem5Session& session, const domain::StratagemRecord& current,
    const domain::Stratagem5Settings& settings, WriteStratagem5 write, void* context) {
    if (!session.applied_table)
        return false;
    return write(context, *session.applied_table,
                 encode_stratagem(domain::apply_stratagem5(current, settings)));
}

bool is_stratagem5_ready(
    const Stratagem5Session& session, StratagemTableId table,
    const domain::StratagemRecord& current, const domain::Stratagem5Settings& settings) {
    return session.requested && session.applied_table && *session.applied_table == table &&
           domain::matches_stratagem5(current, settings);
}

bool restore_stratagem5(Stratagem5Session& session, WriteStratagem5 write, void* context) {
    bool restored = false;
    if (session.applied_table)
        restored = write(context, *session.applied_table, encode_stratagem(session.original));
    session.applied_table.reset();
    session.original = {};
    return restored;
}

bool set_stratagem5_requested(Stratagem5Session& session, bool enabled, WriteStratagem5 write, void* context) {
    session.requested = enabled;
    if (enabled || !session.applied_table)
        return true;
    return restore_stratagem5(session, write, context);
}

void reset_stratagem5_session(Stratagem5Session& session) {
    session.applied_table.reset();
    session.original = {};
}

} // namespace s8::game
