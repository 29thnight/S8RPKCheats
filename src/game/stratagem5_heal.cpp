#include "stratagem5_heal.h"

#include <algorithm>

namespace s8::game {

void begin_stratagem5_healing(Stratagem5HealHistory& history, std::uint64_t list_identity) {
    if (history.list_identity != list_identity) {
        history = {};
        history.list_identity = list_identity;
    }
}

std::optional<std::uint16_t> observe_stratagem5_unit(
    Stratagem5HealHistory& history, std::size_t index,
    const std::optional<Stratagem5UnitSnapshot>& unit, const domain::Stratagem5Settings& settings) {
    if (!unit) {
        history.entries[index].reset();
        return std::nullopt;
    }

    std::optional<std::uint16_t> change;
    const auto& previous = history.entries[index];
    if (history.primed && previous && previous->identity == unit->identity &&
        settings.heal_amount > 0 && settings.effect1 == 10 && settings.power1 > 0 &&
        unit->morale > previous->morale) {
        const int before = previous->morale;
        const bool matches = static_cast<int>(unit->morale) - before == settings.power1 ||
            (unit->morale == 100 && before < 100 && before + settings.power1 >= 100);
        if (matches && unit->troops && unit->max_troops) {
            const auto next = std::min(static_cast<std::uint32_t>(*unit->troops) +
                                       static_cast<std::uint32_t>(settings.heal_amount),
                                       static_cast<std::uint32_t>(*unit->max_troops));
            if (next != *unit->troops)
                change = static_cast<std::uint16_t>(next);
        }
    }
    history.entries[index] = Stratagem5HealHistory::Entry{unit->identity, unit->morale};
    return change;
}

void finish_stratagem5_healing(Stratagem5HealHistory& history, std::size_t count, bool any_valid) {
    if (!history.primed && any_valid)
        history.primed = true;
    for (std::size_t i = count; i < history.entries.size(); ++i)
        history.entries[i].reset();
}

Stratagem5HealChanges observe_stratagem5_healing(
    Stratagem5HealHistory& history, std::uint64_t list_identity,
    std::span<const std::optional<Stratagem5UnitSnapshot>> units,
    const domain::Stratagem5Settings& settings) {
    Stratagem5HealChanges changes{};
    if (units.empty())
        return changes;
    begin_stratagem5_healing(history, list_identity);
    const auto count = std::min(units.size(), history.entries.size());
    bool any_valid = false;
    for (std::size_t i = 0; i < count; ++i) {
        any_valid |= units[i].has_value();
        changes[i] = observe_stratagem5_unit(history, i, units[i], settings);
    }
    finish_stratagem5_healing(history, count, any_valid);
    return changes;
}

} // namespace s8::game
