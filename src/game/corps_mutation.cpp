#include "corps_mutation.h"

#include "city_management.h"
#include "../platform/windows/process_memory.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <span>

namespace s8::game {
namespace {

constexpr std::uintptr_t CITY_STRIDE = 0x2a0;
constexpr std::uintptr_t OFFICER_STRIDE = 0x3d0;

template<class T> bool read_value(std::uintptr_t address, T& value) {
    return platform::windows::read_memory(address, std::as_writable_bytes(std::span(&value, 1)));
}

template<class T> T from_record(const std::array<std::byte, 0xf0>& bytes, std::size_t offset) {
    T result{};

    std::memcpy(&result, bytes.data() + offset, sizeof(result));
    return result;
}

std::span<const std::byte> data_bytes(const std::uint64_t& value, std::size_t width) {
    return std::as_bytes(std::span(&value, 1)).first(width);
}

std::uint16_t& journal_slot(CorpsDeployment& state, const CorpsDataWrite& write) {
    if (write.general) { return state.general_journal; }
    if (write.row != NO_CORPS_ROW) { return write.status ? state.status_journal[write.row] : state.city_journal[write.row]; }
    return state.governor_journal[write.city];
}

domain::CorpsStatus write_data(CorpsDeployment& state, std::span<const CorpsDataWrite> writes) {
    for (const CorpsDataWrite& write : writes) {
        std::uint64_t current = 0;

        if (!platform::windows::read_memory(write.address, std::as_writable_bytes(std::span(&current, 1)).first(write.width)) ||
            current != write.before) {
            return domain::CorpsStatus::OwnershipChanged;
        }
        if (journal_slot(state, write) == NO_CORPS_ROW && state.journal_count + writes.size() > state.journal.size()) {
            return domain::CorpsStatus::CapacityExceeded;
        }
    }
    for (const CorpsDataWrite& write : writes) {
        std::uint16_t& slot = journal_slot(state, write);

        if (slot == NO_CORPS_ROW) {
            slot = static_cast<std::uint16_t>(state.journal_count++);
            state.journal[slot] = {write.address, write.before, write.before, write.after,
                write.row == NO_CORPS_ROW ? domain::NO_CITY_OFFICER : state.rows[write.row].id, write.width};
        } else {
            CorpsJournalEntry& entry = state.journal[slot];

            if (entry.address != write.address || entry.width != write.width) { return domain::CorpsStatus::OwnershipChanged; }
            entry.previous = write.before;
            entry.after = write.after;
        }
        // Record the attempted write before calling the memory boundary; a
        // partially written field is also part of rollback ownership.
        ++state.write_sequence;
        if (!platform::windows::write_data_memory(write.address, data_bytes(write.after, write.width))) {
            return domain::CorpsStatus::WriteFailed;
        }
    }
    for (const CorpsDataWrite& write : writes) {
        std::uint64_t current = 0;

        if (!platform::windows::read_memory(write.address, std::as_writable_bytes(std::span(&current, 1)).first(write.width)) ||
            current != write.after) {
            return domain::CorpsStatus::WriteFailed;
        }
    }
    return domain::CorpsStatus::Complete;
}

bool same_corps_city(const CorpsDeployment& state, std::uint8_t city) {
    std::uintptr_t corps = 0;
    const std::uintptr_t address = state.city_table + city * CITY_STRIDE;

    return city < domain::CITY_COUNT && read_city_force(address) == state.player_force &&
        read_value(address + 0x90, corps) && corps == state.corps;
}

std::uint16_t officer_index(const CorpsDeployment& state, std::uintptr_t address) {
    if (address < state.roster) { return NO_CORPS_ROW; }
    const std::uintptr_t delta = address - state.roster;

    return delta % OFFICER_STRIDE == 0 && delta / OFFICER_STRIDE < state.rows.size()
        ? static_cast<std::uint16_t>(delta / OFFICER_STRIDE) : NO_CORPS_ROW;
}

bool governor_owner(const CorpsDeployment& state, std::uintptr_t pointer) {
    if (pointer == 0) { return true; }
    const std::uint16_t slot = officer_index(state, pointer);
    std::uint16_t id = 0;
    std::uintptr_t force = 0;

    return slot != NO_CORPS_ROW && read_value(pointer + 8, id) && id == state.rows[slot].id &&
        read_value(pointer + 0x18, force) && force == state.player_force;
}

} // namespace

void begin_corps_journal(CorpsDeployment& state) {
    state.journal_count = 0;
    state.journal_moved = state.snapshot.moved;
    state.journal_swapped = state.snapshot.swapped;
    state.journal_appointed = state.snapshot.appointed;
    state.city_journal.fill(NO_CORPS_ROW);
    state.status_journal.fill(NO_CORPS_ROW);
    state.governor_journal.fill(NO_CORPS_ROW);
    state.general_journal = NO_CORPS_ROW;
}

bool restore_corps_journal_unit(CorpsDeployment& state) {
    if (state.journal_count == 0) { return true; }
    const CorpsJournalEntry& entry = state.journal[state.journal_count - 1];
    std::array<std::byte, sizeof(std::uint64_t)> current{};
    const std::span<const std::byte> before = data_bytes(entry.before, entry.width);
    const std::span<const std::byte> previous = data_bytes(entry.previous, entry.width);
    const std::span<const std::byte> after = data_bytes(entry.after, entry.width);

    if (entry.officer_id != domain::NO_CITY_OFFICER) {
        const std::uintptr_t slot = (entry.address - state.roster) / OFFICER_STRIDE;
        std::uint16_t id = 0;
        std::uintptr_t force = 0;

        if (entry.address < state.roster || slot >= state.rows.size() ||
            !read_value(state.roster + slot * OFFICER_STRIDE + 8, id) || id != entry.officer_id ||
            !read_value(state.roster + slot * OFFICER_STRIDE + 0x18, force) || force != state.player_force) {
            return false;
        }
    } else {
        if (!governor_owner(state, static_cast<std::uintptr_t>(entry.before)) ||
            !governor_owner(state, static_cast<std::uintptr_t>(entry.after))) { return false; }
        std::uintptr_t corps = state.corps;
        std::uintptr_t force = 0;

        if (entry.address != state.corps + 0x20 &&
            (!read_value(entry.address - 8, corps) || corps != state.corps)) { return false; }
        if (corps != 0 && (!read_value(corps + 0x10, force) || force != state.player_force)) { return false; }
    }
    if (!platform::windows::read_memory(entry.address, std::span(current).first(entry.width))) { return false; }
    if (!std::equal(before.begin(), before.end(), current.begin())) {
        for (std::size_t byte = 0; byte < entry.width; ++byte) {
            if (current[byte] != before[byte] && current[byte] != previous[byte] && current[byte] != after[byte]) { return false; }
        }
        ++state.write_sequence;
        if (!platform::windows::write_data_memory(entry.address, before) ||
            !platform::windows::read_memory(entry.address, std::span(current).first(entry.width)) ||
            !std::equal(before.begin(), before.end(), current.begin())) {
            return false;
        }
    }
    if (entry.officer_id != domain::NO_CITY_OFFICER) {
        const std::size_t slot = (entry.address - state.roster) / OFFICER_STRIDE;
        CorpsRosterEntry& row = state.rows[slot];

        if (entry.address == row.address + 0x10) {
            row.status_pair = entry.width == 1 ? static_cast<std::uint16_t>((row.status_pair & 0xff00) | entry.before)
                : static_cast<std::uint16_t>(entry.before);
        } else if (entry.address == row.address + 0x20) {
            if (row.city < domain::CITY_COUNT && state.snapshot.cities[row.city].current_count != 0) {
                --state.snapshot.cities[row.city].current_count;
            }
            row.city_pointer = static_cast<std::uintptr_t>(entry.before);
            row.city = city_index_from_pointer(state.city_table, row.city_pointer);
            if (row.city < domain::CITY_COUNT) { ++state.snapshot.cities[row.city].current_count; }
        }
    } else if (entry.address >= state.city_table + 0x98 &&
        (entry.address - state.city_table - 0x98) % CITY_STRIDE == 0 &&
        (entry.address - state.city_table - 0x98) / CITY_STRIDE < domain::CITY_COUNT) {
        const std::size_t city = (entry.address - state.city_table - 0x98) / CITY_STRIDE;
        const std::uint16_t row = officer_index(state, static_cast<std::uintptr_t>(entry.before));

        state.city_governors[city] = static_cast<std::uintptr_t>(entry.before);
        state.snapshot.cities[city].current_governor = row == NO_CORPS_ROW ? domain::NO_CITY_OFFICER : state.rows[row].id;
    } else if (entry.address == state.corps + 0x20) {
        const std::uint16_t row = officer_index(state, static_cast<std::uintptr_t>(entry.before));

        for (std::size_t city = 0; city < domain::CITY_COUNT; ++city) {
            if (state.city_corps[city] == state.corps) {
                state.snapshot.cities[city].general = row == NO_CORPS_ROW ? domain::NO_CITY_OFFICER : state.rows[row].id;
            }
        }
    }
    --state.journal_count;
    if (state.journal_count == 0) {
        state.snapshot.moved = state.journal_moved;
        state.snapshot.swapped = state.journal_swapped;
        state.snapshot.appointed = state.journal_appointed;
    }
    return true;
}

bool refresh_corps_live_row(CorpsDeployment& state, std::uint16_t index) {
    if (index >= state.rows.size()) { return false; }
    CorpsRosterEntry& row = state.rows[index];
    std::array<std::byte, 0xf0> bytes{};

    if (row.address != state.roster + index * OFFICER_STRIDE || !read_value(row.address, bytes) ||
        from_record<std::uint16_t>(bytes, 8) != row.id || from_record<std::uintptr_t>(bytes, 0x18) != state.player_force) {
        return false;
    }
    const std::uintptr_t city_pointer = from_record<std::uintptr_t>(bytes, 0x20);
    const std::uint8_t city = city_index_from_pointer(state.city_table, city_pointer);

    if (city == domain::NO_CITY || read_city_force(city_pointer) != state.player_force) { return false; }
    row.status_pair = from_record<std::uint16_t>(bytes, 0x10);
    row.city_pointer = city_pointer;
    row.city = city;
    row.loyalty = from_record<std::uint8_t>(bytes, 0xec);
    return true;
}

domain::CorpsStatus move_corps_officer(CorpsDeployment& state, std::uint16_t index, std::uint8_t target, bool allow_cross_corps) {
    if (index >= state.rows.size() || target >= domain::CITY_COUNT || !refresh_corps_live_row(state, index)) {
        return domain::CorpsStatus::OwnershipChanged;
    }
    CorpsRosterEntry& row = state.rows[index];
    const std::uint8_t status = static_cast<std::uint8_t>(row.status_pair);
    const std::uintptr_t target_pointer = state.city_table + target * CITY_STRIDE;
    std::uintptr_t source_corps = 0;
    std::uintptr_t target_corps = 0;

    if (status == 0xd8 || status == 0xe8 || read_city_force(target_pointer) != state.player_force ||
        !read_value(row.city_pointer + 0x90, source_corps) || !read_value(target_pointer + 0x90, target_corps) ||
        (!allow_cross_corps && (source_corps != state.corps || target_corps != state.corps || (status != 0x18 && status != 0x28))) ||
        (status != 0x18 && status != 0x28 && status != 0x38 && status != 0x48 && status != 0xc8) ||
        (source_corps != target_corps && status != 0x28)) {
        return domain::CorpsStatus::OwnershipChanged;
    }
    if (row.city == target) { return domain::CorpsStatus::Complete; }
    const CorpsDataWrite write{row.address + 0x20, row.city_pointer, target_pointer, index, domain::NO_CITY, 8};
    const domain::CorpsStatus result = write_data(state, std::span(&write, 1));

    if (result == domain::CorpsStatus::Complete) {
        if (state.snapshot.cities[row.city].current_count != 0) { --state.snapshot.cities[row.city].current_count; }
        ++state.snapshot.cities[target].current_count;
        row.city = target;
        row.city_pointer = target_pointer;
        ++state.snapshot.moved;
    }
    return result;
}

domain::CorpsStatus swap_corps_governor(CorpsDeployment& state, std::uint8_t city, std::uint16_t candidate) {
    if (city >= domain::CITY_COUNT || candidate >= state.rows.size() || !same_corps_city(state, city)) {
        return domain::CorpsStatus::OwnershipChanged;
    }
    const std::uintptr_t city_address = state.city_table + city * CITY_STRIDE;
    std::uintptr_t governor = 0;

    if (!read_value(city_address + 0x98, governor)) { return domain::CorpsStatus::ReadFailed; }
    if (governor != state.city_governors[city]) { return domain::CorpsStatus::OwnershipChanged; }
    const std::uint16_t previous = officer_index(state, governor);

    if (previous == NO_CORPS_ROW || previous == candidate || !refresh_corps_live_row(state, previous) || !refresh_corps_live_row(state, candidate)) {
        return domain::CorpsStatus::OwnershipChanged;
    }
    CorpsRosterEntry& old = state.rows[previous];
    CorpsRosterEntry& next = state.rows[candidate];

    if (static_cast<std::uint8_t>(old.status_pair) != 0xe8 || static_cast<std::uint8_t>(next.status_pair) != 0x28 ||
        old.city != city || next.city != city) { return domain::CorpsStatus::OwnershipChanged; }
    const std::uint16_t old_after = (next.status_pair & 0xff00) | 0x28;
    const std::uint16_t next_after = (old.status_pair & 0xff00) | 0xe8;
    const std::array<CorpsDataWrite, 3> writes{{
        {old.address + 0x10, old.status_pair, old_after, previous, domain::NO_CITY, 2, true},
        {next.address + 0x10, next.status_pair, next_after, candidate, domain::NO_CITY, 2, true},
        {city_address + 0x98, governor, next.address, NO_CORPS_ROW, city, 8}}};
    const domain::CorpsStatus result = write_data(state, writes);

    if (result == domain::CorpsStatus::Complete) {
        old.status_pair = old_after;
        next.status_pair = next_after;
        state.city_governors[city] = next.address;
        state.snapshot.cities[city].current_governor = next.id;
        ++state.snapshot.swapped;
    }
    return result;
}

domain::CorpsStatus appoint_corps_governor(CorpsDeployment& state, std::uint8_t city, std::uint16_t candidate) {
    if (city >= domain::CITY_COUNT || candidate >= state.rows.size() || !same_corps_city(state, city) || !refresh_corps_live_row(state, candidate)) {
        return domain::CorpsStatus::OwnershipChanged;
    }
    const std::uintptr_t city_address = state.city_table + city * CITY_STRIDE;
    std::uintptr_t governor = 0;
    CorpsRosterEntry& next = state.rows[candidate];

    if (!read_value(city_address + 0x98, governor) || governor != 0 || next.city != city || next.loyalty != 100 || next.status_pair != 0xd328) {
        return domain::CorpsStatus::OwnershipChanged;
    }
    constexpr std::uint16_t APPOINTED_STATUS = 0xd2e8;
    const std::array<CorpsDataWrite, 2> writes{{
        {next.address + 0x10, next.status_pair, APPOINTED_STATUS, candidate, domain::NO_CITY, 2, true},
        {city_address + 0x98, 0, next.address, NO_CORPS_ROW, city, 8}}};
    const domain::CorpsStatus result = write_data(state, writes);

    if (result == domain::CorpsStatus::Complete) {
        next.status_pair = APPOINTED_STATUS;
        state.city_governors[city] = next.address;
        state.snapshot.cities[city].current_governor = next.id;
        ++state.snapshot.appointed;
    }
    return result;
}

domain::CorpsStatus swap_corps_general(CorpsDeployment& state, std::uint16_t previous, std::uint16_t candidate) {
    if (state.corps <= 0x10000 || previous >= state.rows.size() || candidate >= state.rows.size() || previous == candidate ||
        !refresh_corps_live_row(state, previous) || !refresh_corps_live_row(state, candidate)) {
        return domain::CorpsStatus::OwnershipChanged;
    }
    CorpsRosterEntry& old = state.rows[previous];
    CorpsRosterEntry& next = state.rows[candidate];
    std::uintptr_t general = 0;
    std::uintptr_t force = 0;

    if (static_cast<std::uint8_t>(old.status_pair) != 0xd8 || static_cast<std::uint8_t>(next.status_pair) != 0xe8 ||
        !same_corps_city(state, old.city) || !same_corps_city(state, next.city) || !read_value(state.corps + 0x20, general) ||
        general != old.address || !read_value(state.corps + 0x10, force) || force != state.player_force) {
        return domain::CorpsStatus::OwnershipChanged;
    }
    // Unlike a governor exchange, a general exchange changes only +0x10;
    // each officer's auxiliary byte at +0x11 stays unchanged.
    const std::array<CorpsDataWrite, 3> writes{{
        {old.address + 0x10, 0xd8, 0xe8, previous, domain::NO_CITY, 1, true},
        {next.address + 0x10, 0xe8, 0xd8, candidate, domain::NO_CITY, 1, true},
        {state.corps + 0x20, general, next.address, NO_CORPS_ROW, domain::NO_CITY, 8, false, true}}};
    const domain::CorpsStatus result = write_data(state, writes);

    if (result == domain::CorpsStatus::Complete) {
        old.status_pair = (old.status_pair & 0xff00) | 0xe8;
        next.status_pair = (next.status_pair & 0xff00) | 0xd8;
        for (std::size_t city = 0; city < domain::CITY_COUNT; ++city) {
            if (state.city_corps[city] == state.corps) { state.snapshot.cities[city].general = next.id; }
        }
        ++state.snapshot.swapped;
    }
    return result;
}

} // namespace s8::game
