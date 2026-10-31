#include "native_officer_names.h"

#include "officer_roster.h"
#include "../platform/windows/process_memory.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <span>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

namespace s8::game {
namespace {

// Verified original entry points in Social/ChildNameDiagnostics.h at
// WOOSEOK99/S8RPKCheats@068ca1d. These are PersonData methods, not live-officer
// substitutes: call them only on the validated first roster record per ID.
constexpr std::uintptr_t GET_NAME = 0x01713db0;
constexpr std::uintptr_t GET_COURTESY = 0x01712a80;
constexpr std::uintptr_t SCENARIO_POINTER = 0x02e98bc8;
constexpr std::uintptr_t OFFICER_STRIDE = 0x3d0;
constexpr std::uint16_t OFFICER_COUNT = 5102;
constexpr std::uint16_t CHILD_FIRST = 4001;
constexpr std::uint64_t FULL_PERIOD_MS = 30000;
constexpr std::uint64_t CHILD_PERIOD_MS = 2000;
constexpr std::chrono::microseconds READ_BUDGET{600};

template<typename T>
bool read_value(std::uintptr_t address, T& value) {
    return platform::windows::read_memory(address, std::as_writable_bytes(std::span(&value, 1)));
}

bool executable_in_module(std::uintptr_t module, std::uintptr_t function) {
    MEMORY_BASIC_INFORMATION region{};

    if (VirtualQuery(reinterpret_cast<const void*>(function), &region, sizeof(region)) != sizeof(region) ||
        region.State != MEM_COMMIT || region.Type != MEM_IMAGE ||
        reinterpret_cast<std::uintptr_t>(region.AllocationBase) != module ||
        (region.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0) {
        return false;
    }
    const DWORD protection = region.Protect & 0xff;

    return protection == PAGE_EXECUTE || protection == PAGE_EXECUTE_READ ||
        protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY;
}

bool copy_native_text(std::uintptr_t function, std::uintptr_t officer, std::array<char, 192>& output) {
    using NativeGetter = const wchar_t* (__fastcall*)(const void*);
    const NativeGetter getter = reinterpret_cast<NativeGetter>(function);
    std::array<wchar_t, 64> wide{};
    std::size_t length = 0;

    __try {
        const wchar_t* text = getter(reinterpret_cast<const void*>(officer));

        if (text == nullptr) {
            return false;
        }
        for (; length < wide.size(); ++length) {
            wide[length] = text[length];
            if (wide[length] == L'\0') {
                break;
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
    if (length == 0 || length == wide.size()) {
        return false;
    }
    const int bytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide.data(),
        static_cast<int>(length), output.data(), static_cast<int>(output.size() - 1), nullptr, nullptr);

    if (bytes <= 0) {
        return false;
    }
    output[static_cast<std::size_t>(bytes)] = '\0';
    return true;
}

bool read_name(std::uintptr_t module, std::uintptr_t officer, std::uint16_t expected,
    domain::NativeOfficerName& result) {
    std::uint16_t id = 0;
    std::array<char, 192> name{};
    std::array<char, 192> courtesy{};

    if (!platform::windows::is_committed_memory(officer, OFFICER_STRIDE) ||
        !read_value(officer + 8, id) || id != expected ||
        !copy_native_text(module + GET_NAME, officer, name)) {
        return false;
    }
    const bool has_courtesy = copy_native_text(module + GET_COURTESY, officer, courtesy) &&
        std::strcmp(name.data(), courtesy.data()) != 0;
    const std::size_t name_size = std::strlen(name.data());
    const std::size_t courtesy_size = has_courtesy ? std::strlen(courtesy.data()) : 0;
    const std::size_t total = name_size + (has_courtesy ? courtesy_size + 2 : 0);

    if (total >= result.text.size() || !read_value(officer + 8, id) || id != expected) {
        return false;
    }
    result.id = expected;
    result.text.fill('\0');
    std::memcpy(result.text.data(), name.data(), name_size);
    if (has_courtesy) {
        result.text[name_size] = '(';
        std::memcpy(result.text.data() + name_size + 1, courtesy.data(), courtesy_size);
        result.text[total - 1] = ')';
    }
    return true;
}

bool current_roots(const NativeOfficerNameContext& context, std::uintptr_t& roster, std::uintptr_t& scenario) {
    std::uintptr_t root = 0;

    if (context.module == 0 || context.game_base <= 0x10000 || context.game_base_pointer == 0 ||
        !read_value(context.game_base_pointer, root) || root != context.game_base ||
        !read_value(context.module + SCENARIO_POINTER, scenario) || scenario <= 0x10000) {
        return false;
    }
    roster = resolve_officer_roster_base(context.module);
    return roster > 0x10000;
}

void reset_context(NativeOfficerNames& state, const NativeOfficerNameContext& context,
    std::uintptr_t roster, std::uintptr_t scenario) {
    state.epoch = context.epoch;
    state.module = context.module;
    state.game_base = context.game_base;
    state.roster = roster;
    state.scenario = scenario;
    state.last_full_ms = 0;
    state.last_children_ms = 0;
    state.cursor = 0;
    state.child_cursor = 0;
    state.scanning = false;
    state.polling_children = false;
    state.child_names_read = false;
    state.seen.reset();
    state.child_slots.fill(0);
    ++state.generation;
    state.output = {};
    state.output.epoch = context.epoch;
    state.output.generation = state.generation;
    state.output.reset = true;
    state.output.publish = true;
    state.pending = true;
}

void append_name(NativeOfficerNames& state, std::uint16_t slot, std::uint16_t id) {
    if (state.output.count == state.output.entries.size()) {
        return;
    }
    if (read_name(state.module, state.roster + static_cast<std::uintptr_t>(slot - 1) * OFFICER_STRIDE,
        id, state.output.entries[state.output.count])) {
        ++state.output.count;
    }
}

} // namespace

void update_native_officer_names(NativeOfficerNames& state, const NativeOfficerNameContext& context) {
    std::uintptr_t roster = 0;
    std::uintptr_t scenario = 0;
    const bool roots_ready = current_roots(context, roster, scenario);

    if (!roots_ready) {
        roster = 0;
        scenario = 0;
    }
    if (state.epoch != context.epoch || state.module != context.module ||
        state.game_base != context.game_base || state.roster != roster || state.scenario != scenario) {
        reset_context(state, context, roster, scenario);
        return;
    }
    if (state.pending || !roots_ready) {
        return;
    }
    if (state.prepared_module != context.module) {
        state.prepared_module = context.module;
        state.getters_ready = state.layout_supported && executable_in_module(context.module, context.module + GET_NAME) &&
            executable_in_module(context.module, context.module + GET_COURTESY);
    }
    if (!state.getters_ready) {
        return;
    }
    state.output.count = 0;
    state.output.reset = false;
    state.output.publish = false;
    std::size_t read = 0;
    const std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::now() + READ_BUDGET;

    if (!state.scanning && (state.last_full_ms == 0 || context.now_ms - state.last_full_ms >= FULL_PERIOD_MS)) {
        state.scanning = true;
        state.cursor = 0;
        state.seen.reset();
        // Keep known child slots while this full pass advances in chunks.
        // The two-second poll revalidates each ID before using its slot, and
        // the first matching record in this pass replaces the cached slot.
    }
    if (!state.polling_children &&
        (state.last_children_ms == 0 || context.now_ms - state.last_children_ms >= CHILD_PERIOD_MS)) {
        state.polling_children = true;
        state.child_cursor = 0;
        state.child_names_read = false;
    }
    if (state.polling_children) {
        // Share the full scan's budget and resume this cursor after a yield.
        // Partial value batches stay pending until the service accepts them.
        while (state.child_cursor < state.child_slots.size() && read < domain::NATIVE_NAME_BATCH_CAPACITY &&
            std::chrono::steady_clock::now() < deadline) {
            const std::size_t index = state.child_cursor++;
            const std::uint16_t slot = state.child_slots[index];
            std::uint16_t birth = 0;

            ++read;
            if (slot != 0 && read_value(state.roster + static_cast<std::uintptr_t>(slot - 1) * OFFICER_STRIDE + 0x34,
                birth) && birth != 0) {
                const std::size_t before = state.output.count;

                append_name(state, slot, static_cast<std::uint16_t>(CHILD_FIRST + index));
                state.child_names_read = state.child_names_read || state.output.count != before;
            }
        }
        if (state.child_cursor == state.child_slots.size()) {
            state.polling_children = false;
            state.last_children_ms = context.now_ms;
            // Publish the completed child poll even when its last chunk had
            // no name: earlier chunks may already be waiting in the service.
            state.output.publish = state.child_names_read;
        }
    }
    if (state.scanning) {
        while (state.cursor < OFFICER_COUNT && read < domain::NATIVE_NAME_BATCH_CAPACITY &&
            std::chrono::steady_clock::now() < deadline) {
            const std::uint16_t slot = static_cast<std::uint16_t>(++state.cursor);
            const std::uintptr_t officer = state.roster + static_cast<std::uintptr_t>(slot - 1) * OFFICER_STRIDE;
            std::uint16_t id = 0;

            ++read;
            if (read_value(officer + 8, id) && id != 0 && id <= OFFICER_COUNT && !state.seen.test(id)) {
                state.seen.set(id);
                if (id >= CHILD_FIRST && id < CHILD_FIRST + state.child_slots.size()) {
                    state.child_slots[id - CHILD_FIRST] = slot;
                }
                append_name(state, slot, id);
            }
            if (std::chrono::steady_clock::now() >= deadline) {
                break;
            }
        }
        if (state.cursor == OFFICER_COUNT) {
            state.scanning = false;
            state.last_full_ms = context.now_ms;
            state.output.publish = true;
        }
    }
    std::uintptr_t current_roster = 0;
    std::uintptr_t current_scenario = 0;

    if (!current_roots(context, current_roster, current_scenario) ||
        current_roster != state.roster || current_scenario != state.scenario) {
        reset_context(state, context, 0, 0);
        return;
    }
    state.pending = state.output.count != 0 || state.output.publish;
}

} // namespace s8::game
