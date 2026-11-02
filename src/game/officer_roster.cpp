#include "officer_roster.h"
#include "../domain/auto_affinity.h"
#include "../platform/windows/process_memory.h"
#include "../platform/windows/speed_clock.h"

#include <algorithm>
#include <bit>
#include <bitset>
#include <chrono>
#include <cstring>
#include <limits>
#include <span>
#include <utility>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

namespace s8::game {
namespace {

std::uintptr_t city_array_base(std::uintptr_t module_base) {
    std::uintptr_t root = 0, next = 0, base = 0;
    if (!platform::windows::read_memory(module_base + 0x034c8630, std::as_writable_bytes(std::span(&root, 1))) ||
        root <= 0x10000 || !platform::windows::read_memory(root, std::as_writable_bytes(std::span(&next, 1))) ||
        next <= 0x10000 || !platform::windows::read_memory(next, std::as_writable_bytes(std::span(&base, 1)))) {
        return 0;
    }
    return base > 0x10000 ? base : 0;
}

std::uintptr_t roster_base(std::uintptr_t module_base) {
    if (!module_base) {
        return 0;
    }
    std::uintptr_t roster = 0;
    if (!platform::windows::read_memory(module_base + 0x034c8630, std::as_writable_bytes(std::span(&roster, 1))) ||
        roster < 0x10000) {
        return 0;
    }
    for (const auto offset : std::array<std::uintptr_t, 5>{0x48, 8, 0x10, 0, 8}) {
        std::uintptr_t next = 0;
        if (!platform::windows::read_memory(roster + offset, std::as_writable_bytes(std::span(&next, 1))) ||
            next < 0x10000) {
            return 0;
        }
        roster = next;
    }
    if (!platform::windows::is_committed_memory(roster, 0x3d0)) {
        return 0;
    }
    return roster;
}

std::uintptr_t roster_officer(const OfficerRosterScan& scan, std::uintptr_t module_base, std::uint16_t id) {
    const std::uintptr_t officer = scan.address(id);

    if (!officer) {
        return 0;
    }
    const std::uintptr_t roster = roster_base(module_base);

    if (!roster || roster != scan.base) {
        return 0;
    }
    std::uint16_t current_id = 0;
    if (!platform::windows::read_memory(officer + 8, std::as_writable_bytes(std::span(&current_id, 1))) ||
        current_id != id) {
        return 0;
    }
    return officer;
}

} // namespace

std::uintptr_t resolve_officer_roster_base(std::uintptr_t module_base) {
    return roster_base(module_base);
}

std::uintptr_t resolve_trait_target(const OfficerRosterScan& scan, std::uintptr_t module_base,
                                    std::uint16_t id, std::uintptr_t player) {
    if (!player) {
        return roster_officer(scan, module_base, id);
    }
    std::uint16_t actual_id = 0;
    return platform::windows::read_memory(player + 8, std::as_writable_bytes(std::span(&actual_id, 1))) && actual_id == id
        ? player : 0;
}

void scan_trait_object(TraitObjectSearch& search, std::size_t max_bytes, const domain::WorkBudget& budget) {
    if (search.complete || budget.exhausted()) {
        return;
    }
    SYSTEM_INFO system{};

    GetSystemInfo(&system);

    const std::uintptr_t minimum = reinterpret_cast<std::uintptr_t>(system.lpMinimumApplicationAddress);
    const std::uintptr_t maximum = reinterpret_cast<std::uintptr_t>(system.lpMaximumApplicationAddress);

    if (search.cursor < minimum) {
        search.cursor = minimum;
    }
    std::size_t scanned = 0;

    while (search.cursor < maximum && scanned < max_bytes) {
        if (budget.exhausted()) {
            return;
        }
        MEMORY_BASIC_INFORMATION region{};

        if (!VirtualQuery(reinterpret_cast<const void*>(search.cursor), &region, sizeof(region))) {
            search.complete = true;
            return;
        }
        const std::uintptr_t end = reinterpret_cast<std::uintptr_t>(region.BaseAddress) + region.RegionSize;
        const DWORD protection = region.Protect & 0xff;
        const bool readable = protection == PAGE_READONLY || protection == PAGE_READWRITE ||
            protection == PAGE_WRITECOPY || protection == PAGE_EXECUTE_READ ||
            protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY;

        if (region.State != MEM_COMMIT || !readable || (region.Protect & PAGE_GUARD)) {
            search.cursor = end;
            continue;
        }
        search.cursor = (search.cursor + 7) & ~std::uintptr_t{7};
        if (search.cursor + 10 > end) {
            search.cursor = end;
            continue;
        }
        const std::size_t amount = std::min({TraitObjectSearch::BLOCK_SIZE, max_bytes - scanned,
            static_cast<std::size_t>(end - search.cursor - 2)});
        const std::size_t candidates = amount / 8;

        if (!candidates) {
            break;
        }
        if (budget.exhausted()) {
            return;
        }
        const std::size_t length = candidates * 8 + 2;

        if (platform::windows::read_memory(search.cursor, std::span(search.bytes).first(length))) {
            for (std::size_t index = 0; index < candidates; ++index) {
                if (index != 0 && index % 64 == 0 && budget.exhausted()) {
                    // Only inspected candidates are retired. The next slice
                    // resumes at the first unexamined pointer, including its ID.
                    // Complete one small comparison slice after an admitted
                    // read, so a slow ReadProcessMemory cannot prevent progress.
                    search.cursor += index * 8;
                    return;
                }
                std::uintptr_t vtable = 0;
                std::uint16_t id = 0;

                std::memcpy(&vtable, search.bytes.data() + index * 8, sizeof(vtable));
                if (vtable == search.vtable) {
                    std::memcpy(&id, search.bytes.data() + index * 8 + 8, sizeof(id));
                    if (id == search.trait_id) {
                        search.result = search.cursor + index * 8;
                        search.complete = true;
                        return;
                    }
                }
            }
        }
        search.cursor += candidates * 8;
        scanned += candidates * 8;
    }
    search.complete = search.cursor >= maximum;
}

void reset_officer_roster(OfficerRosterScan& scan) {
    scan.base = 0;
    scan.cursor = 0;
    scan.slots.fill(0);
    scan.entries.fill({});
    scan.dirty.fill(0);
    scan.dirty_count = 0;
    scan.publication_word = 0;
    ++scan.generation;
    scan.publication_epoch.reset();
    scan.publication_pending = false;
    scan.reset_pending = true;
    scan.has_scan = false;
    scan.requested = false;
    scan.completed = false;
}

void request_officer_roster(OfficerRosterScan& scan) {
    if (!scan.has_scan) {
        reset_officer_roster(scan);
    }
    scan.cursor = 0;
    scan.has_scan = true;
    scan.requested = true;
    scan.completed = false;
}

namespace {

void mark_roster_dirty(OfficerRosterScan& scan, std::uint16_t id) {
    const std::uint64_t bit = std::uint64_t{1} << (id % 64);
    std::uint64_t& word = scan.dirty[id / 64];

    if ((word & bit) == 0) {
        word |= bit;
        ++scan.dirty_count;
    }
}

} // namespace

const domain::OfficerRosterEntry* OfficerRosterScan::find(std::uint16_t id) const {
    return id < entries.size() && slots[id] != 0 ? &entries[id] : nullptr;
}

std::uintptr_t OfficerRosterScan::address(std::uint16_t id) const {
    return find(id) ? base + (slots[id] - 1) * RECORD_SIZE : 0;
}

void OfficerRosterScan::refresh(std::uintptr_t module_base, const domain::WorkBudget& budget) {
    if (!requested || budget.exhausted()) {
        return;
    }

    const std::uintptr_t roster = roster_base(module_base);

    if (!roster) {
        return;
    }

    // The existing chain points to array index 1 (ID 1). Index 0 is ID 0;
    // the allocation cookie precedes it. Admit this layout at the read boundary.
    std::uint64_t allocation_count = 0;

    if (!platform::windows::read_memory(roster - RECORD_SIZE - sizeof(allocation_count),
            std::as_writable_bytes(std::span(&allocation_count, 1))) ||
        allocation_count != domain::OFFICER_ROSTER_RECORD_COUNT + 1) {
        return;
    }

    snapshot.resize(domain::OFFICER_ROSTER_RECORD_COUNT * RECORD_SIZE);

    if (!platform::windows::read_memory(roster, std::span(snapshot))) {
        return;
    }

    std::array<std::uint16_t, domain::OFFICER_ROSTER_ID_SLOTS> observed_slots{};
    std::array<domain::OfficerRosterEntry, domain::OFFICER_ROSTER_RECORD_COUNT> observed{};

    // Validate the incoming snapshot before changing the published cache.
    for (std::size_t slot = 0; slot < observed.size(); ++slot) {
        std::uint16_t id = 0;
        const std::byte* record = snapshot.data() + slot * RECORD_SIZE;

        std::memcpy(&id, record + 8, sizeof(id));
        if (id == 0 || id >= observed_slots.size() || observed_slots[id] != 0) {
            return;
        }

        observed_slots[id] = static_cast<std::uint16_t>(slot + 1);
        observed[slot] = {id, std::to_integer<std::uint8_t>(record[0x10])};
    }

    if (base != roster || slots != observed_slots) {
        reset_officer_roster(*this);
        base = roster;
        slots = observed_slots;
        has_scan = true;
    }

    for (const domain::OfficerRosterEntry& row : observed) {
        domain::OfficerRosterEntry& cached = entries[row.id];

        if (cached.id != row.id || cached.status != row.status) {
            cached = row;
            mark_roster_dirty(*this, row.id);
        }
    }

    cursor = observed.size();
    requested = false;
    completed = true;
}

void update_officer_roster(OfficerRosterScan& scan, std::uintptr_t module_base, const domain::WorkBudget& budget) {
    scan.refresh(module_base, budget);
}

bool update_roster_officer_status(OfficerRosterScan& scan, std::uint16_t id, std::uint8_t status) {
    if (id == 0 || id > domain::OFFICER_ROSTER_LIMIT || scan.entries[id].id != id) {
        return false;
    }
    if (scan.entries[id].status != status) {
        scan.entries[id].status = status;
        mark_roster_dirty(scan, id);
    }
    return true;
}

const domain::OfficerRosterUpdate* peek_officer_roster_update(OfficerRosterScan& scan, domain::GameEpoch epoch) {
    if (scan.publication_epoch && *scan.publication_epoch != epoch) {
        // A caller that misses the explicit epoch reset must still never label
        // the old roster's rows as belonging to the new game session.
        reset_officer_roster(scan);
    }
    scan.publication_epoch = epoch;
    if (scan.publication_pending) {
        return &scan.publication;
    }
    domain::OfficerRosterUpdate& update = scan.publication;

    update.epoch = epoch;
    update.generation = scan.generation;
    update.count = 0;
    update.available = scan.has_scan;
    update.loading = scan.has_scan && (scan.requested || scan.completed);
    if (scan.reset_pending) {
        update.kind = domain::OfficerRosterUpdateKind::Reset;
        scan.publication_pending = true;
        return &update;
    }
    if (scan.dirty_count != 0) {
        update.kind = domain::OfficerRosterUpdateKind::Rows;

        for (std::size_t offset = 0; offset < scan.dirty.size(); ++offset) {
            const std::size_t word_index = (scan.publication_word + offset) % scan.dirty.size();
            std::uint64_t bits = scan.dirty[word_index];

            while (bits != 0) {
                const std::size_t bit_index = static_cast<std::size_t>(std::countr_zero(bits));
                const std::size_t id = word_index * 64 + bit_index;

                bits &= bits - 1;
                if (id != 0 && id <= domain::OFFICER_ROSTER_LIMIT) {
                    update.entries[update.count++] = scan.entries[id];
                }
                if (update.count == update.entries.size()) {
                    scan.publication_word = word_index;
                    scan.publication_pending = true;
                    return &update;
                }
            }
        }
        scan.publication_pending = update.count != 0;
        return scan.publication_pending ? &update : nullptr;
    }
    if (scan.completed) {
        update.kind = domain::OfficerRosterUpdateKind::Complete;
        update.loading = false;
        scan.publication_pending = true;
        return &update;
    }
    return nullptr;
}

void acknowledge_officer_roster_update(OfficerRosterScan& scan) {
    if (!scan.publication_pending) {
        return;
    }
    const domain::OfficerRosterUpdate& update = scan.publication;

    if (update.kind == domain::OfficerRosterUpdateKind::Reset) {
        scan.reset_pending = false;
    } else if (update.kind == domain::OfficerRosterUpdateKind::Complete) {
        scan.completed = false;
    } else {
        for (std::size_t index = 0; index < update.count; ++index) {
            const domain::OfficerRosterEntry& row = update.entries[index];
            const std::uint64_t bit = std::uint64_t{1} << (row.id % 64);
            std::uint64_t& word = scan.dirty[row.id / 64];

            // A command may have changed this row while the full channel held
            // an older packet. Retain the newer value for the next publication.
            if (scan.entries[row.id].status == row.status && (word & bit) != 0) {
                word &= ~bit;
                --scan.dirty_count;
            }
        }
    }
    scan.publication_pending = false;
}

domain::OfficerTraits read_officer_traits(std::uintptr_t officer) {
    domain::OfficerTraits traits{};
    for (std::size_t slot = 0; slot < traits.size(); ++slot) {
        std::uintptr_t trait_address = 0;
        if (!platform::windows::read_memory(officer + 0x88 + slot * sizeof(trait_address),
                std::as_writable_bytes(std::span(&trait_address, 1))) || trait_address < 0x10000) {
            continue;
        }
        domain::Trait trait{};
        if (!platform::windows::read_memory(trait_address + 8, std::as_writable_bytes(std::span(&trait.id, 1)))) {
            continue;
        }
        bool valid = true;
        for (std::size_t i = 0; i < trait.effects.size(); ++i) {
            std::array<std::uint16_t, 3> values{};
            if (!platform::windows::read_memory(trait_address + 0x0a + i * 6,
                    std::as_writable_bytes(std::span(values)))) {
                valid = false;
                break;
            }
            if (values[0] == 0) {
                break;
            }
            trait.effects[i] = {values[0], values[1], values[2]};
        }
        if (valid) {
            traits[slot] = trait;
        }
    }
    return traits;
}

std::optional<domain::OfficerDetail> read_roster_officer(const OfficerRosterScan& scan,
                                                         std::uintptr_t module_base, std::uint16_t id) {
    const auto officer = roster_officer(scan, module_base, id);
    if (!officer) {
        return std::nullopt;
    }
    domain::OfficerDetail detail{};
    detail.id = id;
    if (!platform::windows::read_memory(officer + 0x10, std::as_writable_bytes(std::span(&detail.status, 1))) ||
        !platform::windows::read_memory(officer + 0x30, std::as_writable_bytes(std::span(&detail.gender, 1))) ||
        !platform::windows::read_memory(officer + 0x60, std::as_writable_bytes(std::span(&detail.strategic_tendency, 1))) ||
        !platform::windows::read_memory(officer + 0x64, std::as_writable_bytes(std::span(&detail.personality, 1))) ||
        !platform::windows::read_memory(officer + 0x68, std::as_writable_bytes(std::span(&detail.voice, 1))) ||
        !platform::windows::read_memory(officer + 0x82, std::as_writable_bytes(std::span(&detail.greed, 1))) ||
        !platform::windows::read_memory(officer + 0x83, std::as_writable_bytes(std::span(&detail.interests, 1))) ||
        !platform::windows::read_memory(officer + 0xa4, std::as_writable_bytes(std::span(&detail.priority, 1))) ||
        !platform::windows::read_memory(officer + 0x139, std::as_writable_bytes(std::span(detail.tactic_levels))) ||
        !platform::windows::read_memory(officer + 0x1d0, std::as_writable_bytes(std::span(detail.skill_levels))) ||
        !platform::windows::read_memory(officer + 0xc4, std::as_writable_bytes(std::span(detail.aptitudes)))) {
        return std::nullopt;
    }
    for (std::size_t i = 0; i < detail.basic.size(); ++i) {
        const auto field = domain::officer_basic_fields[i];
        if (!platform::windows::read_memory(officer + field.offset,
                std::as_writable_bytes(std::span(&detail.basic[i], 1)).first(field.size))) {
            return std::nullopt;
        }
    }
    for (std::size_t i = 0; i < detail.family.size(); ++i) {
        std::uintptr_t relative = 0;
        if (platform::windows::read_memory(officer + 0x40 + i * sizeof(relative),
                std::as_writable_bytes(std::span(&relative, 1)))) {
            relative &= 0x0000ffffffffffffull;
            if (relative > 0x10000) {
                std::uint16_t relative_id = 0;
                if (platform::windows::read_memory(relative + 8, std::as_writable_bytes(std::span(&relative_id, 1))) &&
                    relative_id >= 1 && relative_id <= 5102) {
                    detail.family[i] = relative_id;
                }
            }
        }
    }
    detail.traits = read_officer_traits(officer);
    const auto city_base = city_array_base(module_base);
    std::uintptr_t city = 0;
    if (city_base && platform::windows::read_memory(officer + 0x20, std::as_writable_bytes(std::span(&city, 1))) &&
        city >= city_base && (city - city_base) / 0x2a0 < 51) {
        detail.city = static_cast<std::uint8_t>((city - city_base) / 0x2a0);
        std::uintptr_t corps = 0;
        if (platform::windows::read_memory(city + 0x90, std::as_writable_bytes(std::span(&corps, 1))) && corps > 0x10000) {
            std::uint64_t number = 0;
            std::uintptr_t governor = 0;
            platform::windows::read_memory(corps + 0x18, std::as_writable_bytes(std::span(&number, 1)));
            detail.corps_number = number;
            if (platform::windows::read_memory(corps + 0x20, std::as_writable_bytes(std::span(&governor, 1))) && governor > 0x10000) {
                std::uint16_t governor_id = 0;
                if (platform::windows::read_memory(governor + 8, std::as_writable_bytes(std::span(&governor_id, 1))) &&
                    governor_id >= 1 && governor_id <= 5102) {
                    detail.corps_governor = governor_id;
                }
            }
        }
    }
    return detail;
}

bool roster_traits_changed(const OfficerRosterScan& scan, std::uintptr_t module_base,
                           const domain::OfficerDetail& detail) {
    const auto officer = roster_officer(scan, module_base, detail.id);
    if (!officer) {
        return false;
    }
    for (std::size_t slot = 0; slot < detail.traits.size(); ++slot) {
        std::uintptr_t address = 0;
        std::uint16_t id = 0;
        if (platform::windows::read_memory(officer + 0x88 + slot * sizeof(address),
                std::as_writable_bytes(std::span(&address, 1))) && address >= 0x10000) {
            platform::windows::read_memory(address + 8, std::as_writable_bytes(std::span(&id, 1)));
        }
        const auto cached = detail.traits[slot] ? detail.traits[slot]->id : 0;
        if (id != cached) {
            return true;
        }
    }
    return false;
}

bool write_roster_officer_basic(const OfficerRosterScan& scan, std::uintptr_t module_base,
                                std::uint16_t id, std::uint8_t index, std::uint32_t value) {
    if (index >= domain::officer_basic_fields.size()) {
        return false;
    }
    const auto officer = roster_officer(scan, module_base, id);
    if (!officer) {
        return false;
    }
    const auto field = domain::officer_basic_fields[index];
    return platform::windows::write_data_memory(officer + field.offset,
        std::as_bytes(std::span(&value, 1)).first(field.size));
}

bool write_roster_officer_gender(const OfficerRosterScan& scan, std::uintptr_t module_base,
                                 std::uint16_t id, std::uint8_t gender) {
    if (gender > 1) {
        return false;
    }
    const auto officer = roster_officer(scan, module_base, id);
    return officer && platform::windows::write_data_memory(officer + 0x30, std::as_bytes(std::span(&gender, 1)));
}

bool write_roster_officer_voice(const OfficerRosterScan& scan, std::uintptr_t module_base,
                                std::uint16_t id, std::uint8_t voice) {
    const auto officer = roster_officer(scan, module_base, id);
    if (!officer) {
        return false;
    }
    const auto bytes = std::as_bytes(std::span(&voice, 1));
    const bool first = platform::windows::write_data_memory(officer + 0x68, bytes);
    const bool second = platform::windows::write_data_memory(officer + 0x70, bytes);
    return first && second;
}

bool write_roster_officer_priority(const OfficerRosterScan& scan, std::uintptr_t module_base,
                                  std::uint16_t id, std::uint8_t priority) {
    if (priority < 1 || priority > 6) {
        return false;
    }
    const std::uintptr_t officer = roster_officer(scan, module_base, id);

    return officer && platform::windows::write_data_memory(officer + 0xa4,
        std::as_bytes(std::span(&priority, 1)));
}

bool set_roster_officer_ronin(const OfficerRosterScan& scan, std::uintptr_t module_base,
                              std::uint16_t id, bool undiscovered_only) {
    const auto officer = roster_officer(scan, module_base, id);
    std::uint8_t status = 0;
    if (!officer || !platform::windows::read_memory(officer + 0x10, std::as_writable_bytes(std::span(&status, 1))) ||
        (undiscovered_only && status != 0x68 && status != 0x78)) {
        return false;
    }
    constexpr std::uint8_t ronin = 0x58;
    return platform::windows::write_data_memory(officer + 0x10, std::as_bytes(std::span(&ronin, 1)));
}

bool move_roster_officer(const OfficerRosterScan& scan, std::uintptr_t module_base,
                         std::uint16_t id, std::uint8_t city) {
    const auto officer = roster_officer(scan, module_base, id);
    const auto base = city_array_base(module_base);
    std::uint8_t status = 0;
    if (!officer || !base || city >= 51 ||
        !platform::windows::read_memory(officer + 0x10, std::as_writable_bytes(std::span(&status, 1))) || status != 0x58) {
        return false;
    }
    const auto target = base + city * 0x2a0;
    return platform::windows::write_data_memory(officer + 0x20, std::as_bytes(std::span(&target, 1)));
}

std::uintptr_t resolve_officer_trait_vtable(std::uintptr_t officer, std::uintptr_t game_base) {
    if (!officer || game_base <= 0x10000) {
        return 0;
    }
    for (std::size_t i = 0; i < 75; ++i) {
        std::uintptr_t trait = 0, vtable = 0;
        std::uint16_t actual_id = 0;
        const auto expected = i < 3 ? 0 : i < 73 ? static_cast<std::uint16_t>(i - 2)
                                                        : static_cast<std::uint16_t>(i + 128);
        const auto address = i < 3 ? officer + 0x88 + i * sizeof(trait)
                                   : game_base + 0x57a4d0 + expected * sizeof(trait);
        if (platform::windows::read_memory(address, std::as_writable_bytes(std::span(&trait, 1))) && trait > 0x10000 &&
            platform::windows::read_memory(trait, std::as_writable_bytes(std::span(&vtable, 1))) && vtable > 0x10000 &&
            platform::windows::read_memory(trait + 8, std::as_writable_bytes(std::span(&actual_id, 1))) &&
            actual_id != 0 && (!expected || actual_id == expected)) {
            return vtable;
        }
    }
    return 0;
}

bool set_officer_trait_object(std::uintptr_t officer, std::uint8_t slot, std::uint16_t trait_id,
                              std::uintptr_t trait, std::uintptr_t expected_vtable) {
    if (!officer || slot >= 3 || !trait_id || trait <= 0x10000) {
        return false;
    }
    std::uintptr_t vtable = 0;
    std::uint16_t actual_id = 0;
    if (!platform::windows::read_memory(trait, std::as_writable_bytes(std::span(&vtable, 1))) || vtable <= 0x10000 ||
        (expected_vtable && vtable != expected_vtable) ||
        !platform::windows::read_memory(trait + 8, std::as_writable_bytes(std::span(&actual_id, 1))) || actual_id != trait_id) {
        return false;
    }
    const auto slot_address = officer + 0x88 + slot * sizeof(trait);
    if (!platform::windows::write_data_memory(slot_address, std::as_bytes(std::span(&trait, 1)))) {
        return false;
    }
    std::uintptr_t written = 0;
    return platform::windows::read_memory(slot_address, std::as_writable_bytes(std::span(&written, 1))) &&
           written == trait && platform::windows::read_memory(written + 8, std::as_writable_bytes(std::span(&actual_id, 1))) &&
           actual_id == trait_id;
}

bool set_officer_trait_direct(std::uintptr_t officer, std::uintptr_t game_base,
                              std::uint8_t slot, std::uint16_t trait_id) {
    std::uintptr_t trait = 0;
    return game_base > 0x10000 && trait_id &&
        platform::windows::read_memory(game_base + 0x57a4d0 + trait_id * sizeof(trait),
                                      std::as_writable_bytes(std::span(&trait, 1))) &&
        set_officer_trait_object(officer, slot, trait_id, trait, 0);
}

bool revive_roster_officer(const OfficerRosterScan& scan, std::uintptr_t module_base,
                           std::uintptr_t player, std::uint16_t id, std::optional<std::uint8_t> city) {
    const auto officer = roster_officer(scan, module_base, id);
    std::uint8_t status = 0;
    if (!officer || !platform::windows::read_memory(officer + 0x10, std::as_writable_bytes(std::span(&status, 1))) ||
        status != 0x88) {
        return false;
    }
    const auto base = city_array_base(module_base);
    std::uintptr_t target = 0;
    if (city) {
        if (!base || *city >= 51) {
            return false;
        }
        target = base + *city * 0x2a0;
    } else if (player && base) {
        std::uintptr_t player_city = 0;
        if (platform::windows::read_memory(player + 0x20, std::as_writable_bytes(std::span(&player_city, 1))) &&
            player_city >= base) {
            target = player_city;
        }
    }
    constexpr std::uint8_t ronin = 0x58, action = 200;
    constexpr std::uint16_t death_year = 255;
    constexpr std::uint32_t death_flag = 0;
    bool success = platform::windows::write_data_memory(officer + 0x10, std::as_bytes(std::span(&ronin, 1)));
    success &= platform::windows::write_data_memory(officer + 0x36, std::as_bytes(std::span(&death_year, 1)));
    success &= platform::windows::write_data_memory(officer + 0xee, std::as_bytes(std::span(&action, 1)));
    success &= platform::windows::write_data_memory(officer + 0x374, std::as_bytes(std::span(&death_flag, 1)));
    if (target) {
        success &= platform::windows::write_data_memory(officer + 0x20, std::as_bytes(std::span(&target, 1)));
    }
    return success;
}

bool write_roster_officer_training(const OfficerRosterScan& scan, std::uintptr_t module_base,
                                  std::uint16_t id, domain::OfficerTrainingKind kind,
                                  std::uint8_t index, std::uint8_t value) {
    std::uintptr_t offset = 0;
    switch (kind) {
    case domain::OfficerTrainingKind::Tactic:
        if (index >= 35 || value > 3) {
            return false;
        }
        offset = 0x139 + index;
        break;
    case domain::OfficerTrainingKind::Skill:
        if (index >= 24 || value > 3) {
            return false;
        }
        offset = 0x1d0 + index;
        break;
    case domain::OfficerTrainingKind::Aptitude:
        if (index >= 11) {
            return false;
        }
        offset = 0xc4 + index;
        break;
    default:
        return false;
    }
    const auto officer = roster_officer(scan, module_base, id);
    return officer && platform::windows::write_data_memory(officer + offset, std::as_bytes(std::span(&value, 1)));
}

namespace {

using RelationKind = domain::OfficerRelationKind;
using RelationStatus = domain::OfficerRelationsStatus;
constexpr std::uintptr_t OFFICER_STRIDE = 0x3d0;
constexpr std::uintptr_t OFFICER_POINTER_MASK = 0x0000ffffffffffffULL;
constexpr std::uintptr_t DIRECT_POINTER_TABLE = 0x57ae38;
constexpr std::array<std::uintptr_t, 2> SYNERGY_OFFSETS{0x547330, 0x433210};
constexpr std::array<RelationKind, 4> DIRECT_KINDS{
    RelationKind::Sworn, RelationKind::Spouse, RelationKind::Enemy, RelationKind::Rival};

enum class InspectionStage {
    Idle, Publish
};

struct InspectionIdentity {
    domain::GameEpoch epoch{};
    std::uintptr_t module = 0;
    std::uintptr_t root_pointer = 0;
    std::uintptr_t root = 0;
    std::uintptr_t roster = 0;
    std::uintptr_t officer = 0;
    std::uint16_t id = 0;
    bool protagonist = false;
    bool registered = false;
    bool operator==(const InspectionIdentity&) const = default;
};

struct FactionObservation {
    InspectionIdentity owner;
    std::uintptr_t force = 0;
    std::uintptr_t vtable = 0;
    std::uint8_t color = 0;
};

struct FactionColorRestore {
    FactionObservation previous;
    std::uint8_t replacement = 0;
    bool pending = false;
};

template<class T> bool inspect_value(std::uintptr_t address, T& value) {
    return platform::windows::read_memory(address, std::as_writable_bytes(std::span(&value, 1)));
}

template<class T, std::size_t N> T inspection_field(const std::array<std::byte, N>& bytes,
    std::size_t offset) {
    T value{};
    std::memcpy(&value, bytes.data() + offset, sizeof(value));
    return value;
}

std::optional<InspectionIdentity> inspection_identity(const OfficerRosterScan&,
    const OfficerInspectionContext& context, std::uint16_t id, bool protagonist) {
    // A confirmed absent scenario selects registration data. A broken live
    // scenario never falls back to editing the registration template.
    if (context.game_base == 0 && context.player == 0 && !protagonist && context.module_base != 0) {
        std::uintptr_t observed_root = 0;
        if (context.game_base_pointer &&
            (!inspect_value(context.game_base_pointer, observed_root) || observed_root != 0)) {
            return std::nullopt;
        }
        std::uintptr_t manager = 0;
        std::uintptr_t registered = 0;
        std::uintptr_t slot = 0;
        if (id >= 3001 && id <= 3300) slot = 8 + (id - 3001) * 8;
        else if (id >= 5101 && id <= 5200) slot = 0x12c8 + (id - 5101) * 8;
        else if (id >= 2001 && id <= 2100) slot = 0x15e8 + (id - 2001) * 8;
        else return std::nullopt;

        std::uint16_t observed_id = 0;
        if (!inspect_value(context.module_base + 0x2ea6840, manager) || !manager ||
            !inspect_value(manager + slot, registered) || !registered ||
            !inspect_value(registered + 8, observed_id) || observed_id != id) {
            return std::nullopt;
        }
        return InspectionIdentity{context.epoch, context.module_base, context.game_base_pointer,
            0, manager, registered, id, false, true};
    }
    std::uintptr_t root = 0;
    if (id == 0 || id > domain::OFFICER_RELATION_CAPACITY || context.module_base == 0 ||
        context.module_base > std::numeric_limits<std::uintptr_t>::max() - 0x34c8630 ||
        context.game_base <= 0x10000 ||
        context.game_base > std::numeric_limits<std::uintptr_t>::max() - 0x800000 ||
        !context.game_base_pointer || !inspect_value(context.game_base_pointer, root) ||
        root != context.game_base) {
        return std::nullopt;
    }
    const std::uintptr_t roster = roster_base(context.module_base);
    std::uintptr_t officer = 0;
    if (protagonist) {
        if (!inspect_value(root + 0xe0, officer) || officer != context.player) {
            return std::nullopt;
        }
    } else {
        std::uintptr_t center = 0;
        if (!inspect_value(context.module_base + 0x2e98bc8, center) || !center ||
            !inspect_value(center + 0x21bdf0 + id * sizeof(std::uintptr_t), officer)) {
            return std::nullopt;
        }
    }
    std::uint16_t observed_id = 0;
    if (roster <= 0x10000 ||
        roster > std::numeric_limits<std::uintptr_t>::max() - domain::OFFICER_RELATION_CAPACITY * OFFICER_STRIDE ||
        officer <= 0x10000 || officer > std::numeric_limits<std::uintptr_t>::max() - OFFICER_STRIDE ||
        !inspect_value(officer + 8, observed_id) || observed_id != id) {
        return std::nullopt;
    }
    return InspectionIdentity{context.epoch, context.module_base, context.game_base_pointer,
        root, roster, officer, id, protagonist};
}

bool same_inspection_owner(const InspectionIdentity& expected, const OfficerRosterScan& scan,
    const OfficerInspectionContext& context) {
    if (expected.epoch != context.epoch) {
        return false;
    }
    const std::optional<InspectionIdentity> current = inspection_identity(scan, context,
        expected.id, expected.protagonist);
    return current && *current == expected;
}

std::optional<FactionObservation> inspect_faction(const InspectionIdentity& owner) {
    FactionObservation result;
    result.owner = owner;
    if (!inspect_value(owner.officer + 0x18, result.force)) {
        return std::nullopt;
    }
    if (result.force == 0) {
        return result;
    }
    if (result.force <= 0x10000 || result.force > std::numeric_limits<std::uintptr_t>::max() - 0x10 ||
        !inspect_value(result.force, result.vtable) || result.vtable <= 0x10000 ||
        !inspect_value(result.force + 9, result.color)) {
        return std::nullopt;
    }
    return result;
}

bool same_faction(const FactionObservation& left, const FactionObservation& right) {
    return left.owner == right.owner && left.force == right.force && left.vtable == right.vtable;
}

constexpr std::size_t relation_index(RelationKind kind) {
    return static_cast<std::size_t>(kind);
}

bool inspection_roster_pointer(std::uintptr_t pointer, std::uintptr_t roster) {
    pointer &= OFFICER_POINTER_MASK;
    return pointer >= roster && pointer - roster < domain::OFFICER_RELATION_CAPACITY * OFFICER_STRIDE &&
        (pointer - roster) % OFFICER_STRIDE == 0;
}

bool inspection_member_id(std::uintptr_t pointer, std::uintptr_t roster, std::uint16_t& id) {
    pointer &= OFFICER_POINTER_MASK;
    id = 0;
    if (!inspection_roster_pointer(pointer, roster)) {
        return true;
    }
    if (!inspect_value(pointer + 8, id)) {
        return false;
    }
    if (id == 0 || id > domain::OFFICER_RELATION_CAPACITY) {
        id = 0;
    }
    return true;
}

bool same_inspection_recovery_owner(const InspectionIdentity& expected,
    const OfficerInspectionContext& context) {
    if (expected.epoch != context.epoch || expected.module != context.module_base ||
        expected.root_pointer != context.game_base_pointer || expected.root != context.game_base) {
        return false;
    }
    std::uintptr_t root = 0;
    std::uint16_t id = 0;
    if (!inspect_value(expected.root_pointer, root) || root != expected.root ||
        roster_base(expected.module) != expected.roster ||
        !inspect_value(expected.officer + 8, id) || id != expected.id) {
        return false;
    }
    if (expected.protagonist) {
        std::uintptr_t player = 0;
        return inspect_value(root + 0xe0, player) && player == expected.officer && player == context.player;
    }
    // Roster refresh clears the UI's ID-to-slot map. Recovery must still be
    // possible during shutdown, without waiting for that map to be rebuilt.
    return inspection_roster_pointer(expected.officer, expected.roster);
}

} // namespace

struct OfficerInspectionState {
    std::optional<InspectionIdentity> owner;
    std::optional<FactionObservation> faction;
    FactionColorRestore restore;
    std::uint64_t token_counter = 0;
    std::chrono::steady_clock::time_point color_after{};
    std::chrono::steady_clock::time_point retry_after{};
    domain::OfficerRelations building;
    std::array<std::bitset<domain::OFFICER_RELATION_CAPACITY + 1>,
        static_cast<std::size_t>(RelationKind::Count)> seen;
    InspectionStage stage = InspectionStage::Idle;
    struct Edge {
        RelationKind kind;
        std::uint16_t other;
    };
    std::array<std::vector<Edge>, domain::OFFICER_RELATION_CAPACITY + 1> indexed;
    std::optional<domain::GameEpoch> indexed_epoch;
    std::uintptr_t indexed_root = 0;
    std::uintptr_t data_center = 0;
    std::uintptr_t direct_base = 0;
    std::uintptr_t synergy_slot = 0;
    std::uintptr_t synergy_pointer = 0;
    std::uintptr_t synergy_base = 0;
    std::array<std::uintptr_t, 3000> direct_records{};
    std::array<std::byte, 5000 * 0x20> synergy_records{};
    bool direct_ready = false;
    bool synergy_ready = false;
    bool direct_read_failed = false;
    bool synergy_read_failed = false;
    bool refresh_requested = true;
};

namespace {

bool publish_inspection_status(OfficerInspectionCache& cache, domain::GameEpoch epoch,
    std::uint16_t id, RelationStatus status) {
    if (cache.relations && cache.relations->epoch == epoch && cache.relations->officer_id == id &&
        cache.relations->status == status) {
        return false;
    }
    own::shared_owner<domain::OfficerRelations> result = own::make_shared<domain::OfficerRelations>();
    result->epoch = epoch;
    result->officer_id = id;
    result->status = status;
    cache.relations = std::move(result);
    return true;
}

void begin_inspection_relations(OfficerInspectionCache& cache, const InspectionIdentity& owner) {
    OfficerInspectionState& state = *cache.state;
    state.owner = owner;
    state.building.epoch = owner.epoch;
    state.building.officer_id = owner.id;
    state.building.status = RelationStatus::Loading;
    for (std::size_t index = 0; index < state.building.groups.size(); ++index) {
        state.building.groups[index].count = 0;
        state.building.groups[index].available = false;
        state.seen[index].reset();
    }
    state.refresh_requested = false;
    state.stage = InspectionStage::Publish;
}

bool inspection_pointer_unchanged(std::uintptr_t slot, std::uintptr_t expected) {
    std::uintptr_t pointer = 0;
    return slot == 0 || (inspect_value(slot, pointer) && pointer == expected);
}

void append_inspection_relation(OfficerInspectionState& state, RelationKind kind, std::uint16_t id) {
    if (id == 0 || id > domain::OFFICER_RELATION_CAPACITY || id == state.owner->id) {
        return;
    }
    const std::size_t index = relation_index(kind);
    domain::OfficerRelationGroup& group = state.building.groups[index];
    if (!state.seen[index].test(id)) {
        // At most one entry per valid ID; the full supported roster fits.
        state.seen[index].set(id);
        group.members[group.count++] = {id, std::nullopt};
    }
}

void read_inspection_relations(OfficerInspectionState& state) {
    const InspectionIdentity& owner = *state.owner;
    if (owner.registered) {
        state.indexed_epoch.reset();
        std::array<std::byte, 0xdc> record{};
        state.direct_ready = state.synergy_ready = inspect_value(owner.officer, record);
        state.direct_read_failed = state.synergy_read_failed = !state.direct_ready;
        if (!state.direct_ready) return;

        for (std::size_t slot = 0; slot < 3; ++slot) {
            append_inspection_relation(state, RelationKind::Sworn,
                inspection_field<std::uint16_t>(record, 0x76 + slot * 2));
            append_inspection_relation(state, RelationKind::Spouse,
                inspection_field<std::uint16_t>(record, 0x7c + slot * 2));
        }
        for (std::size_t slot = 0; slot < 20; ++slot) {
            const std::uint8_t kind = inspection_field<std::uint8_t>(record, 0x8e + slot * 4);
            if (kind == 1 || kind == 2) {
                append_inspection_relation(state, kind == 1 ? RelationKind::Antipathetic : RelationKind::Synergetic,
                    inspection_field<std::uint16_t>(record, 0x8c + slot * 4));
            }
        }
        for (const RelationKind kind : {RelationKind::Sworn, RelationKind::Spouse,
                RelationKind::Synergetic, RelationKind::Antipathetic}) {
            state.building.groups[relation_index(kind)].available = true;
        }
        return;
    }

    // Index once per session/explicit refresh, not once per selected officer.
    // Each entry is a native pair, never five adjacent pointer-shaped fields.
    const bool new_session = !state.indexed_epoch || *state.indexed_epoch != owner.epoch || state.indexed_root != owner.root;
    if (new_session || !state.direct_ready || !state.synergy_ready) {
        if (new_session) {
            for (std::vector<OfficerInspectionState::Edge>& edges : state.indexed) edges.clear();
            state.indexed_epoch = owner.epoch;
            state.indexed_root = owner.root;
            state.direct_ready = state.synergy_ready = false;
            state.direct_read_failed = state.synergy_read_failed = false;
            state.synergy_slot = state.synergy_pointer = state.synergy_base = 0;
            state.data_center = 0;
            inspect_value(owner.module + 0x2e98bc8, state.data_center);
            state.direct_base = state.data_center;
        }
        for (std::vector<OfficerInspectionState::Edge>& edges : state.indexed) {
            std::erase_if(edges, [&](const OfficerInspectionState::Edge& edge) {
                const bool destiny = edge.kind == RelationKind::Synergetic || edge.kind == RelationKind::Antipathetic;
                return destiny ? !state.synergy_ready : !state.direct_ready;
            });
        }

        if (!state.direct_ready) {
            const bool pointers_read = state.data_center &&
                inspect_value(state.data_center + DIRECT_POINTER_TABLE, state.direct_records);
            state.direct_read_failed = !pointers_read;
            if (pointers_read) {
                for (const std::uintptr_t address : state.direct_records) {
                    if (!address) continue;
                    std::array<std::byte, 0x20> record{};
                    if (!inspect_value(address, record)) { state.direct_read_failed = true; continue; }
                    const std::uint8_t kind = inspection_field<std::uint8_t>(record, 8);
                    if (kind == 0) continue;
                    // Other native relationship kinds are valid, but not edited by these groups.
                    if (kind > DIRECT_KINDS.size()) continue;
                    std::uint16_t first = 0, second = 0;
                    const std::uintptr_t a = inspection_field<std::uintptr_t>(record, 0x10);
                    const std::uintptr_t b = inspection_field<std::uintptr_t>(record, 0x18);
                    if (!a || !b || !inspect_value(a + 8, first) || !inspect_value(b + 8, second) ||
                        first == 0 || second == 0 || first > domain::OFFICER_RELATION_CAPACITY ||
                        second > domain::OFFICER_RELATION_CAPACITY) {
                        state.direct_read_failed = true;
                        continue;
                    }
                    state.indexed[first].push_back({DIRECT_KINDS[kind - 1], second});
                    state.indexed[second].push_back({DIRECT_KINDS[kind - 1], first});
                }
                state.direct_ready = !state.direct_read_failed;
            }
        }

        // Preserve the existing destiny-table layouts, but copy the table once.
        // Validation and indexing use that same snapshot, not 5,000 native reads.
        if (!state.synergy_ready) {
            for (const std::uintptr_t offset : SYNERGY_OFFSETS) {
                std::uintptr_t pointer = 0;
                if (!inspect_value(owner.root + offset, pointer) || pointer <= 0x10000 ||
                    !inspect_value(pointer + 0xa0, state.synergy_records)) continue;
                bool valid = true;
                for (std::size_t row = 0; row < 5000; ++row) {
                    const std::size_t position = row * 0x20;
                    const std::uint8_t kind = inspection_field<std::uint8_t>(state.synergy_records, position + 0x18);
                    if (kind == 0) continue;
                    if (kind > 2 ||
                        !inspection_roster_pointer(inspection_field<std::uintptr_t>(state.synergy_records, position + 8), owner.roster) ||
                        !inspection_roster_pointer(inspection_field<std::uintptr_t>(state.synergy_records, position + 0x10), owner.roster)) {
                        valid = false;
                        break;
                    }
                }
                if (!valid) continue;
                state.synergy_slot = owner.root + offset;
                state.synergy_pointer = pointer;
                state.synergy_base = pointer + 0xa0;
                state.synergy_ready = true;
                for (std::size_t row = 0; row < 5000; ++row) {
                    const std::size_t position = row * 0x20;
                    const std::uint8_t kind = inspection_field<std::uint8_t>(state.synergy_records, position + 0x18);
                    if (kind == 0 || inspection_field<std::uint8_t>(state.synergy_records, position + 0x19) != 0) continue;
                    std::uint16_t first = 0, second = 0;
                    if (!inspection_member_id(inspection_field<std::uintptr_t>(state.synergy_records, position + 8), owner.roster, first) ||
                        !inspection_member_id(inspection_field<std::uintptr_t>(state.synergy_records, position + 0x10), owner.roster, second) || !first || !second) {
                        state.synergy_ready = false;
                        break;
                    }
                    const RelationKind group = kind == 1 ? RelationKind::Antipathetic : RelationKind::Synergetic;
                    state.indexed[first].push_back({group, second});
                    state.indexed[second].push_back({group, first});
                }
                break;
            }
            state.synergy_read_failed = !state.synergy_ready;
        }
    }

    for (const OfficerInspectionState::Edge& edge : state.indexed[owner.id]) {
        const bool destiny = edge.kind == RelationKind::Synergetic || edge.kind == RelationKind::Antipathetic;
        if (destiny ? !state.synergy_ready : !state.direct_ready) continue;
        if (destiny) {
            bool direct = false;
            for (const RelationKind kind : DIRECT_KINDS) direct |= state.seen[relation_index(kind)].test(edge.other);
            if (direct) continue;
        }
        append_inspection_relation(state, edge.kind, edge.other);
    }
    for (const RelationKind kind : DIRECT_KINDS) state.building.groups[relation_index(kind)].available = state.direct_ready;
    state.building.groups[relation_index(RelationKind::Synergetic)].available = state.synergy_ready;
    state.building.groups[relation_index(RelationKind::Antipathetic)].available = state.synergy_ready;
    // Optional affinity is read only for the selected officer's actual members.
    if (state.data_center) {
        for (domain::OfficerRelationGroup& group : state.building.groups) {
            for (std::size_t index = 0; index < group.count; ++index) {
                domain::OfficerRelationMember& member = group.members[index];
                const std::optional<std::uint32_t> offset = domain::affinity_pair_offset(owner.id, member.id);
                std::uint8_t affinity = 0;
                if (offset && inspect_value(state.data_center + 0x24206 + *offset, affinity) && affinity <= 100) member.affinity = affinity;
            }
        }
    }
}

// Keep SEH at the native-call boundary; no C++ owning objects in these frames.
bool set_registered_relation(std::uintptr_t module, std::uintptr_t officer,
    RelationKind kind, int slot, std::uint16_t replacement) {
    const std::uintptr_t offset = kind == RelationKind::Sworn ? 0x1f03df0 :
        kind == RelationKind::Spouse ? 0x1f02e30 : 0x1f024f0;
    const std::uint8_t native_kind = replacement == 0 ? 0 : kind == RelationKind::Antipathetic ? 1 : 2;
    using SlotSetter = void(__fastcall*)(std::uintptr_t, int, std::uint16_t);
    using DestinySetter = void(__fastcall*)(std::uintptr_t, int, std::uint16_t, std::uint8_t);
    __try {
        if (kind == RelationKind::Sworn || kind == RelationKind::Spouse) {
            reinterpret_cast<SlotSetter>(module + offset)(officer, slot, replacement);
        } else {
            reinterpret_cast<DestinySetter>(module + offset)(officer, slot, replacement, native_kind);
        }
        return true;
    } __except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION || GetExceptionCode() == EXCEPTION_IN_PAGE_ERROR
            ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
        return false;
    }
}

bool commit_native_pair(std::uintptr_t module, std::uintptr_t center, std::uintptr_t record,
    std::uintptr_t owner, std::uintptr_t replacement, std::uint8_t kind,
    const std::byte* previous) {
    using MapPair = void(__fastcall*)(std::uintptr_t, std::uintptr_t, std::uintptr_t, std::int16_t);
    using ResetPair = void(__fastcall*)(std::uintptr_t);
    using FindPair = int(__fastcall*)(std::uintptr_t, std::uintptr_t, std::uintptr_t);
    using CreatePair = bool(__fastcall*)(std::uintptr_t, std::uintptr_t, std::uintptr_t,
        std::uint8_t, std::uint8_t, std::uintptr_t, std::uintptr_t, std::uint8_t);
    __try {
        const std::uint8_t old_kind = *reinterpret_cast<const std::uint8_t*>(previous + 8);
        const std::uintptr_t first = *reinterpret_cast<const std::uintptr_t*>(previous + 0x10);
        const std::uintptr_t second = *reinterpret_cast<const std::uintptr_t*>(previous + 0x18);
        const std::uintptr_t vtable = *reinterpret_cast<const std::uintptr_t*>(previous);
        if (replacement && replacement != first && replacement != second &&
            reinterpret_cast<FindPair>(module + 0x16c7c70)(center, owner, replacement) >= 0) return false;
        if (old_kind != 0) {
            reinterpret_cast<MapPair>(module + 0x16c7d00)(center, first, second, -1);
            reinterpret_cast<ResetPair>(*reinterpret_cast<const std::uintptr_t*>(vtable + 0x50))(record);
        }
        if (!replacement) {
            return reinterpret_cast<FindPair>(module + 0x16c7c70)(center, first, second) == -1;
        }
        if (reinterpret_cast<CreatePair>(module + 0x16c5c50)(center, owner, replacement, kind, 0, 0, 0, 0)) {
            const int slot = reinterpret_cast<FindPair>(module + 0x16c7c70)(center, owner, replacement);
            if (slot < 0 || slot >= 3000) return false;
            const std::uintptr_t applied = *reinterpret_cast<const std::uintptr_t*>(center + DIRECT_POINTER_TABLE + slot * 8);
            return applied && *reinterpret_cast<const std::uint8_t*>(applied + 8) == kind;
        }
        // Native allocation failure: restore the removed relationship via the same initializer.
        if (old_kind != 0) {
            reinterpret_cast<CreatePair>(module + 0x16c5c50)(center, first, second, old_kind,
                static_cast<std::uint8_t>(previous[0x38]),
                *reinterpret_cast<const std::uintptr_t*>(previous + 0x20),
                *reinterpret_cast<const std::uintptr_t*>(previous + 0x28),
                static_cast<std::uint8_t>(previous[0x39]));
        }
        return false;
    } __except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION || GetExceptionCode() == EXCEPTION_IN_PAGE_ERROR
            ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
        return false;
    }
}

