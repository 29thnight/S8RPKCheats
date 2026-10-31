#include "stratagem5_model_binding.h"
#include "../platform/windows/process_memory.h"

#include <span>

namespace s8::game {

bool extend_stratagem5_model(Stratagem5ModelBinding& state, domain::GameEpoch epoch,
                             std::uintptr_t owner, const Stratagem5Plan& plan) {
    const std::uintptr_t entry_address = owner + 0x100 + static_cast<std::uintptr_t>(plan.original_count) * 0x10;
    const std::uintptr_t count_address = owner + 0x150;
    const std::uint32_t new_count = plan.original_count + 1;
    std::uintptr_t camp_data = 0;

    if (!platform::windows::read_memory(owner + 0x10, std::as_writable_bytes(std::span(&camp_data, 1))) || !camp_data) {
        return false;
    }
    if (state.applied && state.applied->epoch == epoch) {
        const Stratagem5ModelApplied& applied = *state.applied;

        return !applied.uncertain && plan.already_present && applied.owner == owner &&
            applied.camp_data == camp_data && applied.entry_address == entry_address &&
            applied.count_address == count_address && applied.original_count == plan.original_count &&
            applied.replacement_entry.data == plan.added.data && applied.replacement_entry.index == plan.added.index;
    }
    if (plan.already_present) {
        return true;
    }
    state.applied = Stratagem5ModelApplied{epoch, entry_address, count_address, plan.before,
        plan.original_count, plan.added, owner, camp_data};

    const bool entry_written = platform::windows::write_memory(entry_address, std::as_bytes(std::span(&plan.added, 1)));
    const bool count_written = platform::windows::write_memory(count_address, std::as_bytes(std::span(&new_count, 1)));
    StratagemEntry entry{};
    std::uint32_t count = 0;
    if (entry_written && count_written &&
        platform::windows::read_memory(entry_address, std::as_writable_bytes(std::span(&entry, 1))) &&
        platform::windows::read_memory(count_address, std::as_writable_bytes(std::span(&count, 1))) &&
        entry.data == plan.added.data && entry.index == plan.added.index &&
        entry.available == plan.added.available && count == new_count) {
        return true;
    }

    state.applied->uncertain = true;
    const bool count_restored = platform::windows::write_memory(count_address, std::as_bytes(std::span(&plan.original_count, 1)));
    const bool entry_restored = platform::windows::write_memory(entry_address, std::as_bytes(std::span(&plan.before, 1)));

    if (count_restored && entry_restored &&
        platform::windows::read_memory(entry_address, std::as_writable_bytes(std::span(&entry, 1))) &&
        platform::windows::read_memory(count_address, std::as_writable_bytes(std::span(&count, 1))) &&
        entry.data == plan.before.data && entry.index == plan.before.index &&
        entry.available == plan.before.available && count == plan.original_count) {
        state.applied.reset();
    }
    return false;
}

bool restore_stratagem5_model(Stratagem5ModelBinding& state, domain::GameEpoch epoch) {
    if (state.applied && state.applied->epoch == epoch) {
        Stratagem5ModelApplied& applied = *state.applied;
        std::uint32_t count = 0;
        std::uintptr_t camp_data = 0;
        StratagemEntry current{};

        if (!platform::windows::read_memory(applied.owner + 0x10, std::as_writable_bytes(std::span(&camp_data, 1)))) {
            return false;
        }
        if (camp_data != applied.camp_data) {
            state.applied.reset();
            return true;
        }
        if (!platform::windows::read_memory(applied.count_address, std::as_writable_bytes(std::span(&count, 1))) ||
            !platform::windows::read_memory(applied.entry_address, std::as_writable_bytes(std::span(&current, 1)))) {
            return false;
        }

        const bool owned = current.data == applied.replacement_entry.data &&
            current.index == applied.replacement_entry.index;
        const bool restored = current.data == applied.original_entry.data &&
            current.index == applied.original_entry.index && current.available == applied.original_entry.available;
        const std::span<const std::byte> current_bytes = std::as_bytes(std::span(&current, 1));
        const std::span<const std::byte> original_bytes = std::as_bytes(std::span(&applied.original_entry, 1));
        const std::span<const std::byte> replacement_bytes = std::as_bytes(std::span(&applied.replacement_entry, 1));
        bool owned_partial = applied.uncertain;

        for (std::size_t index = 0; owned_partial && index < current_bytes.size(); ++index) {
            owned_partial = current_bytes[index] == original_bytes[index] || current_bytes[index] == replacement_bytes[index];
        }
        if ((count != applied.original_count && count != applied.original_count + 1) || (!owned && !restored && !owned_partial)) {
            state.applied.reset();
            return true;
        }

        if (count != applied.original_count && !platform::windows::write_memory(applied.count_address,
                std::as_bytes(std::span(&applied.original_count, 1)))) {
            applied.uncertain = true;
            return false;
        }

        if (!restored && !platform::windows::write_memory(applied.entry_address,
                std::as_bytes(std::span(&applied.original_entry, 1)))) {
            applied.uncertain = true;
            return false;
        }
        if (!platform::windows::read_memory(applied.count_address, std::as_writable_bytes(std::span(&count, 1))) ||
            !platform::windows::read_memory(applied.entry_address, std::as_writable_bytes(std::span(&current, 1))) ||
            count != applied.original_count || current.data != applied.original_entry.data ||
            current.index != applied.original_entry.index || current.available != applied.original_entry.available) {
            applied.uncertain = true;
            return false;
        }
    }
    state.applied.reset();
    return true;
}

void reset_stratagem5_model(Stratagem5ModelBinding& state) {
    state.applied.reset();
}

} // namespace s8::game
