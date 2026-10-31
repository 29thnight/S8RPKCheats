#pragma once

#include "../domain/stratagem5.h"

#include <array>
#include <cstdint>
#include <optional>
#include <span>

namespace s8::game {

struct Stratagem5UnitSnapshot {
    std::uint64_t identity;
    std::uint8_t morale;
    std::optional<std::uint16_t> troops;
    std::optional<std::uint16_t> max_troops;
};

struct Stratagem5HealHistory {
    struct Entry { std::uint64_t identity; std::uint8_t morale; };
    std::uint64_t list_identity = 0;
    bool primed = false;
    std::array<std::optional<Entry>, 60> entries{};
};

using Stratagem5HealChanges = std::array<std::optional<std::uint16_t>, 60>;

void begin_stratagem5_healing(Stratagem5HealHistory& history, std::uint64_t list_identity);
std::optional<std::uint16_t> observe_stratagem5_unit(
    Stratagem5HealHistory& history, std::size_t index,
    const std::optional<Stratagem5UnitSnapshot>& unit, const domain::Stratagem5Settings& settings);
void finish_stratagem5_healing(Stratagem5HealHistory& history, std::size_t count, bool any_valid);

Stratagem5HealChanges observe_stratagem5_healing(
    Stratagem5HealHistory& history, std::uint64_t list_identity,
    std::span<const std::optional<Stratagem5UnitSnapshot>> units,
    const domain::Stratagem5Settings& settings);

} // namespace s8::game