void refresh_inspected_color(OfficerInspectionCache& cache, const InspectionIdentity& owner) {
    OfficerInspectionState& state = *cache.state;
    if (owner.registered) {
        cache.faction_color = {};
        cache.faction_color.officer_id = owner.id;
        cache.faction_color.no_faction = true;
        return;
    }
    const std::optional<FactionObservation> current = inspect_faction(owner);
    cache.faction_color.officer_id = owner.id;
    cache.faction_color.available = false;
    cache.faction_color.no_faction = false;
    cache.faction_color.recovery_pending = state.restore.pending;
    if (!current) {
        return;
    }
    if (!state.faction || !same_faction(*state.faction, *current)) {
        cache.faction_color.token = state.token_counter == std::numeric_limits<std::uint64_t>::max()
            ? 0 : ++state.token_counter;
    }
    state.faction = current;
    cache.faction_color.color = current->color;
    cache.faction_color.no_faction = current->force == 0;
    cache.faction_color.available = current->force != 0 && cache.faction_color.token != 0 && !state.restore.pending;
}

} // namespace

OfficerInspectionCache::OfficerInspectionCache() : state(own::make_unique<OfficerInspectionState>()) {}
OfficerInspectionCache::~OfficerInspectionCache() = default;

void request_officer_relations_refresh(OfficerInspectionCache& cache) {
    cache.state->refresh_requested = true;
    cache.state->indexed_epoch.reset();
    if (cache.state->owner) {
        publish_inspection_status(cache, cache.state->owner->epoch, cache.state->owner->id, RelationStatus::Loading);
    }
}

