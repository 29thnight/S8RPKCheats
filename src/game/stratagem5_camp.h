#pragma once

#include "stratagem5_count.h"

namespace s8::game {

struct Stratagem5CampSlot {
    domain::GameEpoch epoch;
    std::uintptr_t owner;
    std::uintptr_t metadata_table;
    std::uintptr_t address;
    std::uintptr_t original;
    std::uint32_t original_count;
};

struct Stratagem5Camp {
    std::optional<Stratagem5CampSlot> slot;
};

bool attach_stratagem5_camp(Stratagem5Camp& state, domain::GameEpoch epoch,
    std::uintptr_t owner, std::uintptr_t metadata_table, const Stratagem5Plan& plan);
bool restore_stratagem5_camp(Stratagem5Camp& state, domain::GameEpoch epoch);
void reset_stratagem5_camp(Stratagem5Camp& state);

} // namespace s8::game
