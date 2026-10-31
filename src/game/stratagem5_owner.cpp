#include "stratagem5_owner.h"
#include "../platform/windows/process_memory.h"

#include <array>
#include <span>

namespace s8::game {

bool is_stratagem5_owner(std::uintptr_t owner, std::uint8_t side) {
    if (!owner)
        return false;
    std::uint8_t actual_side = 0;
    std::uint16_t gauge = 0;
    std::uint8_t last_count = 0;
    return platform::windows::read_memory(owner + 0x18, std::as_writable_bytes(std::span(&actual_side, 1))) &&
           platform::windows::read_memory(owner + 0x154, std::as_writable_bytes(std::span(&gauge, 1))) &&
           platform::windows::read_memory(owner + 0x19c, std::as_writable_bytes(std::span(&last_count, 1))) &&
           actual_side == side;
}

std::optional<Stratagem5CountCandidate> read_stratagem5_candidate(
    std::uintptr_t owner, std::uintptr_t metadata_table) {
    if (!owner || !metadata_table)
        return std::nullopt;

    static_assert(sizeof(StratagemEntry) == 0x10);
    std::array<StratagemEntry, 5> entries{};
    std::array<std::uint64_t, 5> camp_rows{};
    std::uint32_t count = 0;
    std::uintptr_t camp_data = 0;
    if (!platform::windows::read_memory(owner + 0x100, std::as_writable_bytes(std::span(entries))) ||
        !platform::windows::read_memory(owner + 0x150, std::as_writable_bytes(std::span(&count, 1))) ||
        !platform::windows::read_memory(owner + 0x10, std::as_writable_bytes(std::span(&camp_data, 1))) ||
        !camp_data ||
        !platform::windows::read_memory(camp_data + 0x68, std::as_writable_bytes(std::span(camp_rows))))
        return std::nullopt;

    const auto plan = prepare_stratagem5_plan(entries, count, camp_rows, metadata_table);
    if (!plan)
        return std::nullopt;
    return Stratagem5CountCandidate{owner, *plan};
}

std::optional<Stratagem5CountCandidate> read_stratagem5_dialog_candidate(
    std::uintptr_t dialog, std::uintptr_t metadata_table) {
    if (!dialog)
        return std::nullopt;
    std::uintptr_t holder = 0;
    std::uintptr_t owner = 0;
    if (!platform::windows::read_memory(dialog + 0x20, std::as_writable_bytes(std::span(&holder, 1))) ||
        !holder ||
        !platform::windows::read_memory(holder, std::as_writable_bytes(std::span(&owner, 1))))
        return std::nullopt;
    return read_stratagem5_candidate(owner, metadata_table);
}

} // namespace s8::game