void reset_officer_inspection(OfficerInspectionCache& cache, domain::GameEpoch) {
    OfficerInspectionState& state = *cache.state;
    state.owner.reset();
    state.indexed_epoch.reset();
    state.faction.reset();
    state.stage = InspectionStage::Idle;
    state.color_after = {};
    state.refresh_requested = true;
    cache.relations = {};
    cache.faction_color = {};
    cache.faction_color.recovery_pending = state.restore.pending;
    // Never reset token_counter or a possibly modified native byte's journal.
}

bool restore_officer_faction_color(OfficerInspectionCache& cache, const OfficerRosterScan&,
    const OfficerInspectionContext& context) {
    OfficerInspectionState& state = *cache.state;
    FactionColorRestore& restore = state.restore;
    if (!restore.pending) {
        cache.faction_color.recovery_pending = false;
        return true;
    }
    cache.faction_color.recovery_pending = true;
    cache.faction_color.available = false;
    if (!same_inspection_recovery_owner(restore.previous.owner, context)) {
        return false;
    }
    const std::optional<FactionObservation> current = inspect_faction(restore.previous.owner);
    if (!current || !same_faction(*current, restore.previous)) {
        return false;
    }
    if (current->color != restore.previous.color) {
        if (current->color != restore.replacement || !same_inspection_recovery_owner(restore.previous.owner, context)) {
            return false;
        }
        const std::uint8_t original = restore.previous.color;
        platform::windows::write_memory(restore.previous.force + 9,
            std::as_bytes(std::span(&original, 1)));
        const std::optional<FactionObservation> verified = inspect_faction(restore.previous.owner);
        // A failed API call may still restore the byte. Readback, under the
        // original live identity, decides whether the journal can be retired.
        if (!verified || !same_faction(*verified, restore.previous) ||
            verified->color != original || !same_inspection_recovery_owner(restore.previous.owner, context)) {
            return false;
        }
    } else if (!same_inspection_recovery_owner(restore.previous.owner, context)) {
        return false;
    }
    restore.pending = false;
    cache.faction_color.recovery_pending = false;
    state.color_after = {};
    return true;
}

