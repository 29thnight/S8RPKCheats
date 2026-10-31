#pragma once

#include <atomic>

namespace s8::runtime {

struct Stratagem5Lifecycle {
    std::atomic<bool> desired{false};
    // Effective is the request armed at a safe boundary, not proof of installed game data.
    std::atomic<bool> effective{false};
    std::atomic<bool> skip_current_battle{true};
    std::atomic<bool> last_transition_succeeded{true};
};

struct Stratagem5LifecycleActions {
    void* context;
    bool (*prepare_bridge)(void* context);
    bool (*set_data)(void* context, bool enabled);
    bool (*set_metadata)(void* context, bool enabled);
    bool (*set_count)(void* context, bool enabled);
};

void request_stratagem5(Stratagem5Lifecycle& lifecycle, bool enabled);
bool should_skip_stratagem5(const Stratagem5Lifecycle& lifecycle);
bool update_stratagem5_lifecycle(Stratagem5Lifecycle& lifecycle, bool battle_active,
                                 bool safe_non_battle, const Stratagem5LifecycleActions& actions);
void reset_stratagem5_battle(Stratagem5Lifecycle& lifecycle);

} // namespace s8::runtime
