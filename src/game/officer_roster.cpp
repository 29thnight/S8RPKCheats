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

namespace {

using RelationKind = domain::OfficerRelationKind;
using RelationStatus = domain::OfficerRelationsStatus;
constexpr std::size_t INSPECTION_WORK_ITEMS = 32;
constexpr std::uintptr_t OFFICER_STRIDE = 0x3d0;
constexpr std::uintptr_t OFFICER_POINTER_MASK = 0x0000ffffffffffffULL;
constexpr std::array<std::uintptr_t, 2> DIRECT_OFFSETS{0x57ae38, 0x462ba8};
constexpr std::array<std::uintptr_t, 2> DIRECT_BIASES{0x80, 0xc0};
constexpr std::array<std::uintptr_t, 2> SYNERGY_OFFSETS{0x547330, 0x433210};
constexpr std::array<RelationKind, 4> DIRECT_KINDS{
    RelationKind::Sworn, RelationKind::Spouse, RelationKind::Enemy, RelationKind::Rival};

enum class InspectionStage {
    Idle, DirectCandidate, DirectValidate, DirectScan,
    SynergyCandidate, SynergyValidate, SynergyScan, Affinity, Publish
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

std::optional<InspectionIdentity> inspection_identity(const OfficerRosterScan& scan,
    const OfficerInspectionContext& context, std::uint16_t id, bool protagonist) {
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
        officer = roster_officer(scan, context.module_base, id);
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
    domain::OfficerRelations building;
    std::array<std::bitset<domain::OFFICER_RELATION_CAPACITY + 1>,
        static_cast<std::size_t>(RelationKind::Count)> seen;
    InspectionStage stage = InspectionStage::Idle;
    std::size_t candidate_index = 0;
    std::size_t cursor = 0;
    std::size_t candidate_valid = 0;
    std::size_t candidate_invalid = 0;
    std::size_t affinity_group = 0;
    std::uintptr_t candidate = 0;
    std::uintptr_t candidate_slot = 0;
    std::uintptr_t candidate_pointer = 0;
    std::uintptr_t direct_slot = 0;
    std::uintptr_t direct_pointer = 0;
    std::uintptr_t synergy_slot = 0;
    std::uintptr_t synergy_pointer = 0;
    std::uintptr_t data_center = 0;
    bool direct_ready = false;
    bool synergy_ready = false;
    bool direct_read_failed = false;
    bool synergy_read_failed = false;
    bool affinity_clearing = false;
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
    state.candidate_index = 0;
    state.cursor = 0;
    state.candidate = 0;
    state.candidate_slot = 0;
    state.candidate_pointer = 0;
    state.direct_slot = 0;
    state.direct_pointer = 0;
    state.synergy_slot = 0;
    state.synergy_pointer = 0;
    state.direct_ready = false;
    state.synergy_ready = false;
    state.direct_read_failed = false;
    state.synergy_read_failed = false;
    state.affinity_clearing = false;
    state.refresh_requested = false;
    state.stage = InspectionStage::DirectCandidate;
    publish_inspection_status(cache, owner.epoch, owner.id, RelationStatus::Loading);
}

void begin_synergy_inspection(OfficerInspectionState& state) {
    state.candidate_index = 0;
    state.cursor = 0;
    state.candidate_slot = 0;
    state.stage = InspectionStage::SynergyCandidate;
}

void begin_inspection_affinity(OfficerInspectionState& state) {
    state.data_center = 0;
    if (!inspect_value(state.owner->module + 0x2e98bc8, state.data_center) ||
        state.data_center <= 0x10000 ||
        state.data_center > std::numeric_limits<std::uintptr_t>::max() - 0x200000) {
        state.data_center = 0;
    }
    state.cursor = 0;
    state.affinity_group = 0;
    state.stage = InspectionStage::Affinity;
}

bool inspection_pointer_unchanged(std::uintptr_t slot, std::uintptr_t expected) {
    std::uintptr_t pointer = 0;
    return slot == 0 || (inspect_value(slot, pointer) && pointer == expected);
}

bool discard_stale_inspection_affinity(OfficerInspectionState& state) {
    if (state.data_center == 0 || state.affinity_clearing ||
        inspection_pointer_unchanged(state.owner->module + 0x2e98bc8, state.data_center)) {
        return false;
    }
    state.affinity_clearing = true;
    state.affinity_group = 0;
    state.cursor = 0;
    state.stage = InspectionStage::Affinity;
    return true;
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

void direct_inspection_failed(OfficerInspectionState& state) {
    state.direct_read_failed = true;
    state.direct_ready = false;
    state.direct_slot = 0;
    for (const RelationKind kind : DIRECT_KINDS) {
        state.building.groups[relation_index(kind)].count = 0;
        state.building.groups[relation_index(kind)].available = false;
        state.seen[relation_index(kind)].reset();
    }
    begin_synergy_inspection(state);
}

void synergy_inspection_failed(OfficerInspectionState& state) {
    state.synergy_read_failed = true;
    state.synergy_ready = false;
    state.synergy_slot = 0;
    for (const RelationKind kind : {RelationKind::Synergetic, RelationKind::Antipathetic}) {
        state.building.groups[relation_index(kind)].count = 0;
        state.building.groups[relation_index(kind)].available = false;
        state.seen[relation_index(kind)].reset();
    }
    begin_inspection_affinity(state);
}

void inspect_relation_candidate(OfficerInspectionState& state, bool synergy) {
    const std::size_t limit = synergy ? SYNERGY_OFFSETS.size() : DIRECT_OFFSETS.size() * DIRECT_BIASES.size();
    if (state.candidate_index == limit) {
        if (synergy) {
            begin_inspection_affinity(state);
        } else {
            begin_synergy_inspection(state);
        }
        return;
    }
    const std::size_t index = state.candidate_index++;
    state.candidate_slot = state.owner->root + (synergy ? SYNERGY_OFFSETS[index] : DIRECT_OFFSETS[index / 2]);
    if (!inspect_value(state.candidate_slot, state.candidate_pointer)) {
        (synergy ? state.synergy_read_failed : state.direct_read_failed) = true;
        return;
    }
    if (state.candidate_pointer <= 0x100c0 ||
        state.candidate_pointer > std::numeric_limits<std::uintptr_t>::max() - 0x40000) {
        return;
    }
    state.candidate = synergy ? state.candidate_pointer + 0xa0 : state.candidate_pointer - DIRECT_BIASES[index % 2];
    state.cursor = 0;
    state.candidate_valid = 0;
    state.candidate_invalid = 0;
    state.stage = synergy ? InspectionStage::SynergyValidate : InspectionStage::DirectValidate;
}

void validate_inspection_candidate(OfficerInspectionState& state, bool synergy) {
    const std::size_t limit = synergy ? 600 : 128;
    const InspectionStage rejected = synergy ? InspectionStage::SynergyCandidate : InspectionStage::DirectCandidate;
    if (!inspection_pointer_unchanged(state.candidate_slot, state.candidate_pointer)) {
        (synergy ? state.synergy_read_failed : state.direct_read_failed) = true;
        state.stage = rejected;
        return;
    }
    if (state.cursor == limit) {
        if (state.candidate_valid == 0) {
            state.stage = rejected;
        } else {
            (synergy ? state.synergy_slot : state.direct_slot) = state.candidate_slot;
            (synergy ? state.synergy_pointer : state.direct_pointer) = state.candidate_pointer;
            state.cursor = 0;
            state.stage = synergy ? InspectionStage::SynergyScan : InspectionStage::DirectScan;
        }
        return;
    }
    std::array<std::byte, 0x40> record{};
    const std::size_t stride = synergy ? 0x20 : 0x40;
    if (!platform::windows::read_memory(state.candidate + state.cursor++ * stride,
            std::span(record).first(stride))) {
        (synergy ? state.synergy_read_failed : state.direct_read_failed) = true;
        state.stage = rejected;
        return;
    }
    const std::uint8_t kind = inspection_field<std::uint8_t>(record, synergy ? 0x18 : 8);
    if (kind == 0) {
        return;
    }
    const std::size_t first = synergy ? 8 : 0x10;
    if (kind > (synergy ? 2 : 4) ||
        !inspection_roster_pointer(inspection_field<std::uintptr_t>(record, first), state.owner->roster) ||
        !inspection_roster_pointer(inspection_field<std::uintptr_t>(record, first + 8), state.owner->roster)) {
        if (++state.candidate_invalid > 3) {
            state.stage = rejected;
        }
    } else {
        ++state.candidate_valid;
    }
}

void inspect_direct_relation(OfficerInspectionState& state) {
    if (!inspection_pointer_unchanged(state.direct_slot, state.direct_pointer)) {
        direct_inspection_failed(state);
        return;
    }
    if (state.cursor == 3000) {
        state.direct_ready = true;
        for (const RelationKind kind : DIRECT_KINDS) {
            state.building.groups[relation_index(kind)].available = true;
        }
        begin_synergy_inspection(state);
        return;
    }
    std::array<std::byte, 0x40> record{};
    if (!inspect_value(state.candidate + state.cursor++ * record.size(), record)) {
        direct_inspection_failed(state);
        return;
    }
    const std::uint8_t kind = inspection_field<std::uint8_t>(record, 8);
    if (kind < 1 || kind > DIRECT_KINDS.size()) {
        return;
    }
    std::array<std::uint16_t, 5> members{};
    bool contains_officer = false;
    for (std::size_t index = 0; index < members.size(); ++index) {
        if (!inspection_member_id(inspection_field<std::uintptr_t>(record, 0x10 + index * 8),
                state.owner->roster, members[index])) {
            direct_inspection_failed(state);
            return;
        }
        contains_officer = contains_officer || members[index] == state.owner->id;
    }
    if (contains_officer) {
        for (const std::uint16_t id : members) {
            append_inspection_relation(state, DIRECT_KINDS[kind - 1], id);
        }
    }
}

void inspect_synergy_relation(OfficerInspectionState& state) {
    if (!inspection_pointer_unchanged(state.synergy_slot, state.synergy_pointer)) {
        synergy_inspection_failed(state);
        return;
    }
    if (state.cursor == 5000) {
        state.synergy_ready = true;
        state.building.groups[relation_index(RelationKind::Synergetic)].available = true;
        state.building.groups[relation_index(RelationKind::Antipathetic)].available = true;
        begin_inspection_affinity(state);
        return;
    }
    std::array<std::byte, 0x20> record{};
    if (!inspect_value(state.candidate + state.cursor++ * record.size(), record)) {
        synergy_inspection_failed(state);
        return;
    }
    const std::uint8_t kind = inspection_field<std::uint8_t>(record, 0x18);
    if ((kind != 1 && kind != 2) || inspection_field<std::uint8_t>(record, 0x19) != 0) {
        return;
    }
    std::uint16_t first = 0, second = 0;
    if (!inspection_member_id(inspection_field<std::uintptr_t>(record, 8), state.owner->roster, first) ||
        !inspection_member_id(inspection_field<std::uintptr_t>(record, 0x10), state.owner->roster, second)) {
        synergy_inspection_failed(state);
        return;
    }
    const std::uint16_t other = first == state.owner->id ? second : second == state.owner->id ? first : 0;
    if (other == 0 || other == state.owner->id) {
        return;
    }
    // The original viewer excludes every direct relation from destiny groups.
    for (const RelationKind direct : DIRECT_KINDS) {
        if (state.seen[relation_index(direct)].test(other)) {
            return;
        }
    }
    append_inspection_relation(state, kind == 1 ? RelationKind::Antipathetic : RelationKind::Synergetic, other);
}

void inspect_relation_affinity(OfficerInspectionState& state) {
    if (state.affinity_group == state.building.groups.size()) {
        state.stage = InspectionStage::Publish;
        return;
    }
    domain::OfficerRelationGroup& group = state.building.groups[state.affinity_group];
    if (state.cursor == group.count) {
        ++state.affinity_group;
        state.cursor = 0;
        return;
    }
    domain::OfficerRelationMember& member = group.members[state.cursor++];
    member.affinity.reset();
    if (state.affinity_clearing || state.data_center == 0) {
        return;
    }
    const std::optional<std::uint32_t> offset = domain::affinity_pair_offset(state.owner->id, member.id);
    std::uint8_t affinity = 0;
    if (offset && inspect_value(state.data_center + 0x24206 + *offset, affinity) && affinity <= 100) {
        member.affinity = affinity;
    }
}

void refresh_inspected_color(OfficerInspectionCache& cache, const InspectionIdentity& owner) {
    OfficerInspectionState& state = *cache.state;
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
    if (cache.state->owner) {
        publish_inspection_status(cache, cache.state->owner->epoch, cache.state->owner->id, RelationStatus::Loading);
    }
}

void reset_officer_inspection(OfficerInspectionCache& cache, domain::GameEpoch) {
    OfficerInspectionState& state = *cache.state;
    state.owner.reset();
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
    const domain::WorkBudget& budget) {
    const platform::windows::RealClockScope real_clock;
    if (budget.exhausted()) {
        return false;
    }
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
    if (new_owner || state.refresh_requested || !cache.relations) {
        begin_inspection_relations(cache, *owner);
        changed = true;
    }
    const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    if (new_owner || now >= state.color_after || state.restore.pending) {
        refresh_inspected_color(cache, *owner);
        state.color_after = now + std::chrono::milliseconds(250);
    }
    if (state.stage == InspectionStage::Idle) {
        return changed || previous_color != cache.faction_color;
    }
    if (!inspection_pointer_unchanged(state.direct_slot, state.direct_pointer) ||
        !inspection_pointer_unchanged(state.synergy_slot, state.synergy_pointer)) {
        state.stage = InspectionStage::Idle;
        changed = publish_inspection_status(cache, context.epoch, officer_id, RelationStatus::ReadFailed);
        return changed || previous_color != cache.faction_color;
    }
    if (state.stage == InspectionStage::Affinity) {
        discard_stale_inspection_affinity(state);
    }
    for (std::size_t work = 0; work < INSPECTION_WORK_ITEMS && !budget.exhausted(); ++work) {
        switch (state.stage) {
        case InspectionStage::DirectCandidate: inspect_relation_candidate(state, false); break;
        case InspectionStage::DirectValidate: validate_inspection_candidate(state, false); break;
        case InspectionStage::DirectScan: inspect_direct_relation(state); break;
        case InspectionStage::SynergyCandidate: inspect_relation_candidate(state, true); break;
        case InspectionStage::SynergyValidate: validate_inspection_candidate(state, true); break;
        case InspectionStage::SynergyScan: inspect_synergy_relation(state); break;
        case InspectionStage::Affinity: inspect_relation_affinity(state); break;
        case InspectionStage::Publish:
            if (!same_inspection_owner(*state.owner, scan, context) ||
                !inspection_pointer_unchanged(state.direct_slot, state.direct_pointer) ||
                !inspection_pointer_unchanged(state.synergy_slot, state.synergy_pointer)) {
                changed = publish_inspection_status(cache, context.epoch, officer_id, RelationStatus::ReadFailed);
            } else if (discard_stale_inspection_affinity(state)) {
                // Clear optional affinity values in bounded slices before
                // publishing a result gathered across different data centers.
                break;
            } else {
                state.building.status = state.direct_ready && state.synergy_ready ? RelationStatus::Ready :
                    state.direct_ready || state.synergy_ready ? RelationStatus::Partial :
                    state.direct_read_failed || state.synergy_read_failed ? RelationStatus::ReadFailed : RelationStatus::Unavailable;
                cache.relations = own::make_shared<domain::OfficerRelations>(state.building);
                changed = true;
            }
            state.stage = InspectionStage::Idle;
            return changed || previous_color != cache.faction_color;
        case InspectionStage::Idle:
            return changed || previous_color != cache.faction_color;
        }
    }
    return changed || previous_color != cache.faction_color;
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
