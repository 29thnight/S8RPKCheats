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
    state.applied = Stratagem5ModelApplied{epoch, entry_address, count_address, plan.before, plan.original_count};

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

void restore_stratagem5_model(Stratagem5ModelBinding& state, domain::GameEpoch epoch) {
    if (state.applied && state.applied->epoch == epoch) {
        const auto& applied = *state.applied;
        std::uint32_t count = 0;
        if (platform::windows::read_memory(applied.count_address, std::as_writable_bytes(std::span(&count, 1))) &&
            count == applied.original_count + 1)
            platform::windows::write_memory(applied.count_address, std::as_bytes(std::span(&applied.original_count, 1)));
        platform::windows::write_memory(applied.entry_address, std::as_bytes(std::span(&applied.original_entry, 1)));
    }
    state.applied.reset();
}

void reset_stratagem5_model(Stratagem5ModelBinding& state) {
    state.applied.reset();
}

} // namespace s8::game
