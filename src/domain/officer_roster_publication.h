#pragma once

#include "ids.h"
#include "officer_roster.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace s8::domain {

inline constexpr std::size_t OFFICER_ROSTER_LIMIT = 5102;
inline constexpr std::size_t OFFICER_ROSTER_ID_SLOTS = OFFICER_ROSTER_LIMIT + 1;
inline constexpr std::size_t OFFICER_ROSTER_BATCH_CAPACITY = 128;

enum class OfficerRosterUpdateKind : std::uint8_t { Reset, Rows, Complete };

struct OfficerRosterUpdate {
    GameEpoch epoch{};
    std::uint64_t generation = 0;
    OfficerRosterUpdateKind kind = OfficerRosterUpdateKind::Reset;
    std::uint16_t count = 0;
    bool available = false;
    bool loading = false;
    std::array<OfficerRosterEntry, OFFICER_ROSTER_BATCH_CAPACITY> entries{};
};

// Owned and mutated only by the Present consumer. Each valid row lives at its
// officer ID; walking IDs produces the original ascending order without a sort.
// Readers borrow this owner within the UI thread, never across publication.
struct OfficerRosterSnapshot {
    GameEpoch epoch{};
    std::uint64_t generation = 0;
    std::uint64_t revision = 0;
    std::size_t entry_count = 0;
    std::array<OfficerRosterEntry, OFFICER_ROSTER_ID_SLOTS> entries{};
    bool stream_initialized = false;
    bool available = false;
    bool loading = false;
};

} // namespace s8::domain
