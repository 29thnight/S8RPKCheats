#include "council_execute_free.h"
#include "../platform/windows/code_memory.h"
#include "../platform/windows/process_memory.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <algorithm>
#include <atomic>

namespace s8::game {
namespace {

// WOOSEOK99/S8RPKCheats@068ca1d, War/CouncilExecuteFreeOfficers.cpp:
// the displaced CALL runs first; RDI is the acting officer, R15 is the list.
// The original four pushes and 0x30-byte call frame establish the callback ABI.
// A completed ID snapshot replaces the original 5200 native activity calls.
// New candidates appear after a bounded refresh; every published ID is resolved
// and checked again in the actual game callback before the native append.
constexpr std::array ORIGINAL = native_hex("E8531BE9FF");
constexpr std::array CAVE_TEMPLATE = native_hex(
    "E8000000005356415441554883EC304889F94C89FA48B80000000000000000FFD0"
    "4883C430415D415C5E5BE900000000");
constexpr std::uint32_t WRITER = std::uint32_t{1} << 31;
static_assert(CAVE_TEMPLATE.size() == 48);

std::atomic<std::uintptr_t> callback_base{0};
std::atomic<std::uint64_t> callback_epoch{0};
std::atomic<bool> callback_enabled{false};

struct CandidatePage {
    std::atomic<std::uint32_t> access{0};
    std::array<std::uint16_t, COUNCIL_OFFICER_SLOT_LIMIT> ids{};
    std::uintptr_t root = 0;
    std::uintptr_t hero = 0;
    std::uintptr_t force = 0;
    std::uint64_t epoch = 0;
    std::size_t count = 0;
};

std::array<CandidatePage, 2> published{};
std::atomic<std::size_t> published_index{0};

struct CandidateBorrow {
    CandidatePage* page = nullptr;

