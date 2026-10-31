#pragma once

#include "stratagem5_memory.h"
#include "stratagem5_runtime.h"
#include "stratagem5_heal.h"
#include "../domain/work_budget.h"

#include <cstddef>
#include <optional>

namespace s8::game {

struct Stratagem5HealingProgress {
    StratagemTableId table{};
    Stratagem5HealHistory history;
    std::uintptr_t unit_list = 0;
    std::size_t unit_count = 0;
    std::size_t unit_cursor = 0;
    bool pending = false;
    bool any_valid = false;
    std::uintptr_t target_unit = 0;
    std::uint16_t troops_before = 0;
    std::uint16_t troops_after = 0;
    bool write_pending = false;
};

struct Stratagem5Binding {
    Stratagem5Runtime runtime;
    std::optional<Stratagem5Memory> memory;
    Stratagem5HealHistory heal_history;
    Stratagem5HealingProgress heal_progress;
};

// The caller invokes these functions in the game context and supplies its epoch.
bool enable_stratagem5(Stratagem5Binding& binding, domain::GameEpoch epoch, std::uintptr_t module_base);
bool refresh_stratagem5(Stratagem5Binding& binding, domain::GameEpoch epoch, std::uintptr_t module_base);
bool apply_stratagem5_metadata(Stratagem5Binding& binding, domain::GameEpoch epoch, std::uintptr_t metadata_table);
Stratagem5SettingsResult set_stratagem5_settings(Stratagem5Binding& binding, domain::Stratagem5Settings settings);
bool disable_stratagem5(Stratagem5Binding& binding);
// Shutdown retries revalidate the still-owned table before reusing its backup.
bool stop_stratagem5(Stratagem5Binding& binding, domain::GameEpoch epoch);
void end_stratagem5_battle(Stratagem5Binding& binding);
void change_stratagem5_session(Stratagem5Binding& binding);
void update_stratagem5_healing(Stratagem5Binding& binding, int unit_count, std::uintptr_t unit_list,
                              const domain::WorkBudget& work_budget = {});

} // namespace s8::game