bool update_officer_inspection(OfficerInspectionCache& cache, const OfficerRosterScan& scan,
    const OfficerInspectionContext& context, std::uint16_t officer_id, bool protagonist,
    const domain::WorkBudget&) {
    const platform::windows::RealClockScope real_clock;
    // The selected detail is one bounded query. Roster/background work must
    // not repeatedly exhaust the shared budget before this query is admitted.
    OfficerInspectionState& state = *cache.state;
    const domain::OfficerFactionColor previous_color = cache.faction_color;
    bool changed = false;
    if (state.restore.pending) {
        restore_officer_faction_color(cache, scan, context);
    }
    const std::optional<InspectionIdentity> owner = inspection_identity(scan, context, officer_id, protagonist);
    if (!owner) {
        cache.faction_color.officer_id = officer_id;
        cache.faction_color.available = false;
        cache.faction_color.no_faction = false;
        state.stage = InspectionStage::Idle;
        state.refresh_requested = true;
        changed = publish_inspection_status(cache, context.epoch, officer_id, RelationStatus::ReadFailed);
        return changed || previous_color != cache.faction_color;
    }
    const bool new_owner = !state.owner || *state.owner != *owner;
    const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    const bool retry_failed = now >= state.retry_after && (!state.direct_ready || !state.synergy_ready);
    if (!owner->registered && state.indexed_epoch &&
        !inspection_pointer_unchanged(context.module_base + 0x2e98bc8, state.data_center)) {
        state.indexed_epoch.reset();
        state.refresh_requested = true;
    }
    if (new_owner || state.refresh_requested || !cache.relations || retry_failed) {
        begin_inspection_relations(cache, *owner);
        changed = true;
    }
    if (new_owner || now >= state.color_after || state.restore.pending) {
        refresh_inspected_color(cache, *owner);
        state.color_after = now + std::chrono::milliseconds(250);
    }
    if (state.stage == InspectionStage::Idle) {
        return changed || previous_color != cache.faction_color;
    }
    read_inspection_relations(state);
    state.retry_after = now + std::chrono::seconds(2);
    if (!same_inspection_owner(*state.owner, scan, context)) {
        state.indexed_epoch.reset();
        changed = publish_inspection_status(cache, context.epoch, officer_id, RelationStatus::ReadFailed);
    } else {
        state.building.status = state.direct_ready && state.synergy_ready ? RelationStatus::Ready :
            state.direct_ready || state.synergy_ready ? RelationStatus::Partial : RelationStatus::ReadFailed;
        cache.relations = own::make_shared<domain::OfficerRelations>(state.building);
        changed = true;
    }
    state.stage = InspectionStage::Idle;
    return changed || previous_color != cache.faction_color;
}

