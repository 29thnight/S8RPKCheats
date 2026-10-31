#pragma once

#include "stratagem5_count.h"

namespace s8::game {

bool is_stratagem5_owner(std::uintptr_t owner, std::uint8_t side);
std::optional<Stratagem5CountCandidate> read_stratagem5_candidate(
    std::uintptr_t owner, std::uintptr_t metadata_table);
std::optional<Stratagem5CountCandidate> read_stratagem5_dialog_candidate(
    std::uintptr_t dialog, std::uintptr_t metadata_table);

} // namespace s8::game
