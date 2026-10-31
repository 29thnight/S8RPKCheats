#pragma once

#include "stratagem5_session.h"

#include <array>
#include <cstdint>

namespace s8::game {

// Bound and used by the game-context caller for one table and epoch.
struct Stratagem5Memory {
    StratagemTableId table;
    std::uintptr_t payload_table;
};

using StratagemTableSnapshot = std::array<std::byte, STRATAGEM_TABLE_SNAPSHOT_SIZE>;

bool read_stratagem_table(const Stratagem5Memory& memory, StratagemTableSnapshot& snapshot);
bool write_stratagem5(void* context, StratagemTableId table, const StratagemPayload& payload);

} // namespace s8::game