domain::Result<void> replace_officer_relation(OfficerInspectionCache& cache,
    const OfficerRosterScan& scan, const OfficerInspectionContext& context,
    std::uint16_t officer_id, domain::OfficerRelationKind kind,
    std::uint16_t previous_id, std::uint16_t replacement_id) {
    OfficerInspectionState& state = *cache.state;
    const bool synergy = kind == RelationKind::Synergetic || kind == RelationKind::Antipathetic;
    if ((!synergy && kind != RelationKind::Spouse && kind != RelationKind::Sworn) ||
        (previous_id == 0 && replacement_id == 0) || replacement_id == officer_id || previous_id == officer_id) {
        return std::unexpected(domain::Error{domain::ErrorCode::InvalidData});
    }
    if (!state.owner || state.owner->id != officer_id || state.stage != InspectionStage::Idle ||
        !(synergy ? state.synergy_ready : state.direct_ready) ||
        !same_inspection_owner(*state.owner, scan, context) ||
        (!state.owner->registered && !inspection_pointer_unchanged(
            synergy ? state.synergy_slot : context.module_base + 0x2e98bc8,
            synergy ? state.synergy_pointer : state.data_center))) {
        return std::unexpected(domain::Error{domain::ErrorCode::StaleEpoch});
    }
    if (previous_id == replacement_id) return {};
    if (state.owner->registered) {
        std::array<std::byte, 0xdc> before{};
        if (!inspect_value(state.owner->officer, before)) {
            return std::unexpected(domain::Error{domain::ErrorCode::PlatformFailure});
        }
        const std::size_t count = synergy ? 20 : 3;
        const std::size_t stride = synergy ? 4 : 2;
        const std::size_t start = synergy ? 0x8c : kind == RelationKind::Sworn ? 0x76 : 0x7c;
        const std::uint8_t native_kind = kind == RelationKind::Antipathetic ? 1 : 2;
        int slot = -1;
        for (std::size_t index = 0; index < count; ++index) {
            const std::uint16_t member = inspection_field<std::uint16_t>(before, start + index * stride);
            const bool matches_kind = !synergy || member == 0 ||
                inspection_field<std::uint8_t>(before, start + index * stride + 2) == native_kind;
            if (replacement_id && member == replacement_id && matches_kind) {
                if (previous_id == replacement_id) return {};
                return std::unexpected(domain::Error{domain::ErrorCode::InvalidData});
            }
            if (slot < 0 && member == previous_id && matches_kind) slot = static_cast<int>(index);
        }
        if (slot < 0 || replacement_id > 5200) {
            return std::unexpected(domain::Error{domain::ErrorCode::InvalidData});
        }
        const bool applied = set_registered_relation(context.module_base, state.owner->officer, kind, slot, replacement_id);
        request_officer_relations_refresh(cache);
        std::array<std::byte, 0xdc> after{};
        if (!applied || !inspect_value(state.owner->officer, after)) {
            return std::unexpected(domain::Error{domain::ErrorCode::PlatformFailure});
        }
        bool found = replacement_id == 0;
        for (std::size_t index = 0; index < count; ++index) {
            const std::uint16_t member = inspection_field<std::uint16_t>(after, start + index * stride);
            const bool matches_kind = !synergy ||
                inspection_field<std::uint8_t>(after, start + index * stride + 2) == native_kind;
            if (member == replacement_id && matches_kind) found = true;
            if (previous_id && member == previous_id && matches_kind) {
                return std::unexpected(domain::Error{domain::ErrorCode::PlatformFailure});
            }
        }
        if (!found) return std::unexpected(domain::Error{domain::ErrorCode::PlatformFailure});
        return {};
    }
    std::uintptr_t replacement = 0;
    if (replacement_id != 0 && replacement_id <= domain::OFFICER_RELATION_CAPACITY) {
        inspect_value(state.data_center + 0x21bdf0 + replacement_id * sizeof(std::uintptr_t), replacement);
    }
    if (replacement_id != 0 && replacement == 0) {
        return std::unexpected(domain::Error{domain::ErrorCode::InvalidData});
    }
    const std::uintptr_t base = synergy ? state.synergy_base : state.direct_base;
    const std::size_t stride = synergy ? 0x20 : 0x40;
    const std::size_t count = synergy ? 5000 : 3000;
    const std::size_t first = synergy ? 8 : 0x10;
    const std::size_t members = 2;
    const std::uint8_t native_kind = synergy ? (kind == RelationKind::Antipathetic ? 1 : 2) :
        (kind == RelationKind::Sworn ? 1 : 2);
    std::uintptr_t target = 0;
    std::uintptr_t vacant = 0;
    std::array<std::byte, 0x40> before{};
    std::array<std::byte, 0x40> vacant_record{};
    std::size_t target_member = 0;
    for (std::size_t row = 0; row < count; ++row) {
        std::array<std::byte, 0x40> record{};
        std::uintptr_t address = base + row * stride;
        if (!synergy && !inspect_value(base + DIRECT_POINTER_TABLE + row * sizeof(std::uintptr_t), address)) {
            return std::unexpected(domain::Error{domain::ErrorCode::PlatformFailure});
        }
        if (!address) continue;
        if (!platform::windows::read_memory(address, std::span(record).first(stride))) {
            return std::unexpected(domain::Error{domain::ErrorCode::PlatformFailure});
        }
        const std::uint8_t row_kind = inspection_field<std::uint8_t>(record, synergy ? 0x18 : 8);
        bool empty = true;
        bool contains_owner = false;
        bool contains_replacement = false;
        std::size_t member_slot = members;
        for (std::size_t member = 0; member < members; ++member) {
            const std::uintptr_t pointer = inspection_field<std::uintptr_t>(record, first + member * 8);
            empty &= pointer == 0;
            if (row_kind != native_kind) continue;
            std::uint16_t id = 0;
            if (pointer && !(synergy ? inspection_member_id(pointer, state.owner->roster, id) : inspect_value(pointer + 8, id))) {
                return std::unexpected(domain::Error{domain::ErrorCode::PlatformFailure});
            }
            contains_owner |= id == officer_id;
            contains_replacement |= replacement_id != 0 && id == replacement_id;
            if ((previous_id != 0 && id == previous_id) ||
                (previous_id == 0 && pointer == 0 && member_slot == members)) member_slot = member;
        }
        // Reuse only an allocated, inactive record with no members.
        if (row_kind == 0 && empty && vacant == 0 &&
            inspection_field<std::uintptr_t>(record, 0) != 0) {
            vacant = address;
            vacant_record = record;
        }
        if (row_kind != native_kind || !contains_owner) continue;
        if (contains_replacement && replacement_id != previous_id) {
            return std::unexpected(domain::Error{domain::ErrorCode::InvalidData});
        }
        if (member_slot != members) {
            if (target != 0) return std::unexpected(domain::Error{domain::ErrorCode::InvalidData});
            target = address;
            target_member = member_slot;
            before = record;
        }
    }
    const bool new_record = target == 0 && previous_id == 0;
    if (new_record) {
        target = vacant;
        before = vacant_record;
        target_member = 1;
    }
    if (target == 0) return std::unexpected(domain::Error{domain::ErrorCode::StaleEpoch});
    if (!synergy) {
        const bool applied = commit_native_pair(context.module_base, state.data_center, target,
            state.owner->officer, replacement, native_kind, before.data());
        request_officer_relations_refresh(cache);
        if (!applied) return std::unexpected(domain::Error{domain::ErrorCode::PlatformFailure});
        return {};
    }
    std::array<std::byte, 0x40> after = before;
    if (new_record) {
        const std::uintptr_t owner = state.owner->officer;
        std::memcpy(after.data() + first, &owner, sizeof(owner));
        after[synergy ? 0x18 : 8] = static_cast<std::byte>(native_kind);
    }
    std::memcpy(after.data() + first + target_member * 8, &replacement, sizeof(replacement));
    if (replacement_id == 0) {
        std::size_t remaining = 0;
        for (std::size_t member = 0; member < members; ++member) {
            const std::uintptr_t pointer = inspection_field<std::uintptr_t>(after, first + member * 8);
            if (pointer != 0) {
                std::memcpy(after.data() + first + remaining++ * 8, &pointer, sizeof(pointer));
            }
        }
        if (remaining < 2) {
            remaining = 0;
            after[synergy ? 0x18 : 8] = std::byte{};
        }
        std::memset(after.data() + first + remaining * 8, 0, (members - remaining) * 8);
    }
    if (!platform::windows::write_memory(target, std::span<const std::byte>(after).first(stride))) {
        platform::windows::write_memory(target, std::span<const std::byte>(before).first(stride));
        return std::unexpected(domain::Error{domain::ErrorCode::PlatformFailure});
    }
    request_officer_relations_refresh(cache);
    return {};
}

