#include "officer_roster.h"
#include "../platform/windows/process_memory.h"

#include <algorithm>
#include <bit>
#include <cstring>
#include <span>

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
    if (!id || id >= scan.slots.size() || !scan.slots[id]) {
        return 0;
    }
    const auto roster = roster_base(module_base);
    if (!roster || roster != scan.base) {
        return 0;
    }
    const auto officer = roster + (scan.slots[id] - 1) * 0x3d0;
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
    reset_officer_roster(scan);
    scan.has_scan = true;
    scan.requested = true;
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

void update_officer_roster(OfficerRosterScan& scan, std::uintptr_t module_base, const domain::WorkBudget& budget) {
    if (!scan.requested || budget.exhausted()) {
        return;
    }
    const std::uintptr_t roster = roster_base(module_base);

    if (!roster) {
        return;
    }
    if (scan.base != roster) {
        if (scan.base != 0) {
            request_officer_roster(scan);
        }
        scan.base = roster;
    }
    const std::size_t end = std::min(scan.cursor + domain::OFFICER_ROSTER_BATCH_CAPACITY, domain::OFFICER_ROSTER_LIMIT);

    for (; scan.cursor < end; ++scan.cursor) {
        if (budget.exhausted()) {
            return;
        }
        const std::uintptr_t officer = roster + scan.cursor * 0x3d0;
        std::uint16_t id = 0, face = 0, merit = 0, reputation = 0;
        std::uint32_t gold = 0;
        std::uint8_t strategy = 0, status = 0;

        if (!platform::windows::read_memory(officer + 8, std::as_writable_bytes(std::span(&id, 1))) ||
            !platform::windows::read_memory(officer + 0x2e, std::as_writable_bytes(std::span(&face, 1))) ||
            !platform::windows::read_memory(officer + 0xe8, std::as_writable_bytes(std::span(&gold, 1))) ||
            !platform::windows::read_memory(officer + 0x100, std::as_writable_bytes(std::span(&merit, 1))) ||
            !platform::windows::read_memory(officer + 0x108, std::as_writable_bytes(std::span(&reputation, 1))) ||
            !platform::windows::read_memory(officer + 0xed, std::as_writable_bytes(std::span(&strategy, 1))) ||
            !platform::windows::read_memory(officer + 0x10, std::as_writable_bytes(std::span(&status, 1))) ||
            id == 0 || id > domain::OFFICER_ROSTER_LIMIT || scan.entries[id].id != 0) {
            continue;
        }
        scan.slots[id] = static_cast<std::uint16_t>(scan.cursor + 1);
        scan.entries[id] = {id, status};
        mark_roster_dirty(scan, id);
    }
    if (scan.cursor == domain::OFFICER_ROSTER_LIMIT) {
        scan.requested = false;
        scan.completed = true;
    }
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

} // namespace s8::game
