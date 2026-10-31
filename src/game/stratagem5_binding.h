#pragma once

#include "stratagem5_memory.h"
#include "stratagem5_runtime.h"
#include "stratagem5_heal.h"

#include <optional>

namespace s8::game {

struct Stratagem5Binding {
    Stratagem5Runtime runtime;
    std::optional<Stratagem5Memory> memory;
    Stratagem5HealHistory heal_history;
};

// The caller invokes these functions in the game context and supplies its epoch.
bool enable_stratagem5(Stratagem5Binding& binding, domain::GameEpoch epoch, std::uintptr_t module_base);
bool refresh_stratagem5(Stratagem5Binding& binding, domain::GameEpoch epoch, std::uintptr_t module_base);
bool apply_stratagem5_metadata(Stratagem5Binding& binding, domain::GameEpoch epoch, std::uintptr_t metadata_table);
Stratagem5SettingsResult set_stratagem5_settings(Stratagem5Binding& binding, domain::Stratagem5Settings settings);
bool disable_stratagem5(Stratagem5Binding& binding);
void end_stratagem5_battle(Stratagem5Binding& binding);
void change_stratagem5_session(Stratagem5Binding& binding);
void update_stratagem5_healing(Stratagem5Binding& binding, int unit_count, std::uintptr_t unit_list);

} // namespace s8::game
