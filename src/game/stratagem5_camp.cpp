#include "stratagem5_camp.h"
#include "../platform/windows/process_memory.h"

#include <span>

namespace s8::game {

bool attach_stratagem5_camp(Stratagem5Camp& state, domain::GameEpoch epoch,
    std::uintptr_t owner, std::uintptr_t metadata_table, const Stratagem5Plan& plan) {
    if (state.slot && state.slot->epoch == epoch && state.slot->owner == owner &&
        state.slot->metadata_table == metadata_table)
        return true;
    state.slot.reset();
    std::uintptr_t camp_data = 0;
    if (!platform::windows::read_memory(owner + 0x10, std::as_writable_bytes(std::span(&camp_data, 1))) || !camp_data)
        return false;
    const auto address = camp_data + 0x68 + static_cast<std::uintptr_t>(plan.camp_slot) * sizeof(std::uintptr_t);
    const auto row5 = metadata_table + 4 * 0x20;
    std::uintptr_t original = 0;
    if (!platform::windows::read_memory(address, std::as_writable_bytes(std::span(&original, 1))) ||
        (original != 0 && original != row5))
        return false;
    if (original != row5) {
        if (!platform::windows::write_data_memory(address, std::as_bytes(std::span(&row5, 1))))
            return false;
        std::uintptr_t written = 0;
        if (!platform::windows::read_memory(address, std::as_writable_bytes(std::span(&written, 1))) || written != row5)
            return false;
    }
    state.slot = Stratagem5CampSlot{epoch, owner, metadata_table, address, original, plan.original_count};
    return true;
}

void restore_stratagem5_camp(Stratagem5Camp& state, domain::GameEpoch epoch) {
    if (state.slot && state.slot->epoch == epoch)
        platform::windows::write_data_memory(state.slot->address, std::as_bytes(std::span(&state.slot->original, 1)));
    state.slot.reset();
}

void reset_stratagem5_camp(Stratagem5Camp& state) {
    state.slot.reset();
}

} // namespace s8::game
