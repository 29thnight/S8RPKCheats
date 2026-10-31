#include "stratagem5_count.h"
#include "../platform/windows/process_memory.h"

#include <span>

namespace s8::game {
namespace {

template<typename T>
bool read_value(std::uintptr_t address, T& value) {
    return platform::windows::read_memory(address, std::as_writable_bytes(std::span(&value, 1)));
}

bool write_count(std::uintptr_t address, std::uint8_t value) {
    std::uint8_t current = 0;

    return platform::windows::write_memory(address, std::as_bytes(std::span(&value, 1))) &&
        read_value(address, current) && current == value;
}

} // namespace

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
    if (!state.requested) {
        return false;
    }
    const std::optional<Stratagem5CountCandidate> candidate = choose_stratagem5_owner(dialog, attack, defense);

    if (!candidate) {
        return false;
    }

    if (state.applied) {
        Stratagem5CountApplied& applied = *state.applied;
        std::uintptr_t camp_data = 0;
        const bool same_owner = applied.epoch == epoch && candidate->owner == applied.owner &&
            candidate->plan.original_count == applied.entry_index && candidate->plan.added.data == applied.metadata_row;

        if (same_owner && !read_value(applied.owner + 0x10, camp_data)) {
            return false;
        }
        const bool current = same_owner && camp_data == applied.camp_data;
        std::uint8_t current_value = 0;
        const std::uintptr_t address = applied.owner + 0x100 +
            static_cast<std::uintptr_t>(applied.entry_index) * 0x10 + 0x0c;

        if (current) {
            if (!read_value(address, current_value)) {
                return false;
            }
            if (!applied.verified) {
                applied.verified = current_value == 1 || write_count(address, 1);
            }
            return applied.verified;
        }
        // The caller restores MODEL -> CAMP -> COUNT before an owner switch.
        // Only an explicit epoch change permits dropping an unresolved backup.
        if (applied.epoch == epoch) {
            return false;
        }
        state.applied.reset();
    }

    const std::uint32_t index = candidate->plan.original_count;
    const std::uintptr_t address = candidate->owner + 0x100 + static_cast<std::uintptr_t>(index) * 0x10 + 0x0c;
    std::uint8_t original = 0;
    std::uintptr_t camp_data = 0;

    if (!read_value(address, original) || !read_value(candidate->owner + 0x10, camp_data) || !camp_data) {
        return false;
    }
    state.applied = Stratagem5CountApplied{epoch, candidate->owner, index, original,
        camp_data, candidate->plan.added.data, false};

    if (!candidate->plan.already_present || original != 0) {
        if (!write_count(address, 1)) {
            return false;
        }
    }
    state.applied->verified = true;
    (void)valid_attack;
    (void)valid_defense;
    return true;
}

bool disable_stratagem5_count(Stratagem5Count& state, domain::GameEpoch epoch,
    std::optional<std::uintptr_t> valid_attack, std::optional<std::uintptr_t> valid_defense) {
    state.requested = false;
    if (state.applied) {
        const Stratagem5CountApplied& applied = *state.applied;

        if (applied.epoch == epoch) {
            std::uintptr_t camp_data = 0;
            std::uint32_t count = 0;
            StratagemEntry entry{};
            const std::uintptr_t address = applied.owner + 0x100 +
                static_cast<std::uintptr_t>(applied.entry_index) * 0x10;

            if (!read_value(applied.owner + 0x10, camp_data)) {
                return false;
            }
            if (camp_data != applied.camp_data) {
                state.applied.reset();
                return true;
            }
            if (!read_value(applied.owner + 0x150, count) || !read_value(address, entry)) {
                return false;
            }
            // The early native dialog is a valid owner even before side/gauge
            // captures exist. Its saved camp/model identities permit recovery.
            const bool owned = (count == applied.entry_index || count == applied.entry_index + 1) &&
                (entry.data == 0 || entry.data == applied.metadata_row) &&
                (entry.index == 0 || entry.index == applied.entry_index);

            if (!owned) {
                state.applied.reset();
                return true;
            }
            const std::uint8_t current = static_cast<std::uint8_t>(entry.available);

            if (current != applied.original && current <= 1 && !write_count(address + 0x0c, applied.original)) {
                return false;
            }
        }
        state.applied.reset();
    }
    (void)valid_attack;
    (void)valid_defense;
    return true;
}

void reset_stratagem5_count(Stratagem5Count& state) {
    state.applied.reset();
}

} // namespace s8::game
