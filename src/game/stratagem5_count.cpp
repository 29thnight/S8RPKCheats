#include "stratagem5_count.h"
#include "../platform/windows/process_memory.h"

#include <span>

namespace s8::game {

std::optional<Stratagem5CountCandidate> choose_stratagem5_owner(
    const std::optional<Stratagem5CountCandidate>& dialog,
    const std::optional<Stratagem5CountCandidate>& attack,
    const std::optional<Stratagem5CountCandidate>& defense) {
    if (dialog)
        return dialog;
    if (attack && defense && attack->owner != defense->owner)
        return std::nullopt;
    return defense ? defense : attack;
}

bool update_stratagem5_count(Stratagem5Count& state, domain::GameEpoch epoch,
    std::optional<std::uintptr_t> valid_attack, std::optional<std::uintptr_t> valid_defense,
    const std::optional<Stratagem5CountCandidate>& dialog,
    const std::optional<Stratagem5CountCandidate>& attack,
    const std::optional<Stratagem5CountCandidate>& defense) {
    if (!state.requested)
        return false;

    if (state.applied) {
        const auto& applied = *state.applied;
        const bool current = applied.epoch == epoch &&
            (valid_attack == applied.owner || valid_defense == applied.owner);
        std::uint8_t current_value = 0;
        const auto address = applied.owner + 0x100 +
            static_cast<std::uintptr_t>(applied.entry_index) * 0x10 + 0x0c;
        if (current && platform::windows::read_memory(address, std::as_writable_bytes(std::span(&current_value, 1))))
            return true;
        state.applied.reset();
    }

    const auto candidate = choose_stratagem5_owner(dialog, attack, defense);
    if (!candidate)
        return false;
    const auto index = candidate->plan.original_count;
    const auto address = candidate->owner + 0x100 + static_cast<std::uintptr_t>(index) * 0x10 + 0x0c;
    std::uint8_t original = 0;
    if (!platform::windows::read_memory(address, std::as_writable_bytes(std::span(&original, 1))))
        return false;

    if (!candidate->plan.already_present || original != 0) {
        const std::uint8_t value = 1;
        if (!platform::windows::write_data_memory(address, std::as_bytes(std::span(&value, 1))))
            return false;
        std::uint8_t written = 0;
        if (!platform::windows::read_memory(address, std::as_writable_bytes(std::span(&written, 1))) || written != value)
            return false;
    }
    state.applied = Stratagem5CountApplied{epoch, candidate->owner, index, original};
    return true;
}

bool disable_stratagem5_count(Stratagem5Count& state, domain::GameEpoch epoch,
    std::optional<std::uintptr_t> valid_attack, std::optional<std::uintptr_t> valid_defense) {
    state.requested = false;
    if (state.applied) {
        const auto& applied = *state.applied;
        if (applied.epoch == epoch &&
            (valid_attack == applied.owner || valid_defense == applied.owner)) {
            const auto address = applied.owner + 0x100 +
                static_cast<std::uintptr_t>(applied.entry_index) * 0x10 + 0x0c;
            if (!platform::windows::write_data_memory(address, std::as_bytes(std::span(&applied.original, 1)))) {
                return false;
            }
        }
        state.applied.reset();
    }
    return true;
}

void reset_stratagem5_count(Stratagem5Count& state) {
    state.applied.reset();
}

} // namespace s8::game
