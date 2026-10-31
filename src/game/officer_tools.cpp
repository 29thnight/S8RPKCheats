#include "officer_tools.h"

#include "officer_roster.h"
#include "game_base.h"
#include "../platform/windows/process_memory.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <span>
#include <type_traits>
#include <utility>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

namespace s8::game {
namespace {

// Original: StatMonitor.cpp::ApplyBatchOfficerEdit,
// SelectOfficercapture_impl.inc::TickBatchRandomTraitJob, MonthCapture.cpp and
// MenuSectionsScenario.cpp's explicit demo-stop action.
constexpr std::size_t ROSTER_STRIDE = 0x3d0;
constexpr std::size_t ROSTER_BUDGET = 128;
constexpr std::size_t TRAINING_WRITE_BUDGET = 64;
constexpr std::size_t RANDOM_WRITE_BUDGET = 32;
constexpr std::size_t TRAIT_SCAN_BYTE_BUDGET = 512 * 1024;
constexpr std::size_t TRAIT_SCAN_REGION_BUDGET = 64;
constexpr std::uintptr_t SCENARIO_ROOT_OFFSET = 0x2e98bc8;

template <typename T>
bool read_value(std::uintptr_t address, T& value) {
    return platform::windows::read_memory(address, std::as_writable_bytes(std::span(&value, 1)));
}

std::uint16_t officer_id(std::uintptr_t officer) {
    std::uint16_t id = 0;

    return officer > 0x10000 && read_value(officer + 8, id) &&
        id > 0 && id <= domain::OFFICER_TOOL_LIMIT ? id : 0;
}

std::uintptr_t scenario_root(std::uintptr_t module) {
    std::uintptr_t root = 0;

    return module != 0 && read_value(module + SCENARIO_ROOT_OFFSET, root) && root > 0x10000 ? root : 0;
}

bool training_status(std::uint8_t status) {
    constexpr std::array<std::uint8_t, 11> STATUSES{
        0x18, 0x28, 0x38, 0x48, 0x58, 0x68, 0x78, 0x88, 0xe8, 0xd8, 0xc8};

    return std::find(STATUSES.begin(), STATUSES.end(), status) != STATUSES.end();
}

bool read_roster_record(std::uintptr_t officer, std::uint16_t& id, std::uint8_t& status) {
    std::array<std::byte, 0x10a> bytes{};

    // Covers every SafeReadRosterStats field in the original batch editor.
    if (!platform::windows::read_memory(officer, bytes)) {
        return false;
    }
    std::memcpy(&id, bytes.data() + 8, sizeof(id));
    status = std::to_integer<std::uint8_t>(bytes[0x10]);
    return id != 0 && id <= domain::OFFICER_TOOL_LIMIT;
}

bool job_identity(const OfficerTools& state, const OfficerToolContext& context) {
    return context.epoch == state.job_epoch && context.game_base == state.job_game_base &&
        state.job_roster != 0 && resolve_officer_roster_base(context.module) == state.job_roster &&
        officer_id(context.player) == state.job_hero;
}

bool begin_job(OfficerTools& state, const OfficerToolContext& context, domain::OfficerToolJob job) {
    const std::uintptr_t roster = resolve_officer_roster_base(context.module);

    if (state.running || context.game_base <= 0x10000 || roster == 0) {
        return false;
    }
    state.job_epoch = context.epoch;
    state.job_game_base = context.game_base;
    state.job_roster = roster;
    state.job_hero = officer_id(context.player);
    state.cursor = 0;
    state.field_cursor = 0;
    state.active_officer = 0;
    state.active_changed = false;
    state.active_failed = false;
    state.running = true;
    state.view.progress = {};
    state.view.progress.job = job;
    state.view.progress.status = domain::OfficerToolStatus::Preparing;
    return true;
}

void finish_job(OfficerTools& state, domain::OfficerToolStatus status) {
    if (state.active_officer != 0) {
        ++state.view.progress.processed;
        state.view.progress.changed += state.view.progress.job == domain::OfficerToolJob::Training
            ? (state.active_changed ? 1 : 0) : (state.active_filled != 0 ? 1 : 0);
        state.view.progress.failed += state.active_failed ? 1 : 0;
    }
    state.running = false;
    state.active_officer = 0;
    state.custom_traits = {};
    state.view.progress.status = status;
    ++state.view.revision;
}

domain::OfficerToolStatus set_scenario_date(const OfficerToolContext& context,
    const domain::SetScenarioDate& request) {
    if ((!request.write_year && !request.write_month) ||
        (request.write_month && (request.month < 1 || request.month > 12))) {
        return domain::OfficerToolStatus::InvalidRequest;
    }
    const std::uintptr_t root = scenario_root(context.module);
    std::array<std::byte, 3> before{};

    if (root == 0 || !platform::windows::read_memory(root + 0x72d0, before)) {
        return domain::OfficerToolStatus::Unavailable;
    }
    std::array<std::byte, 3> after = before;
    std::array<std::byte, 3> verify{};
    const std::size_t offset = request.write_year ? 0 : 2;
    const std::size_t count = request.write_year ? (request.write_month ? 3 : 2) : 1;

    if (request.write_year) {
        std::memcpy(after.data(), &request.year, sizeof(request.year));
    }
    if (request.write_month) {
        after[2] = static_cast<std::byte>(request.month);
    }
    const bool wrote = platform::windows::write_data_memory(root + 0x72d0 + offset,
        std::span(after).subspan(offset, count));

    if (wrote && platform::windows::read_memory(root + 0x72d0 + offset,
        std::span(verify).subspan(offset, count)) &&
        std::equal(after.begin() + offset, after.begin() + offset + count, verify.begin() + offset)) {
        return domain::OfficerToolStatus::Completed;
    }
    const bool restored = scenario_root(context.module) == root &&
        platform::windows::write_data_memory(root + 0x72d0 + offset, std::span(before).subspan(offset, count)) &&
        platform::windows::read_memory(root + 0x72d0 + offset, std::span(verify).subspan(offset, count)) &&
        std::equal(before.begin() + offset, before.begin() + offset + count, verify.begin() + offset);

    return restored ? domain::OfficerToolStatus::WriteFailed : domain::OfficerToolStatus::RollbackFailed;
}

void finish_training_officer(OfficerTools& state) {
    ++state.view.progress.processed;
    state.view.progress.changed += state.active_changed ? 1 : 0;
    state.view.progress.failed += state.active_failed ? 1 : 0;
    state.active_officer = 0;
    state.field_cursor = 0;
    ++state.cursor;
}

void tick_training(OfficerTools& state, const domain::WorkBudget& work_budget) {
    std::size_t slots_read = 0;
    std::size_t writes = 0;

    state.view.progress.status = domain::OfficerToolStatus::Applying;
    while (state.cursor < domain::OFFICER_TOOL_LIMIT && slots_read < 32 && writes < TRAINING_WRITE_BUDGET &&
        !work_budget.exhausted()) {
        const std::uintptr_t officer = state.job_roster + state.cursor * ROSTER_STRIDE;

        if (state.active_officer == 0) {
            std::uint16_t id = 0;
            std::uint8_t status = 0;

            ++slots_read;
            state.view.progress.scanned = static_cast<std::uint32_t>(state.cursor + 1);
            if (!read_roster_record(officer, id, status) || id != state.cursor + 1 || !training_status(status)) {
                ++state.cursor;
                continue;
            }
            ++state.view.progress.eligible;
            state.active_officer = id;
            state.active_changed = false;
            state.active_failed = false;
            state.field_cursor = 0;
        } else if (officer_id(officer) != state.active_officer) {
            state.active_failed = true;
            finish_training_officer(state);
            continue;
        }
        for (; state.field_cursor < 59 && writes < TRAINING_WRITE_BUDGET && !work_budget.exhausted(); ++state.field_cursor) {
            const bool skill = state.field_cursor < state.training.skills.size();
            const std::size_t index = skill ? state.field_cursor : state.field_cursor - state.training.skills.size();
            const domain::TrainingChoice& choice = skill ? state.training.skills[index] : state.training.tactics[index];

            if (!choice.enabled) {
                continue;
            }
            const std::uintptr_t address = officer + (skill ? 0x1d0 : 0x139) + index;
            std::uint8_t current = 0;

            ++writes;
            if (!read_value(address, current)) {
                state.active_failed = true;
                continue;
            }
            if (current != choice.level) {
                const bool changed = platform::windows::write_data_memory(address,
                    std::as_bytes(std::span(&choice.level, 1))) && read_value(address, current) && current == choice.level;

                state.active_changed = state.active_changed || changed;
                state.active_failed = state.active_failed || !changed;
            }
        }
        if (state.field_cursor == 59) {
            finish_training_officer(state);
        }
    }
    if (state.cursor == domain::OFFICER_TOOL_LIMIT) {
        finish_job(state, state.view.progress.failed == 0
            ? domain::OfficerToolStatus::Completed : domain::OfficerToolStatus::WriteFailed);
    }
}

bool base_trait(std::uint16_t id) {
    return (id >= 1 && id <= 70) || id == 201 || id == 202;
}

void add_requested_trait(OfficerTools& state, std::uint16_t id) {
    if (id == 0 || state.requested_traits.test(id)) {
        return;
    }
    state.requested_traits.set(id);
    state.pool[state.pool_count++] = id;
    state.view.progress.trait_candidates = static_cast<std::uint32_t>(state.pool_count);
}

void build_random_pool(OfficerTools& state, const domain::WorkBudget& work_budget) {
    std::size_t visited = 0;

    if (state.custom_traits) {
        for (; state.custom_cursor != state.custom_traits->end() && visited < ROSTER_BUDGET && !work_budget.exhausted();
            ++state.custom_cursor, ++visited) {
            const std::uint16_t id = state.custom_cursor->first;
            const domain::CustomTrait& trait = state.custom_cursor->second;

            if (!base_trait(id) && !trait.name.empty() && trait.grade >= 1 && trait.grade <= 3 &&
                state.random_request.grades[trait.grade - 1]) {
                add_requested_trait(state, id);
            }
        }
        if (state.custom_cursor != state.custom_traits->end()) {
            return;
        }
    }
    if (state.pool_count == 0) {
        finish_job(state, domain::OfficerToolStatus::Unavailable);
        return;
    }
    state.random_stage = RandomTraitStage::Roster;
    state.cursor = 0;
}

void collect_random_roster(OfficerTools& state, const domain::WorkBudget& work_budget) {
    const std::size_t end = std::min(state.cursor + ROSTER_BUDGET, domain::OFFICER_TOOL_LIMIT);

    for (; state.cursor < end && !work_budget.exhausted(); ++state.cursor) {
        const std::uintptr_t officer = state.job_roster + state.cursor * ROSTER_STRIDE;
        std::uint16_t id = 0;
        std::uint8_t status = 0;

        if (!read_roster_record(officer, id, status) || state.roster_slots[id] != 0 ||
            (state.random_request.officer_id != 0 && id != state.random_request.officer_id)) {
            continue;
        }
        state.roster_slots[id] = static_cast<std::uint16_t>(state.cursor + 1);
        if (state.view.progress.eligible == 0) {
            state.trait_vtable = resolve_officer_trait_vtable(officer, state.job_game_base);

            if (!platform::windows::is_committed_memory(state.trait_vtable, sizeof(std::uintptr_t))) {
                state.trait_vtable = 0;
            }
        }
        ++state.view.progress.eligible;
    }
    state.view.progress.scanned = static_cast<std::uint32_t>(state.cursor);
    if (state.cursor == domain::OFFICER_TOOL_LIMIT) {
        if (state.view.progress.eligible == 0) {
            finish_job(state, domain::OfficerToolStatus::Unavailable);
        } else {
            state.cursor = 1;
            state.random_stage = state.trait_vtable == 0 ? RandomTraitStage::Vtable : RandomTraitStage::Seed;
        }
    }
}

bool read_trait_identity(std::uintptr_t trait, std::uintptr_t& vtable, std::uint16_t& id) {
    std::array<std::byte, 10> bytes{};

    if (trait <= 0x10000 || !platform::windows::read_memory(trait, bytes)) {
        return false;
    }
    std::memcpy(&vtable, bytes.data(), sizeof(vtable));
    std::memcpy(&id, bytes.data() + 8, sizeof(id));
    return vtable > 0x10000 && id != 0;
}

void find_random_vtable(OfficerTools& state, const domain::WorkBudget& work_budget) {
    const std::size_t end = std::min(state.cursor + 32, state.roster_slots.size());

    for (; state.cursor < end && state.trait_vtable == 0 && !work_budget.exhausted(); ++state.cursor) {
        const std::uint16_t slot = state.roster_slots[state.cursor];
        std::array<std::uintptr_t, 3> traits{};

        if (slot == 0 || !read_value(state.job_roster + (slot - 1) * ROSTER_STRIDE + 0x88, traits)) {
            continue;
        }
        for (const std::uintptr_t trait : traits) {
            std::uintptr_t vtable = 0;
            std::uint16_t id = 0;

            if (read_trait_identity(trait, vtable, id) &&
                platform::windows::is_committed_memory(vtable, sizeof(std::uintptr_t))) {
                state.trait_vtable = vtable;
                break;
            }
        }
    }
    if (state.trait_vtable != 0) {
        state.random_stage = RandomTraitStage::Seed;
        state.pool_cursor = 0;
    } else if (state.cursor == state.roster_slots.size()) {
        finish_job(state, domain::OfficerToolStatus::Unavailable);
    }
}

void seed_random_objects(OfficerTools& state, const domain::WorkBudget& work_budget) {
    const std::size_t end = std::min(state.pool_cursor + ROSTER_BUDGET, state.pool_count);

    for (; state.pool_cursor < end && !work_budget.exhausted(); ++state.pool_cursor) {
        const std::uint16_t requested = state.pool[state.pool_cursor];
        std::uintptr_t trait = 0;
        std::uintptr_t vtable = 0;
        std::uint16_t actual_id = 0;

        if (read_value(state.job_game_base + 0x57a4d0 + requested * sizeof(std::uintptr_t), trait) &&
            read_trait_identity(trait, vtable, actual_id) && actual_id == requested && vtable == state.trait_vtable) {
            state.trait_objects[requested] = trait;
            ++state.view.progress.trait_objects;
        }
    }
    if (state.pool_cursor == state.pool_count) {
        state.pool_cursor = 0;
        state.compact_count = 0;
        state.random_stage = state.view.progress.trait_objects == state.pool_count
            ? RandomTraitStage::Compact : RandomTraitStage::Search;
    }
}

void scan_random_objects(OfficerTools& state, const domain::WorkBudget& work_budget) {
    SYSTEM_INFO system{};
    std::size_t scanned = 0;
    std::size_t regions = 0;
    const std::uintptr_t own_begin = reinterpret_cast<std::uintptr_t>(&state);
    const std::uintptr_t own_end = own_begin + sizeof(state);
    const std::uintptr_t stack_marker = reinterpret_cast<std::uintptr_t>(&scanned);

    GetSystemInfo(&system);
    const std::uintptr_t minimum = reinterpret_cast<std::uintptr_t>(system.lpMinimumApplicationAddress);
    const std::uintptr_t maximum = reinterpret_cast<std::uintptr_t>(system.lpMaximumApplicationAddress);

    state.trait_scan_address = std::max(state.trait_scan_address, minimum);
    while (state.trait_scan_address < maximum && scanned < TRAIT_SCAN_BYTE_BUDGET &&
        regions < TRAIT_SCAN_REGION_BUDGET && state.view.progress.trait_objects < state.pool_count &&
        !work_budget.exhausted()) {
        MEMORY_BASIC_INFORMATION region{};

        if (!VirtualQuery(reinterpret_cast<const void*>(state.trait_scan_address), &region, sizeof(region))) {
            state.trait_scan_address = maximum;
            break;
        }
        ++regions;
        const std::uintptr_t begin = reinterpret_cast<std::uintptr_t>(region.BaseAddress);
        const std::uintptr_t end = begin + region.RegionSize;
        const DWORD protection = region.Protect & 0xff;
        const bool readable = protection == PAGE_READONLY || protection == PAGE_READWRITE ||
            protection == PAGE_WRITECOPY || protection == PAGE_EXECUTE_READ ||
            protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY;

        if (end <= state.trait_scan_address) {
            state.trait_scan_address = maximum;
            break;
        }
        // Copies in this job's scratch buffer and its executing stack are not
        // game objects, even when their first ten bytes match a trait record.
        if (region.State != MEM_COMMIT || !readable || (region.Protect & PAGE_GUARD) ||
            (stack_marker >= begin && stack_marker < end)) {
            state.trait_scan_address = end;
            continue;
        }
        state.trait_scan_address = (state.trait_scan_address + 7) & ~std::uintptr_t{7};
        if (end - state.trait_scan_address < 10) {
            state.trait_scan_address = end;
            continue;
        }
        const std::size_t amount = std::min({std::size_t{16384}, TRAIT_SCAN_BYTE_BUDGET - scanned,
            static_cast<std::size_t>(end - state.trait_scan_address - 2)});
        const std::size_t candidates = amount / 8;

        if (candidates == 0) {
            break;
        }
        std::size_t processed = 0;

        if (platform::windows::read_memory(state.trait_scan_address,
            std::span(state.scan_buffer).first(candidates * 8 + 2))) {
            for (; processed < candidates && !work_budget.exhausted(); ++processed) {
                const std::uintptr_t address = state.trait_scan_address + processed * 8;
                std::uintptr_t vtable = 0;
                std::uint16_t id = 0;

                if (address >= own_begin && address < own_end) {
                    continue;
                }
                std::memcpy(&vtable, state.scan_buffer.data() + processed * 8, sizeof(vtable));
                if (vtable != state.trait_vtable) {
                    continue;
                }
                std::memcpy(&id, state.scan_buffer.data() + processed * 8 + 8, sizeof(id));
                if (state.requested_traits.test(id) && state.trait_objects[id] == 0) {
                    state.trait_objects[id] = address;
                    ++state.view.progress.trait_objects;
                }
            }
        } else {
            processed = candidates;
        }
        state.trait_scan_address += processed * 8;
        scanned += processed * 8;
    }
    if (state.trait_scan_address >= maximum || state.view.progress.trait_objects == state.pool_count) {
        state.random_stage = RandomTraitStage::Compact;
        state.pool_cursor = 0;
        state.compact_count = 0;
    }
}

void compact_random_pool(OfficerTools& state, const domain::WorkBudget& work_budget) {
    const std::size_t end = std::min(state.pool_cursor + ROSTER_BUDGET, state.pool_count);

    for (; state.pool_cursor < end && !work_budget.exhausted(); ++state.pool_cursor) {
        const std::uint16_t id = state.pool[state.pool_cursor];

        if (state.trait_objects[id] != 0) {
            state.pool[state.compact_count++] = id;
        }
    }
    if (state.pool_cursor == state.pool_count) {
        state.pool_count = state.compact_count;
        if (state.pool_count == 0) {
            finish_job(state, domain::OfficerToolStatus::Unavailable);
        } else {
            state.random_stage = RandomTraitStage::Apply;
            state.cursor = 1;
            state.active_officer = 0;
            state.view.progress.status = domain::OfficerToolStatus::Applying;
        }
    }
}

bool current_trait_ids(std::uintptr_t officer, std::array<std::uint16_t, 3>& ids, std::size_t& empty_slot) {
    std::array<std::uintptr_t, 3> traits{};

    if (!read_value(officer + 0x88, traits)) {
        return false;
    }
    empty_slot = traits.size();
    for (std::size_t i = 0; i < traits.size(); ++i) {
        ids[i] = 0;
        if (traits[i] == 0) {
            empty_slot = std::min(empty_slot, i);
        } else {
            std::uintptr_t vtable = 0;

            // A failed read is not an empty slot. Preserve the existing pointer.
            if (!read_trait_identity(traits[i], vtable, ids[i])) {
                return false;
            }
        }
    }
    return true;
}

void finish_random_officer(OfficerTools& state, bool full) {
    ++state.view.progress.processed;
    state.view.progress.already_full += full ? 1 : 0;
    state.view.progress.changed += state.active_filled != 0 ? 1 : 0;
    state.view.progress.failed += state.active_failed ? 1 : 0;
    state.active_officer = 0;
    state.active_filled = 0;
    ++state.cursor;
}

void apply_random_traits(OfficerTools& state, const domain::WorkBudget& work_budget) {
    std::size_t visited = 0;
    std::size_t attempts = 0;

    while (state.cursor < state.roster_slots.size() && visited < ROSTER_BUDGET &&
        attempts < RANDOM_WRITE_BUDGET && !work_budget.exhausted()) {
        const std::uint16_t slot = state.roster_slots[state.cursor];

        if (slot == 0) {
            ++state.cursor;
            ++visited;
            continue;
        }
        const std::uintptr_t officer = state.job_roster + (slot - 1) * ROSTER_STRIDE;
        std::array<std::uint16_t, 3> current_ids{};
        std::size_t empty_slot = 0;

        if (state.active_officer == 0) {
            state.active_officer = static_cast<std::uint16_t>(state.cursor);
            state.active_filled = 0;
            state.active_failed = false;
            state.random_remaining = state.pool_count;
        }
        ++attempts;
        if (officer_id(officer) != state.active_officer || !current_trait_ids(officer, current_ids, empty_slot)) {
            state.active_failed = true;
            finish_random_officer(state, false);
            ++visited;
            continue;
        }
        if (empty_slot == current_ids.size()) {
            finish_random_officer(state, state.active_filled == 0);
            ++visited;
            continue;
        }
        if (state.random_remaining == 0) {
            state.active_failed = true;
            finish_random_officer(state, false);
            ++visited;
            continue;
        }
        std::uniform_int_distribution<std::size_t> choose(0, state.random_remaining - 1);
        const std::size_t index = choose(state.random);
        const std::uint16_t trait = state.pool[index];

        // Draw without replacement by moving the chosen item to the used tail.
        // Reusing the permutation avoids allocating/shuffling a vector per officer.
        std::swap(state.pool[index], state.pool[state.random_remaining - 1]);
        --state.random_remaining;
        if (std::find(current_ids.begin(), current_ids.end(), trait) != current_ids.end()) {
            continue;
        }
        if (set_officer_trait_object(officer, static_cast<std::uint8_t>(empty_slot), trait,
            state.trait_objects[trait], state.trait_vtable)) {
            ++state.active_filled;
            ++state.view.progress.filled_slots;
        } else {
            state.active_failed = true;
        }
    }
    if (state.cursor == state.roster_slots.size()) {
        finish_job(state, state.view.progress.failed == 0
            ? domain::OfficerToolStatus::Completed : domain::OfficerToolStatus::WriteFailed);
    }
}

bool demo_roots_match(const DemoRestoreIdentity& saved, const OfficerToolContext& context) {
    return saved.live_officer != 0 && context.game_base_pointer != 0 && saved.module == context.module &&
        context.game_base == saved.game_base && read_game_base(context.game_base_pointer) == saved.game_base &&
        resolve_officer_roster_base(context.module) == saved.roster && scenario_root(context.module) == saved.scenario;
}

bool demo_object_matches(const DemoRestoreIdentity& saved) {
    std::uintptr_t vtable = 0;
    std::uintptr_t first_virtual = 0;

    return officer_id(saved.live_officer) == saved.officer_id &&
        platform::windows::is_committed_memory(saved.live_officer, 0x200) &&
        read_value(saved.live_officer, vtable) && vtable == saved.vtable &&
        read_value(vtable, first_virtual) && first_virtual == saved.first_virtual;
}

void observe_demo_identity(OfficerTools& state, const OfficerToolContext& context) {
    const std::uintptr_t roster = resolve_officer_roster_base(context.module);
    const std::uintptr_t scenario = scenario_root(context.module);
    const std::uint16_t hero = officer_id(context.player);

    if (context.game_base_pointer == 0 || context.game_base <= 0x10000 || roster == 0 || scenario == 0 ||
        read_game_base(context.game_base_pointer) != context.game_base) {
        state.demo = {};
    } else if (hero != 0) {
        DemoRestoreIdentity observed;

        observed.module = context.module;
        observed.game_base = context.game_base;
        observed.roster = roster;
        observed.scenario = scenario;
        observed.live_officer = context.player;
        observed.officer_id = hero;
        if (read_value(context.player, observed.vtable) && observed.vtable > 0x10000 &&
            read_value(observed.vtable, observed.first_virtual) && observed.first_virtual > 0x10000 &&
            platform::windows::is_committed_memory(context.player, 0x200)) {
            state.demo = observed;
        } else {
            state.demo = {};
        }
    } else if (!demo_roots_match(state.demo, context) || !demo_object_matches(state.demo)) {
        state.demo = {};
    }
    state.view.demo_ready = state.demo.live_officer != 0 && hero == 0;
    state.view.demo_officer = state.demo.officer_id;
}

domain::OfficerToolStatus stop_demo_play(OfficerTools& state, const OfficerToolContext& context,
    const domain::StopDemoPlay& request) {
    std::uintptr_t current = 0;

    if (request.officer_id == 0 || request.officer_id != state.demo.officer_id ||
        !demo_roots_match(state.demo, context) || !demo_object_matches(state.demo) ||
        !read_value(context.game_base + 0xe0, current)) {
        return domain::OfficerToolStatus::Unavailable;
    }
    if (officer_id(current) != 0 && current != state.demo.live_officer) {
        return domain::OfficerToolStatus::Conflict;
    }
    if (current == state.demo.live_officer) {
        return domain::OfficerToolStatus::Completed;
    }
    const std::uintptr_t target = state.demo.live_officer;
    std::uintptr_t verify = 0;
    const bool wrote = platform::windows::write_data_memory(context.game_base + 0xe0,
        std::as_bytes(std::span(&target, 1)));

    if (wrote && read_value(context.game_base + 0xe0, verify) && verify == target) {
        state.view.demo_ready = false;
        return domain::OfficerToolStatus::Completed;
    }
    const bool restored = demo_roots_match(state.demo, context) &&
        platform::windows::write_data_memory(context.game_base + 0xe0, std::as_bytes(std::span(&current, 1))) &&
        read_value(context.game_base + 0xe0, verify) && verify == current;

    return restored ? domain::OfficerToolStatus::WriteFailed : domain::OfficerToolStatus::RollbackFailed;
}

} // namespace

bool read_scenario_date(std::uintptr_t module, std::uint16_t& year, std::uint8_t& month) {
    const std::uintptr_t root = scenario_root(module);
    std::array<std::byte, 3> bytes{};

    if (root == 0 || !platform::windows::read_memory(root + 0x72d0, bytes)) {
        return false;
    }
    std::memcpy(&year, bytes.data(), sizeof(year));
    month = std::to_integer<std::uint8_t>(bytes[2]);
    return true;
}

void reset_officer_tools(OfficerTools& state, domain::GameEpoch epoch) {
    const bool interrupted = state.running;

    if (interrupted) {
        finish_job(state, domain::OfficerToolStatus::SessionChanged);
    }
    const domain::OfficerToolProgress previous = state.view.progress;
    const std::uint64_t revision = state.view.revision + 1;

    state.view = {};
    state.view.epoch = epoch;
    state.view.revision = revision;
    if (interrupted) {
        state.view.progress = previous;
        state.view.progress.status = domain::OfficerToolStatus::SessionChanged;
    }
    state.running = false;
    state.active_officer = 0;
    state.cursor = 0;
    state.pool_count = 0;
    state.pool_cursor = 0;
    state.trait_vtable = 0;
    state.trait_scan_address = 0;
    state.custom_traits = {};
    state.last_date_ms = 0;
    state.changed_pending = true;
    reset_family_tools(state.family, epoch);
    // Losing P1 also increments the global epoch when demo play starts. Keep
    // this separate observation only until observe_demo_identity revalidates
    // the game/roster/scenario roots and the exact live object's ID and vtable.
    // It never authorizes an old batch job or substitutes a roster master.
}

void stop_officer_tools(OfficerTools& state) {
    if (state.running) {
        finish_job(state, domain::OfficerToolStatus::Cancelled);
    }
    reset_family_tools(state.family, state.view.epoch);
    state.view.family = state.family.view;
    state.demo = {};
    state.view.demo_ready = false;
    state.view.demo_officer = 0;
    state.changed_pending = true;
    ++state.view.revision;
}

bool submit_officer_tool(OfficerTools& state, const OfficerToolContext& context,
    const domain::OfficerToolsRequest& request,
    own::shared_owner<const domain::CustomTraits> custom_traits) {
    if (request.epoch != context.epoch) {
        state.view.request_status = domain::OfficerToolStatus::SessionChanged;
        ++state.view.revision;
        state.changed_pending = true;
        return false;
    }
    if (state.view.epoch != context.epoch) {
        reset_officer_tools(state, context.epoch);
    }
    const domain::OfficerToolStatus result = std::visit([&]<typename T>(const T& action) -> domain::OfficerToolStatus {
        if constexpr (std::is_same_v<T, domain::BatchTrainingRequest>) {
            bool selected = false;

            for (const domain::TrainingChoice& choice : action.tactics) {
                selected = selected || choice.enabled;
                if (choice.enabled && choice.level > 3) {
                    return domain::OfficerToolStatus::InvalidRequest;
                }
            }
            for (const domain::TrainingChoice& choice : action.skills) {
                selected = selected || choice.enabled;
                if (choice.enabled && choice.level > 3) {
                    return domain::OfficerToolStatus::InvalidRequest;
                }
            }
            if (!selected) {
                return domain::OfficerToolStatus::InvalidRequest;
            }
            if (!begin_job(state, context, domain::OfficerToolJob::Training)) {
                return state.running ? domain::OfficerToolStatus::Conflict : domain::OfficerToolStatus::Unavailable;
            }
            state.training = action;
            return domain::OfficerToolStatus::Preparing;
        } else if constexpr (std::is_same_v<T, domain::RandomTraitsRequest>) {
            if ((!action.grades[0] && !action.grades[1] && !action.grades[2]) ||
                action.officer_id > domain::OFFICER_TOOL_LIMIT) {
                return domain::OfficerToolStatus::InvalidRequest;
            }
            if (!begin_job(state, context, domain::OfficerToolJob::RandomTraits)) {
                return state.running ? domain::OfficerToolStatus::Conflict : domain::OfficerToolStatus::Unavailable;
            }
            state.random_request = action;
            state.custom_traits = std::move(custom_traits);
            if (state.custom_traits) {
                state.custom_cursor = state.custom_traits->cbegin();
            }
            state.roster_slots.fill(0);
            state.requested_traits.reset();
            state.trait_objects.fill(0);
            state.pool_count = 0;
            state.pool_cursor = 0;
            state.compact_count = 0;
            state.trait_vtable = 0;
            state.trait_scan_address = 0;
            state.random_stage = RandomTraitStage::Pool;
            state.random.seed(static_cast<std::uint32_t>(context.now_ms ^ context.epoch.value));
            if (action.grades[0]) {
                for (std::uint16_t id = 1; id <= 70; ++id) {
                    add_requested_trait(state, id);
                }
                add_requested_trait(state, 201);
                add_requested_trait(state, 202);
            }
            return domain::OfficerToolStatus::Preparing;
        } else if constexpr (std::is_same_v<T, domain::CancelOfficerToolJob>) {
            if (state.running) {
                // Completed field edits remain; cancellation stops future writes.
                finish_job(state, domain::OfficerToolStatus::Cancelled);
            }
            return domain::OfficerToolStatus::Completed;
        } else if constexpr (std::is_same_v<T, domain::RefreshFamilyTools>) {
            request_family_tools_refresh(state.family);
            return domain::OfficerToolStatus::Preparing;
        } else if constexpr (std::is_same_v<T, domain::SetChildSchedule>) {
            return set_child_schedule(state.family, context, action);
        } else if constexpr (std::is_same_v<T, domain::SetPregnancySpouse>) {
            return set_pregnancy_spouse(state.family, context, action);
        } else if constexpr (std::is_same_v<T, domain::SetFamilyPage>) {
            set_family_page(state.family, action);
            return domain::OfficerToolStatus::Completed;
        } else if constexpr (std::is_same_v<T, domain::SetScenarioDate>) {
            return set_scenario_date(context, action);
        } else if constexpr (std::is_same_v<T, domain::StopDemoPlay>) {
            return stop_demo_play(state, context, action);
        }
    }, request.action);

    state.view.request_status = result;
    state.view.family = state.family.view;
    state.last_date_ms = 0;
    state.changed_pending = true;
    ++state.view.revision;
    return result == domain::OfficerToolStatus::Preparing || result == domain::OfficerToolStatus::Completed;
}

bool tick_officer_tools(OfficerTools& state, const OfficerToolContext& context,
    const domain::WorkBudget& work_budget) {
    if (work_budget.exhausted()) {
        return false;
    }
    const std::uint64_t revision = state.view.revision;
    bool changed = state.changed_pending;

    if (state.view.epoch != context.epoch) {
        reset_officer_tools(state, context.epoch);
        changed = true;
    }
    state.changed_pending = false;
    const bool previous_demo_ready = state.view.demo_ready;
    const std::uint16_t previous_demo_officer = state.view.demo_officer;
    const std::uint16_t hero = officer_id(context.player);

    observe_demo_identity(state, context);
    changed = changed || state.view.hero != hero || previous_demo_ready != state.view.demo_ready ||
        previous_demo_officer != state.view.demo_officer;
    state.view.hero = hero;
    if (state.last_date_ms == 0 || context.now_ms - state.last_date_ms >= 250) {
        std::uint16_t year = 0;
        std::uint8_t month = 0;
        const bool ready = read_scenario_date(context.module, year, month);

        changed = changed || ready != state.view.date_ready || year != state.view.year || month != state.view.month;
        state.view.date_ready = ready;
        state.view.year = year;
        state.view.month = month;
        state.last_date_ms = context.now_ms;
    }
    const bool family_first = state.family_first;

    state.family_first = !state.family_first;
    if (family_first && tick_family_tools(state.family, context, work_budget)) {
        state.view.family = state.family.view;
        changed = true;
    }
    if (state.running && !work_budget.exhausted()) {
        changed = true;
        if (!job_identity(state, context)) {
            finish_job(state, domain::OfficerToolStatus::SessionChanged);
        } else if (state.view.progress.job == domain::OfficerToolJob::Training) {
            tick_training(state, work_budget);
        } else {
            switch (state.random_stage) {
            case RandomTraitStage::Pool: build_random_pool(state, work_budget); break;
            case RandomTraitStage::Roster: collect_random_roster(state, work_budget); break;
            case RandomTraitStage::Vtable: find_random_vtable(state, work_budget); break;
            case RandomTraitStage::Seed: seed_random_objects(state, work_budget); break;
            case RandomTraitStage::Search: scan_random_objects(state, work_budget); break;
            case RandomTraitStage::Compact: compact_random_pool(state, work_budget); break;
            case RandomTraitStage::Apply: apply_random_traits(state, work_budget); break;
            }
        }
    }
    if (!family_first && tick_family_tools(state.family, context, work_budget)) {
        state.view.family = state.family.view;
        changed = true;
    }
    if (changed && revision == state.view.revision) {
        ++state.view.revision;
    }
    return changed || revision != state.view.revision;
}

} // namespace s8::game
