#pragma once

#include "../domain/ids.h"
#include "stratagem5_model.h"

#include <cstdint>
#include <optional>

namespace s8::game {

struct Stratagem5CountCandidate {
    std::uintptr_t owner;
    Stratagem5Plan plan;
};

struct Stratagem5CountApplied {
    domain::GameEpoch epoch;
    std::uintptr_t owner;
    std::uint32_t entry_index;
    std::uint8_t original;
    std::uintptr_t camp_data;
    std::uintptr_t metadata_row;
    bool verified = false;
};

struct Stratagem5Count {
    bool requested = false;
    std::optional<Stratagem5CountApplied> applied;
};

// Candidates are provided only after their owner and plan have been validated.
std::optional<Stratagem5CountCandidate> choose_stratagem5_owner(
    const std::optional<Stratagem5CountCandidate>& dialog,
    const std::optional<Stratagem5CountCandidate>& attack,
    const std::optional<Stratagem5CountCandidate>& defense);

bool update_stratagem5_count(Stratagem5Count& state, domain::GameEpoch epoch,
    std::optional<std::uintptr_t> valid_attack, std::optional<std::uintptr_t> valid_defense,
    const std::optional<Stratagem5CountCandidate>& dialog,
    const std::optional<Stratagem5CountCandidate>& attack,
    const std::optional<Stratagem5CountCandidate>& defense);
bool disable_stratagem5_count(Stratagem5Count& state, domain::GameEpoch epoch,
    std::optional<std::uintptr_t> valid_attack, std::optional<std::uintptr_t> valid_defense);
void reset_stratagem5_count(Stratagem5Count& state);

} // namespace s8::game