domain::Result<void> write_officer_faction_color(OfficerInspectionCache& cache,
    const OfficerRosterScan& scan, const OfficerInspectionContext& context,
    std::uint16_t officer_id, std::uint64_t expected_token, std::uint8_t expected_color, std::uint8_t color) {
    OfficerInspectionState& state = *cache.state;
    if (state.restore.pending) {
        return std::unexpected(domain::Error{domain::ErrorCode::PlatformFailure, 0x5302});
    }
    if (!state.owner || !state.faction || officer_id != state.owner->id || expected_token == 0 ||
        expected_token != cache.faction_color.token || !cache.faction_color.available ||
        !same_inspection_owner(*state.owner, scan, context)) {
        return std::unexpected(domain::Error{domain::ErrorCode::StaleEpoch});
    }
    const std::optional<FactionObservation> current = inspect_faction(*state.owner);
    if (!current || !same_faction(*state.faction, *current) || current->force == 0 || current->color != expected_color ||
        !same_inspection_owner(*state.owner, scan, context)) {
        state.color_after = {};
        return std::unexpected(domain::Error{domain::ErrorCode::StaleEpoch});
    }
    if (expected_color == color) {
        return {};
    }
    state.restore = {*current, color, true};
    cache.faction_color.recovery_pending = true;
    const bool written = platform::windows::write_memory(current->force + 9, std::as_bytes(std::span(&color, 1)));
    const std::optional<FactionObservation> verified = inspect_faction(*state.owner);
    if (written && verified && same_faction(*current, *verified) && verified->color == color &&
        same_inspection_owner(*state.owner, scan, context)) {
        state.restore.pending = false;
        state.faction = verified;
        cache.faction_color.color = color;
        cache.faction_color.recovery_pending = false;
        state.color_after = {};
        return {};
    }
    const bool restored = restore_officer_faction_color(cache, scan, context);
    state.color_after = {};
    return std::unexpected(domain::Error{domain::ErrorCode::PlatformFailure, restored ? 0x5301u : 0x5302u});
}

} // namespace s8::game
