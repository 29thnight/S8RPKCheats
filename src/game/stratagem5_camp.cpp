#include "stratagem5_camp.h"
#include "../platform/windows/process_memory.h"

#include <span>

namespace s8::game {

bool attach_stratagem5_camp(Stratagem5Camp& state, domain::GameEpoch epoch,
    std::uintptr_t owner, std::uintptr_t metadata_table, const Stratagem5Plan& plan) {
    std::uintptr_t camp_data = 0;

    if (!platform::windows::read_memory(owner + 0x10, std::as_writable_bytes(std::span(&camp_data, 1))) || !camp_data) {
        return false;
    }
    const std::uintptr_t address = camp_data + 0x68 + static_cast<std::uintptr_t>(plan.camp_slot) * sizeof(std::uintptr_t);
    const std::uintptr_t row5 = metadata_table + 4 * 0x20;
    std::uintptr_t original = 0;

    if (!platform::windows::read_memory(address, std::as_writable_bytes(std::span(&original, 1))) ||
        (original != 0 && original != row5)) {
        return false;
    }
    const bool same_slot = state.slot && state.slot->epoch == epoch && state.slot->owner == owner &&
        state.slot->metadata_table == metadata_table && state.slot->camp_data == camp_data && state.slot->address == address;

    if (state.slot && state.slot->epoch == epoch && !same_slot) {
        return false;
    }
    if (!same_slot) {
        state.slot = Stratagem5CampSlot{epoch, owner, metadata_table, address, original, plan.original_count, camp_data};
    }
    if (original != row5) {
        state.slot->uncertain = true;
        if (!platform::windows::write_memory(address, std::as_bytes(std::span(&row5, 1)))) {
            return false;
        }
        std::uintptr_t written = 0;

        if (!platform::windows::read_memory(address, std::as_writable_bytes(std::span(&written, 1))) || written != row5) {
            return false;
        }
    }
    state.slot->uncertain = false;
    return true;
}

bool restore_stratagem5_camp(Stratagem5Camp& state, domain::GameEpoch epoch) {

    if (state.slot && state.slot->epoch == epoch) {

        Stratagem5CampSlot& slot = *state.slot;
        std::uintptr_t current = 0;
        std::uintptr_t camp_data = 0;
        const std::uintptr_t replacement = slot.metadata_table + 4 * 0x20;

        if (!platform::windows::read_memory(slot.owner + 0x10, std::as_writable_bytes(std::span(&camp_data, 1)))) {
            return false;
        }
        if (camp_data != slot.camp_data) {
            state.slot.reset();
            return true;
        }
        if (!platform::windows::read_memory(slot.address, std::as_writable_bytes(std::span(&current, 1)))) {
            return false;
        }
        const std::span<const std::byte> current_bytes = std::as_bytes(std::span(&current, 1));
        const std::span<const std::byte> original_bytes = std::as_bytes(std::span(&slot.original, 1));
        const std::span<const std::byte> replacement_bytes = std::as_bytes(std::span(&replacement, 1));
        bool owned_partial = slot.uncertain;

        for (std::size_t index = 0; owned_partial && index < current_bytes.size(); ++index) {
            owned_partial = current_bytes[index] == original_bytes[index] || current_bytes[index] == replacement_bytes[index];
        }
        if (current != slot.original && current != replacement && !owned_partial) {
            state.slot.reset();
            return true;
        }

        if (current != slot.original) {
            slot.uncertain = true;

            if (!platform::windows::write_memory(slot.address, std::as_bytes(std::span(&slot.original, 1))) ||
                !platform::windows::read_memory(slot.address, std::as_writable_bytes(std::span(&current, 1))) ||
                current != slot.original) {
                return false;
            }
        }
    }

    state.slot.reset();
    return true;
}

void reset_stratagem5_camp(Stratagem5Camp& state) {
    state.slot.reset();
}

} // namespace s8::game
