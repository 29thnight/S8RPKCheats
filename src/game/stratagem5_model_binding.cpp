#include "stratagem5_model_binding.h"
#include "../platform/windows/process_memory.h"

#include <span>

namespace s8::game {

bool extend_stratagem5_model(Stratagem5ModelBinding& state, domain::GameEpoch epoch,
                             std::uintptr_t owner, const Stratagem5Plan& plan) {
    if (plan.already_present)
        return true;
    const auto entry_address = owner + 0x100 + static_cast<std::uintptr_t>(plan.original_count) * 0x10;
    const auto count_address = owner + 0x150;
    const auto new_count = plan.original_count + 1;
    state.applied = Stratagem5ModelApplied{epoch, entry_address, count_address, plan.before, plan.original_count, plan.added};

    const bool entry_written = platform::windows::write_memory(entry_address, std::as_bytes(std::span(&plan.added, 1)));
    const bool count_written = platform::windows::write_memory(count_address, std::as_bytes(std::span(&new_count, 1)));
    StratagemEntry entry{};
    std::uint32_t count = 0;
    if (entry_written && count_written &&
        platform::windows::read_memory(entry_address, std::as_writable_bytes(std::span(&entry, 1))) &&
        platform::windows::read_memory(count_address, std::as_writable_bytes(std::span(&count, 1))) &&
        entry.data == plan.added.data && entry.index == plan.added.index &&
        entry.available == plan.added.available && count == new_count)
        return true;

    platform::windows::write_memory(count_address, std::as_bytes(std::span(&plan.original_count, 1)));
    platform::windows::write_memory(entry_address, std::as_bytes(std::span(&plan.before, 1)));
    state.applied.reset();
    return false;
}

bool restore_stratagem5_model(Stratagem5ModelBinding& state, domain::GameEpoch epoch) {
    if (state.applied && state.applied->epoch == epoch) {
        const Stratagem5ModelApplied& applied = *state.applied;
        std::uint32_t count = 0;
        StratagemEntry current{};

        if (!platform::windows::read_memory(applied.count_address, std::as_writable_bytes(std::span(&count, 1))) ||
            !platform::windows::read_memory(applied.entry_address, std::as_writable_bytes(std::span(&current, 1)))) {
            return false;
        }

        const bool owned = current.data == applied.replacement_entry.data &&
            current.index == applied.replacement_entry.index;
        const bool restored = current.data == applied.original_entry.data &&
            current.index == applied.original_entry.index && current.available == applied.original_entry.available;

        if ((count != applied.original_count && count != applied.original_count + 1) || (!owned && !restored)) {
            state.applied.reset();
            return true;
        }

        if (count != applied.original_count && !platform::windows::write_memory(applied.count_address,
                std::as_bytes(std::span(&applied.original_count, 1)))) {
            return false;
        }

        if (!restored && !platform::windows::write_memory(applied.entry_address,
                std::as_bytes(std::span(&applied.original_entry, 1)))) {
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