    ~CandidateBorrow() {
        if (page) {
            page->access.fetch_sub(1, std::memory_order_release);
        }
    }
};

template<typename T>
bool read_fast(std::uintptr_t address, T& value) {
    if (!address) {
        return false;
    }
    __try {
        value = *reinterpret_cast<const T*>(address);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool ruler_context(std::uintptr_t base, std::uintptr_t& root,
    std::uintptr_t& hero, std::uintptr_t& force) {
    std::uintptr_t status = 0;
    std::uint8_t kind = 0;

    return read_fast(base + 0x02e98bc8, root) && root &&
        read_fast(root + 0xe0, hero) && hero && read_fast(hero + 0x10, status) && status &&
        read_fast(status + 8, kind) && kind == 1 && read_fast(hero + 0x18, force) && force;
}

std::uintptr_t eligible_officer(std::uintptr_t root, std::uintptr_t force, std::uint16_t slot) {
    std::uintptr_t officer = 0;
    std::uintptr_t status = 0;
    std::uintptr_t officer_force = 0;
    std::uintptr_t city = 0;
    std::uintptr_t district = 0;
    std::uintptr_t city_force = 0;
    std::uint8_t kind = 0;

    if (!slot || slot > COUNCIL_OFFICER_SLOT_LIMIT ||
        !read_fast(root + 0x21bdf0 + static_cast<std::uintptr_t>(slot) * sizeof(officer), officer) || !officer ||
        !read_fast(officer + 0x10, status) || !status || !read_fast(status + 8, kind) || kind != 0x0a ||
        !read_fast(officer + 0x18, officer_force) || officer_force ||
        !read_fast(officer + 0x20, city) || !city || !read_fast(city + 0x90, district) || !district ||
        !read_fast(district + 0x10, city_force) || city_force != force) {
        return 0;
    }
    return officer;
}

bool active_officer(std::uintptr_t officer) {
    std::uintptr_t vtable = 0;
    std::uintptr_t target = 0;

    if (!read_fast(officer, vtable) || !vtable || !read_fast(vtable + 0x48, target) || !target) {
        return false;
    }

    using Active = std::uint8_t(__fastcall*)(std::uintptr_t);

    __try {
        return reinterpret_cast<Active>(target)(officer) != 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

CandidatePage* borrow_candidates() {
    for (std::size_t attempt = 0; attempt < published.size(); ++attempt) {
        const std::size_t index = published_index.load(std::memory_order_acquire);
        CandidatePage& page = published[index];
        std::uint32_t access = page.access.load(std::memory_order_acquire);

        if ((access & WRITER) != 0 || access == WRITER - 1 ||
            !page.access.compare_exchange_strong(access, access + 1, std::memory_order_acq_rel)) {
            continue;
        }
        if (index == published_index.load(std::memory_order_acquire)) {
            return &page;
        }
        page.access.fetch_sub(1, std::memory_order_release);
    }
    return nullptr;
}

bool publish_candidates(const CouncilExecuteFree& state) {
    const std::size_t index = 1 - published_index.load(std::memory_order_acquire);
    CandidatePage& page = published[index];
    std::uint32_t empty = 0;

    if (!page.access.compare_exchange_strong(empty, WRITER, std::memory_order_acq_rel)) {
        return false;
    }
    std::copy(state.candidates.begin(), state.candidates.begin() + state.candidate_count, page.ids.begin());
    page.root = state.scan_root;
    page.hero = state.scan_hero;
    page.force = state.scan_force;
    page.epoch = state.epoch.value;
    page.count = state.candidate_count;
    page.access.store(0, std::memory_order_release);
    published_index.store(index, std::memory_order_release);
    return true;
}

void __fastcall append_free_officers(std::uintptr_t acting_officer, std::uintptr_t list) {
    if (!list || !callback_enabled.load(std::memory_order_acquire)) {
        return;
    }

    const std::uintptr_t base = callback_base.load(std::memory_order_acquire);
    std::uintptr_t root = 0;
    std::uintptr_t hero = 0;
    std::uintptr_t force = 0;

    if (!base || !ruler_context(base, root, hero, force) || hero != acting_officer) {
        return;
    }

    const CandidateBorrow borrow{borrow_candidates()};

    if (!borrow.page || borrow.page->root != root || borrow.page->hero != hero ||
        borrow.page->force != force || borrow.page->epoch != callback_epoch.load(std::memory_order_acquire)) {
        return;
    }

    using Append = void(__fastcall*)(std::uintptr_t, std::uintptr_t*);
    const Append append = reinterpret_cast<Append>(base + 0x0001a0c0);

    for (std::size_t index = 0; index < borrow.page->count; ++index) {
        std::uintptr_t current_root = 0;

        if (borrow.page->epoch != callback_epoch.load(std::memory_order_acquire) ||
            !callback_enabled.load(std::memory_order_acquire) ||
            !read_fast(base + 0x02e98bc8, current_root) || current_root != root) {
            return;
        }

        std::uintptr_t officer = eligible_officer(root, force, borrow.page->ids[index]);

        if (officer && active_officer(officer) &&
            eligible_officer(root, force, borrow.page->ids[index]) == officer) {
            append(list, &officer);
        }
    }
}

bool prepare_council_execute(CouncilExecuteFree& state, std::uintptr_t base) {
    state.patches.last_result = platform::windows::CodePatchResult::ExpectedBytesMismatch;

    if (!native_bytes_match(base + 0x018c9ca8, ORIGINAL)) {
        return false;
    }

    state.patches.last_result = platform::windows::CodePatchResult::WriteFailed;

    const std::uintptr_t cave = platform::windows::allocate_code_near(base + 0x018c9ca8, 0x400);

    if (!cave) {
        return false;
    }

    std::array<std::byte, CAVE_TEMPLATE.size()> code = CAVE_TEMPLATE;
    NativePatchGroup patches{};

    if (!put_native_rel32(code, 1, cave + 1, base + 0x0175b800) ||
        !put_native_pointer(code, 23, reinterpret_cast<std::uintptr_t>(&append_free_officers)) ||
        !put_native_rel32(code, 44, cave + 44, base + 0x018c9cad) ||
        !append_native_branch(patches, base + 0x018c9ca8, cave, ORIGINAL) ||
        !platform::windows::write_code_memory(cave, code) ||
        !platform::windows::retain_callback_module(reinterpret_cast<std::uintptr_t>(&append_free_officers))) {
        platform::windows::free_code(cave);
        return false;
    }

    std::uintptr_t expected_base = 0;

    if (!callback_base.compare_exchange_strong(expected_base, base, std::memory_order_acq_rel) &&
        expected_base != base) {
        platform::windows::free_code(cave);
        return false;
    }
    state.module_base = base;
    state.cave = cave;
    state.patches = patches;
    return true;
}

} // namespace

void reset_council_execute_free(CouncilExecuteFree& state, domain::GameEpoch epoch) {
    callback_epoch.store(epoch.value, std::memory_order_release);
    state.epoch = epoch;
    state.scan_root = 0;
    state.scan_hero = 0;
    state.scan_force = 0;
    state.candidate_count = 0;
    state.next_slot = 1;
    state.ready = false;
    state.preparing_enable = false;
}

void update_council_execute_candidates(CouncilExecuteFree& state, std::uintptr_t module_base,
    domain::GameEpoch epoch, std::size_t slot_budget, const domain::WorkBudget& work_budget) {
    if (!module_base || (state.module_base && state.module_base != module_base) || !slot_budget) {
        return;
    }
    if (state.epoch != epoch) {
        reset_council_execute_free(state, epoch);
    }

    std::uintptr_t root = 0;
    std::uintptr_t hero = 0;
    std::uintptr_t force = 0;

    if (!ruler_context(module_base, root, hero, force) ||
        !platform::windows::is_committed_memory(root + 0x21bdf8,
            COUNCIL_OFFICER_SLOT_LIMIT * sizeof(std::uintptr_t))) {
        // An empty snapshot is ready too; non-rulers retain the native list.
        root = 0;
        hero = 0;
        force = 0;
    }
    if (state.scan_root != root || state.scan_hero != hero || state.scan_force != force) {
        state.scan_root = root;
        state.scan_hero = hero;
        state.scan_force = force;
        state.candidate_count = 0;
        state.next_slot = 1;
        state.ready = false;
    }
    if (!root) {
        state.next_slot = COUNCIL_OFFICER_SLOT_LIMIT + 1;
    }

    const std::size_t budget = std::min(slot_budget, COUNCIL_OFFICER_SLOT_LIMIT);
    const std::size_t end = std::min(state.next_slot + budget, COUNCIL_OFFICER_SLOT_LIMIT + 1);

    for (; state.next_slot < end && !work_budget.exhausted(); ++state.next_slot) {
        const std::uint16_t slot = static_cast<std::uint16_t>(state.next_slot);

        if (eligible_officer(root, force, slot)) {
            state.candidates[state.candidate_count++] = slot;
        }
    }
    if (state.next_slot > COUNCIL_OFFICER_SLOT_LIMIT) {
        std::uintptr_t current_root = 0;
        std::uintptr_t current_hero = 0;
        std::uintptr_t current_force = 0;

        if (root && (!ruler_context(module_base, current_root, current_hero, current_force) ||
            current_root != root || current_hero != hero || current_force != force)) {
            state.ready = false;
            state.next_slot = 1;
            state.candidate_count = 0;
            return;
        }
        if (publish_candidates(state)) {
            state.ready = true;
            state.next_slot = 1;
            state.candidate_count = 0;
        }
    }
}

bool set_council_execute_free(CouncilExecuteFree& state, std::uintptr_t module_base,
    domain::GameEpoch epoch, bool enabled) {
    if (!module_base || (state.module_base && state.module_base != module_base)) {
        state.patches.last_result = platform::windows::CodePatchResult::InvalidInput;
        return false;
    }
    if (!enabled) {
        if (!set_native_patch_group(state.patches, false)) {
            return false;
        }
        callback_enabled.store(false, std::memory_order_release);
        reset_council_execute_free(state, epoch);
        return true;
    }
    if (!state.cave && !prepare_council_execute(state, module_base)) {
        return false;
    }
    if (!state.patches.applied) {
        if (!state.preparing_enable) {
            reset_council_execute_free(state, epoch);
            state.preparing_enable = true;
        }
        // Preparation is bounded too. The owner keeps the request Pending and
        // advances the same scan between commit attempts; no hook is published
        // until a complete candidate snapshot exists.
        update_council_execute_candidates(state, module_base, epoch);
        if (!state.ready) {
            state.patches.last_result = platform::windows::CodePatchResult::Busy;
            return false;
        }
    }

    const bool previously_enabled = state.patches.applied;

    callback_enabled.store(true, std::memory_order_release);
    if (!set_native_patch_group(state.patches, true)) {
        callback_enabled.store(previously_enabled, std::memory_order_release);
        return false;
    }
    state.preparing_enable = false;
    return true;
}

} // namespace s8::game
