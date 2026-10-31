#include "stratagem5_lifecycle.h"

namespace s8::runtime {

void request_stratagem5(Stratagem5Lifecycle& lifecycle, bool enabled) {
    lifecycle.desired.store(enabled);
}

bool should_skip_stratagem5(const Stratagem5Lifecycle& lifecycle) {
    return lifecycle.skip_current_battle.load() || !lifecycle.effective.load();
}

bool update_stratagem5_lifecycle(Stratagem5Lifecycle& lifecycle, bool battle_active,
                                 bool safe_non_battle, const Stratagem5LifecycleActions& actions) {
    if (battle_active || !safe_non_battle)
        return true;

    bool result = true;
    const bool desired = lifecycle.desired.load();
    if (desired != lifecycle.effective.load()) {
        if (desired) {
            actions.prepare_bridge(actions.context);
            actions.set_data(actions.context, true);
            actions.set_metadata(actions.context, true);
            actions.set_count(actions.context, true);
        } else {
            const bool metadata_ok = actions.set_metadata(actions.context, false);
            const bool count_ok = actions.set_count(actions.context, false);
            const bool data_ok = actions.set_data(actions.context, false);
            result = metadata_ok && count_ok && data_ok;
        }
        lifecycle.effective.store(desired);
    }
    lifecycle.skip_current_battle.store(false);
    return result;
}

void reset_stratagem5_battle(Stratagem5Lifecycle& lifecycle) {
    lifecycle.skip_current_battle.store(true);
}

} // namespace s8::runtime
