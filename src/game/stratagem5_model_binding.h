#pragma once

#include "stratagem5_camp.h"

namespace s8::game {

struct Stratagem5ModelApplied {
    domain::GameEpoch epoch;
    std::uintptr_t entry_address;
    std::uintptr_t count_address;
    StratagemEntry original_entry;
    std::uint32_t original_count;
    StratagemEntry replacement_entry;
    std::uintptr_t owner;
    std::uintptr_t camp_data;
    bool uncertain = false;
};

struct Stratagem5ModelBinding {
    std::optional<Stratagem5ModelApplied> applied;
};

bool extend_stratagem5_model(Stratagem5ModelBinding& state, domain::GameEpoch epoch,
                             std::uintptr_t owner, const Stratagem5Plan& plan);
bool restore_stratagem5_model(Stratagem5ModelBinding& state, domain::GameEpoch epoch);
void reset_stratagem5_model(Stratagem5ModelBinding& state);

} // namespace s8::game
