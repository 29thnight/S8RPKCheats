#include "stratagem5_native.h"
#include "stratagem5_native_code.h"
#include "stratagem5_model.h"
#include "../platform/windows/process_memory.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <array>
#include <atomic>
#include <cstring>

namespace s8::game {
namespace {

using stratagem5_native_detail::Admission;
using InitLayouts = void(__fastcall*)(std::uintptr_t, const void*, int, std::uintptr_t);
using LookupLayout = std::uintptr_t(__fastcall*)(std::uintptr_t, int);
using RegisterLayout = void(__fastcall*)(std::uintptr_t, int, std::uintptr_t, int, int);
using GetMemoryManager = std::uintptr_t(__fastcall*)();
using Allocate = std::uintptr_t(__fastcall*)(std::uintptr_t, std::size_t, void*);
using ConstructButton = std::uintptr_t(__fastcall*)(std::uintptr_t);
using InitializeButton = void(__fastcall*)(std::uintptr_t, int, int, std::uintptr_t);
using SetTrickId = void(__fastcall*)(std::uintptr_t, std::uint8_t);
using SetPosition = void(__fastcall*)(std::uintptr_t, int, int);
using SetVisible = void(__fastcall*)(std::uintptr_t, bool);
using SetU32 = void(__fastcall*)(std::uintptr_t, std::uint32_t);
using SetU64 = void(__fastcall*)(std::uintptr_t, std::uint64_t);

constexpr std::size_t BUTTON_LEDGER_CAPACITY = STRATAGEM5_NATIVE_QUARANTINE_LIMIT;
constexpr std::size_t NO_BUTTON_RECORD = BUTTON_LEDGER_CAPACITY;
constexpr std::size_t BUTTON_SIZE = 0x1d8;
constexpr std::uint32_t UNREGISTERED_BUTTON = 0xffffffffu;
constexpr std::uint32_t READY_BUTTON = 1;

enum class ButtonStage : std::uint8_t {
    Empty,
    Allocating,
    Allocated,
    Constructing,
    Constructed,
    Initializing,
    Initialized,
    Prepared,
    Registering,
    Registered
};

struct ButtonRecord {
    // Owner-thread only. Volatile keeps stage/handle stores observable after
    // Windows SEH from a foreign call; it is not a synchronization primitive.
    volatile std::uintptr_t allocator = 0;
    volatile std::uintptr_t raw = 0;
    volatile std::uintptr_t button = 0;
    std::uintptr_t layout = 0;
    std::uintptr_t original_vtable = 0;
    std::array<std::uintptr_t, 4> original_buttons{};
    std::uint64_t epoch = 0;
    std::uint64_t generation = 0;
    volatile ButtonStage stage = ButtonStage::Empty;
    bool retired = false;
    bool quarantined = false;
};

struct LayoutGeneration {
    std::uintptr_t layout = 0;
    std::uintptr_t descriptors = 0;
    std::uintptr_t helpers = 0;
    std::uintptr_t helper = 0;
    std::uintptr_t helper_vtable = 0;
    std::array<std::uint32_t, 5> descriptor_prefix{};
    std::array<std::uintptr_t, 4> original_buttons{};
    std::uintptr_t button = 0;
    std::size_t button_record = NO_BUTTON_RECORD;
    bool helper_valid = false;
    bool attempted = false;
    bool initialized = false;
    bool registered = false;
    bool registration_hidden = false;
};

struct NativeState {
    stratagem5_native_detail::Code code;
    Stratagem5Native* owner = nullptr;
    Stratagem5NativeCallbacks callbacks;
    Stratagem5NativeView view;
    LayoutGeneration layout;
    // Value-owned bookkeeping, not a C++ owner of foreign game allocations.
    // Uncertain entries never recycle; confirmed registry transfers recycle
    // only after the corresponding native generation retires.
    std::array<ButtonRecord, BUTTON_LEDGER_CAPACITY> buttons{};
    std::uintptr_t dialog = 0;
    std::uint64_t epoch = 0;
    bool metadata = false;
    bool quiesced = false;
    bool control_active = false;
    std::atomic<Admission> admission{Admission::Closed};
    std::atomic<unsigned> active{0};
    std::atomic<DWORD> thread{0};
    std::atomic<std::uintptr_t> original_init{0};
    // These atomics are also read/written by retained x64 stubs. Their storage
    // is static and naturally aligned; no entry points refer to a Runtime.
    std::atomic<std::uintptr_t> active_layout{0};
    std::atomic<std::uintptr_t> active_button{0};
    std::atomic<std::uintptr_t> attack_owner{0};
    std::atomic<std::uintptr_t> defense_owner{0};
    std::atomic<bool> foreign_thread_seen{false};
};

static_assert(std::atomic<std::uintptr_t>::is_always_lock_free);
static_assert(std::atomic<bool>::is_always_lock_free);
static_assert(std::atomic<Admission>::is_always_lock_free);
static_assert(sizeof(std::uintptr_t) == 8 && sizeof(std::atomic<std::uintptr_t>) == 8);
static_assert(sizeof(std::atomic<bool>) == 1);
static_assert(sizeof(std::atomic<Admission>) == 1);
NativeState native;
thread_local bool callback_active = false;

template<typename T>
bool read_value(std::uintptr_t address, T& value) {
    return platform::windows::read_memory(address, std::as_writable_bytes(std::span(&value, 1)));
}

template<typename T>
bool write_value(std::uintptr_t address, const T& value) {
    return platform::windows::write_memory(address, std::as_bytes(std::span(&value, 1)));
}

ButtonRecord* current_button_record() {
    if (native.layout.button_record >= native.buttons.size()) {
        return nullptr;
    }
    ButtonRecord& record = native.buttons[native.layout.button_record];

    if (record.retired || record.stage == ButtonStage::Empty ||
        record.layout != native.layout.layout || record.epoch != native.epoch ||
        record.generation != native.view.layout_generation) {
        return nullptr;
    }
    return &record;
}

void set_quarantined(ButtonRecord& record, bool quarantined) {
    if (record.quarantined == quarantined) {
        return;
    }
    record.quarantined = quarantined;
    if (quarantined) {
        ++native.view.quarantined_buttons;
    } else {
        --native.view.quarantined_buttons;
    }
}

void release_button_record(ButtonRecord& record) {
    set_quarantined(record, false);
    record = {};
}

ButtonRecord* reserve_button_record(std::uintptr_t allocator, std::uintptr_t original_vtable) {
    for (std::size_t index = 0; index < native.buttons.size(); ++index) {
        ButtonRecord& record = native.buttons[index];

        if (record.stage != ButtonStage::Empty) {
            continue;
        }
        record.allocator = allocator;
        record.layout = native.layout.layout;
        record.original_vtable = original_vtable;
        record.epoch = native.epoch;
        record.generation = native.view.layout_generation;
        record.stage = ButtonStage::Allocating;
        set_quarantined(record, true);
        native.layout.button_record = index;
        ++native.view.allocation_attempts;
        return &record;
    }
    native.view.issue = Stratagem5NativeIssue::AllocationBudgetExhausted;
    return nullptr;
}

void unpublish_button() {
    native.active_layout.store(0, std::memory_order_release);
    native.active_button.store(0, std::memory_order_release);
}

void retire_layout() {
    // A new native initialization/epoch ends the old object's lifetime. Do not
    // call a virtual function or restore bytes through the previous addresses.
    unpublish_button();

    ButtonRecord* const record = current_button_record();

    if (record != nullptr) {
        if (record->stage == ButtonStage::Registered) {
            // Registry ownership was positively confirmed. Recycle metadata,
            // never the game-owned button or its allocator's storage.
            release_button_record(*record);
        } else {
            record->retired = true;
            set_quarantined(*record, true);
        }
    }
    native.layout = {};
    native.dialog = 0;
    native.view.registered = false;
    native.attack_owner.store(0, std::memory_order_release);
    native.defense_owner.store(0, std::memory_order_release);
}

void publish_view(Stratagem5Native& state) {
    native.view.prepared = native.code.patches.count != 0 && native.code.bounds.count != 0;
    native.view.installed = native.code.patches.applied && !native.code.patches.uncertain &&
        native.code.bounds.applied && !native.code.bounds.uncertain;
    native.view.registered = native.layout.registered;
    native.view.patch_result = native.code.patches.last_result;
    if (native.foreign_thread_seen.exchange(false, std::memory_order_acq_rel)) {
        native.view.issue = Stratagem5NativeIssue::ForeignThread;
    }
    state.view = native.view;
}

bool open_admission() {
    Admission expected = Admission::Closed;

    return native.admission.compare_exchange_strong(expected, Admission::Open,
        std::memory_order_release, std::memory_order_relaxed);
}

struct CallbackScope {
    bool entered = false;

    CallbackScope() {
        if (native.admission.load(std::memory_order_acquire) != Admission::Open) {
            return;
        }
        native.active.fetch_add(1, std::memory_order_acq_rel);
        if (native.admission.load(std::memory_order_acquire) != Admission::Open) {
            native.active.fetch_sub(1, std::memory_order_release);
            return;
        }
        if (GetCurrentThreadId() != native.thread.load(std::memory_order_acquire)) {
            unpublish_button();
            native.foreign_thread_seen.store(true, std::memory_order_release);
            native.active.fetch_sub(1, std::memory_order_release);
            return;
        }
        if (callback_active || native.control_active) {
            native.active.fetch_sub(1, std::memory_order_release);
            return;
        }
        callback_active = true;
        entered = true;
    }

    ~CallbackScope() {
        if (entered) {
            callback_active = false;
            native.active.fetch_sub(1, std::memory_order_release);
        }
    }
};

struct ControlScope {
    bool entered = false;
    bool reopen = false;

    explicit ControlScope(Stratagem5Native& state) {
        if (native.owner && native.owner != &state) {
            state.view.issue = Stratagem5NativeIssue::HookFailure;
            state.view.patch_result = platform::windows::CodePatchResult::InvalidInput;
            return;
        }
        const DWORD thread = native.thread.load(std::memory_order_acquire);

        if (thread && thread != GetCurrentThreadId()) {
            state.view.issue = Stratagem5NativeIssue::ForeignThread;
            state.view.patch_result = platform::windows::CodePatchResult::Busy;
            return;
        }
        Admission expected = Admission::Open;

        reopen = native.admission.compare_exchange_strong(expected, Admission::Closed,
            std::memory_order_acq_rel, std::memory_order_acquire);
        if (native.active.load(std::memory_order_acquire) || native.control_active) {
            native.code.patches.last_result = platform::windows::CodePatchResult::Busy;
            publish_view(state);
            if (reopen) {
                open_admission();
            }
            return;
        }
        native.control_active = true;
        entered = true;
    }

    ~ControlScope() {
        if (entered) {
            native.control_active = false;
            if (reopen && !native.quiesced) {
                open_admission();
            }
        }
    }
};

bool valid_session() {
    if (!native.callbacks.valid_session || !native.callbacks.context ||
        !native.callbacks.valid_session(native.callbacks.context)) {
        unpublish_button();
        native.view.issue = Stratagem5NativeIssue::InvalidSession;
        return false;
    }
    return true;
}

bool virtual_address(std::uintptr_t object, std::uintptr_t offset, std::uintptr_t& function) {
    std::uintptr_t vtable = 0;
    MEMORY_BASIC_INFORMATION memory{};
    constexpr DWORD EXECUTABLE = PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;

    return object && read_value(object, vtable) && vtable && read_value(vtable + offset, function) &&
        function && VirtualQuery(reinterpret_cast<const void*>(function), &memory, sizeof(memory)) == sizeof(memory) &&
        memory.State == MEM_COMMIT && !(memory.Protect & (PAGE_GUARD | PAGE_NOACCESS)) && (memory.Protect & EXECUTABLE);
}

bool set_visible(std::uintptr_t button, bool visible) {
    std::uintptr_t function = 0;

    if (!virtual_address(button, 0x108, function)) {
        return false;
    }
    reinterpret_cast<SetVisible>(function)(button, visible);
    return true;
}

bool layout_metrics(std::uintptr_t layout, int& first, int& y, int& step) {
    std::uintptr_t descriptors = 0;
    std::uint32_t count = 0;
    std::array<std::array<int, 2>, 4> positions{};

    if (!read_value(layout + 0x140, descriptors) || !descriptors ||
        !read_value(layout + 0x150, count) || count != 8) {
        return false;
    }
    for (std::size_t index = 0; index < positions.size(); ++index) {
        if (!read_value(descriptors + (index + 2) * 0x60 + 4, positions[index])) {
            return false;
        }
    }
    first = positions[0][0];
    y = positions[0][1];
    step = positions[1][0] - first;
    return step > 0 && positions[2][0] - positions[1][0] == step &&
        positions[3][0] - positions[2][0] == step &&
        positions[1][1] == y && positions[2][1] == y && positions[3][1] == y &&
        first >= -4096 && positions[3][0] <= 8192 && y >= -4096 && y <= 8192;
}

bool valid_helper(std::uintptr_t layout) {
    const LayoutGeneration& generation = native.layout;
    std::uintptr_t descriptors = 0, helpers = 0, owner = 0, helper = 0, vtable = 0;
    std::uint32_t count = 0;
    std::array<std::uint32_t, 5> prefix{};

    return generation.helper_valid && generation.layout == layout &&
        read_value(layout + 0x140, descriptors) && descriptors == generation.descriptors &&
        read_value(layout + 0x148, helpers) && helpers == generation.helpers &&
        read_value(layout + 0x150, count) && count == 8 &&
        read_value(layout + 0x158, owner) && owner == layout &&
        read_value(helpers + 7 * sizeof(std::uintptr_t), helper) && helper == generation.helper &&
        read_value(helper, vtable) && vtable == generation.helper_vtable &&
        read_value(descriptors + 7 * 0x60, prefix) && prefix == generation.descriptor_prefix;
}

enum class RegistrationState { Unavailable, Unregistered, Owned, Foreign };

RegistrationState inspect_registration(std::uintptr_t layout, const ButtonRecord& record) {
    const LayoutGeneration& generation = native.layout;
    std::array<std::uintptr_t, 4> buttons{};
    std::array<std::uint32_t, 5> prefix{};
    std::uintptr_t descriptors = 0, helpers = 0, owner = 0, helper = 0, vtable = 0;
    std::uint32_t count = 0, id = 0, button_state = 0;

    if (!layout || generation.layout != layout || record.layout != layout || record.retired ||
        record.epoch != native.epoch || record.generation != native.view.layout_generation || !record.button ||
        !read_value(layout + 0x140, descriptors) || descriptors != generation.descriptors ||
        !read_value(layout + 0x148, helpers) || helpers != generation.helpers ||
        !read_value(layout + 0x150, count) || count != 8 ||
        !read_value(layout + 0x158, owner) || owner != layout ||
        !read_value(descriptors + 7 * 0x60, prefix) || prefix != generation.descriptor_prefix ||
        !read_value(layout + 0x1e0, buttons) || buttons != record.original_buttons ||
        !read_value(helpers + 7 * sizeof(std::uintptr_t), helper)) {
        return RegistrationState::Unavailable;
    }
    const LookupLayout lookup = reinterpret_cast<LookupLayout>(native.code.module + 0x01d15440);
    const std::uintptr_t maker = layout + 0x140;
    const std::uintptr_t registered = lookup(maker, 7);

    if (registered && registered != record.button) {
        return RegistrationState::Foreign;
    }
    for (std::size_t index = 0; index < buttons.size(); ++index) {
        if (!buttons[index] || lookup(maker, static_cast<int>(index) + 2) != buttons[index]) {
            return RegistrationState::Unavailable;
        }
    }
    if (!read_value(record.button, vtable) || vtable != record.original_vtable ||
        !read_value(record.button + 0x88, id) ||
        !read_value(record.button + 0x8c, button_state) || button_state != READY_BUTTON) {
        return RegistrationState::Unavailable;
    }
    if (registered == record.button && !helper && id == 7) {
        return RegistrationState::Owned;
    }
    if (!registered && helper == generation.helper && id == UNREGISTERED_BUTTON && valid_helper(layout)) {
        return RegistrationState::Unregistered;
    }
    return RegistrationState::Unavailable;
}

void adopt_registered_button(ButtonRecord& record) {
    record.stage = ButtonStage::Registered;
    set_quarantined(record, false);
    native.layout.button = record.button;
    native.layout.original_buttons = record.original_buttons;
    native.layout.initialized = true;
    native.layout.registered = true;
    native.view.registered = true;
}

bool valid_registry(std::uintptr_t layout) {
    const ButtonRecord* const record = current_button_record();

    return native.layout.registered && record != nullptr && record->stage == ButtonStage::Registered &&
        inspect_registration(layout, *record) == RegistrationState::Owned;
}

bool position_buttons(std::uintptr_t layout, std::uint32_t total) {
    std::array<std::uintptr_t, 4> originals{};
    int first = 0, y = 0, step = 0;

    if (total < 2 || total > 5 || !read_value(layout + 0x1e0, originals) ||
        !layout_metrics(layout, first, y, step)) {
        return false;
    }
    const int center = first + step * 3 / 2;

    for (std::uint32_t index = 0; index < total; ++index) {
        const std::uintptr_t button = index == 4 ? native.layout.button : originals[index];
        std::uintptr_t function = 0;

        if (!virtual_address(button, 0x90, function)) {
            return false;
        }
        reinterpret_cast<SetPosition>(function)(button, stratagem5_position(center, step, total, index), y);
    }
    return true;
}

bool create_button(std::uintptr_t layout) {
    LayoutGeneration& generation = native.layout;

    if (generation.layout != layout || !valid_helper(layout)) {
        return false;
    }
    if (generation.button) {
        return generation.initialized;
    }
    if (generation.attempted) {
        return false;
    }
    std::uintptr_t original = 0, original_vtable = 0;
    int first = 0, y = 0, step = 0;

    if (!read_value(layout + 0x1e0, original) || !read_value(original, original_vtable) ||
        !original_vtable || !layout_metrics(layout, first, y, step)) {
        return false;
    }
    generation.attempted = true;
    const std::uintptr_t module = native.code.module;
    const std::uintptr_t manager = reinterpret_cast<GetMemoryManager>(module + 0x00014ed0)();
    std::uintptr_t allocator = 0, allocate = 0;

    if (!manager || !read_value(manager + 0x118, allocator) || !virtual_address(allocator, 0x28, allocate)) {
        native.view.issue = Stratagem5NativeIssue::AllocationFailed;
        return false;
    }
    struct AllocationTag {
        std::uint32_t tag;
        std::uint32_t reserved;
        std::uintptr_t context;
    } tag{0x35, 0, 0};
    ButtonRecord* const record = reserve_button_record(allocator, original_vtable);

    if (record == nullptr) {
        return false;
    }
    // The record precedes every foreign call and owns only its bookkeeping.
    // If a call raises SEH, the completed stage and returned addresses survive
    // in static storage; no guessed destructor/deallocator is ever invoked.
    record->raw = reinterpret_cast<Allocate>(allocate)(allocator, BUTTON_SIZE, &tag);
    record->stage = ButtonStage::Allocated;
    if (!record->raw) {
        release_button_record(*record);
        generation.button_record = NO_BUTTON_RECORD;
        native.view.issue = Stratagem5NativeIssue::AllocationFailed;
        return false;
    }
    if (!platform::windows::is_committed_memory(record->raw, BUTTON_SIZE)) {
        native.view.issue = Stratagem5NativeIssue::AllocationFailed;
        return false;
    }
    record->stage = ButtonStage::Constructing;
    record->button = reinterpret_cast<ConstructButton>(module + 0x01e7fec0)(record->raw);
    record->stage = ButtonStage::Constructed;
    if (!record->button || !platform::windows::is_committed_memory(record->button, BUTTON_SIZE)) {
        native.view.issue = Stratagem5NativeIssue::AllocationFailed;
        return false;
    }
    const std::uintptr_t button = record->button;
    const int x = stratagem5_position(first + step * 3 / 2, step, 5, 4);

    generation.button = button;
    record->stage = ButtonStage::Initializing;
    reinterpret_cast<InitializeButton>(module + 0x01e7ff40)(button, x, y, layout);
    record->stage = ButtonStage::Initialized;

    constexpr std::array<std::uint8_t, 2> INITIAL_FLAGS{0, 1};
    std::int32_t state = 0;

    if (!write_value(button + 0x1d4, INITIAL_FLAGS) || !read_value(button + 0x1c8, state)) {
        return false;
    }
    if (state != 1) {
        std::uint32_t flags = 0;
        std::uintptr_t function1 = 0, function2 = 0, copy = 0;
        std::uint64_t pair = 0;

        if (!read_value(button + 0x40, flags) || !write_value(button + 0x40, flags | 0x200u) ||
            !virtual_address(button, 0x268, function1) || !virtual_address(button, 0x270, function2) ||
            !read_value(module + 0x02c3f808, pair) || !read_value(button + 0x1a8, copy)) {
            return false;
        }
        reinterpret_cast<SetU32>(function1)(button, 0);
        reinterpret_cast<SetU64>(function2)(button, pair);
        if (!write_value(button + 0x48, copy)) {
            return false;
        }
    }
    std::uintptr_t vtable = 0;

    if (!write_value(button + 0x88, UNREGISTERED_BUTTON) || !write_value(button + 0x8c, READY_BUTTON)) {
        return false;
    }
    reinterpret_cast<SetTrickId>(module + 0x01e7fd90)(button, 5);
    generation.initialized = read_value(button, vtable) && vtable == original_vtable && set_visible(button, false);
    if (generation.initialized) {
        record->stage = ButtonStage::Prepared;
    }
    return generation.initialized;
}

bool register_button(std::uintptr_t layout) {
    LayoutGeneration& generation = native.layout;
    ButtonRecord* const record = current_button_record();

    if (record == nullptr || !generation.button || !generation.initialized) {
        return false;
    }
    if (record->stage == ButtonStage::Prepared) {
        if (!valid_helper(layout)) {
            return false;
        }
        const LookupLayout lookup = reinterpret_cast<LookupLayout>(native.code.module + 0x01d15440);
        const RegisterLayout register_layout = reinterpret_cast<RegisterLayout>(native.code.module + 0x01d16aa0);
        const std::uintptr_t maker = layout + 0x140;
        std::array<std::uintptr_t, 4> originals{};

        if (!read_value(layout + 0x1e0, originals) || lookup(maker, 7)) {
            return false;
        }
        for (std::size_t index = 0; index < originals.size(); ++index) {
            if (!originals[index] || lookup(maker, static_cast<int>(index) + 2) != originals[index]) {
                return false;
            }
        }
        record->original_buttons = originals;
        generation.original_buttons = originals;
        record->stage = ButtonStage::Registering;
        // Registration may consume helper7 before returning or raising SEH.
        // Never repeat this call. Later events only inspect/adopt its result.
        register_layout(maker, 7, generation.button, 0x14, 1);
    }
    if ((record->stage != ButtonStage::Registering && record->stage != ButtonStage::Registered) ||
        inspect_registration(layout, *record) != RegistrationState::Owned) {
        native.view.issue = Stratagem5NativeIssue::RegistryFailed;
        return false;
    }
    adopt_registered_button(*record);
    if (!set_visible(record->button, false)) {
        native.view.issue = Stratagem5NativeIssue::MemoryFailed;
        return false;
    }
    generation.registration_hidden = true;
    // Callback registration is a one-shot native event, possibly before the
    // model is readable. Publish the hidden fifth button for that binding now.
    native.active_button.store(generation.button, std::memory_order_release);
    native.active_layout.store(layout, std::memory_order_release);
    return true;
}

bool prepare_layout(std::uintptr_t layout) {
    if (!native.view.armed || !native.metadata || !valid_session()) {
        return false;
    }
    if (valid_registry(layout) && native.layout.registration_hidden) {
        return true;
    }
    const ButtonRecord* const record = current_button_record();
    const bool registration_started = record != nullptr &&
        (record->stage == ButtonStage::Registering || record->stage == ButtonStage::Registered);
    const bool prepared = registration_started || (valid_helper(layout) && create_button(layout));

    if (!prepared || !register_button(layout)) {
        if (native.view.issue == Stratagem5NativeIssue::None) {
            native.view.issue = Stratagem5NativeIssue::InvalidLayout;
        }
        return false;
    }
    native.view.issue = Stratagem5NativeIssue::None;
    return true;
}

bool prepare_dialog(std::uintptr_t dialog) {
    if (!native.view.armed || !native.metadata || !valid_session()) {
        return false;
    }
    unpublish_button();

    std::uintptr_t layout = 0;

    if (!dialog || !read_value(dialog + 8, layout) || !layout ||
        layout != native.layout.layout || !native.callbacks.prepare_model) {
        native.view.issue = Stratagem5NativeIssue::InvalidLayout;
        return false;
    }
    if (!valid_registry(layout) && !prepare_layout(layout)) {
        return false;
    }
    native.dialog = dialog;
    const Stratagem5NativeContext context{
        dialog,
        native.attack_owner.load(std::memory_order_acquire),
        native.defense_owner.load(std::memory_order_acquire)
    };
    std::uint32_t original_count = 0;

    if (!native.callbacks.prepare_model(native.callbacks.context, context, original_count) ||
        original_count < 1 || original_count > 4) {
        // Registration can publish a hidden button before the model is ready.
        // A failed owner transition must close the fifth-slot path again.
        unpublish_button();
        native.view.issue = set_visible(native.layout.button, false)
            ? Stratagem5NativeIssue::WaitingModel : Stratagem5NativeIssue::MemoryFailed;
        return false;
    }
    if (original_count < 4) {
        unpublish_button();
        if (native.layout.registered && !set_visible(native.layout.button, false)) {
            return false;
        }
        native.view.issue = Stratagem5NativeIssue::None;
        return true;
    }
    if (!prepare_layout(layout) || !valid_registry(layout)) {
        return false;
    }
    native.active_button.store(native.layout.button, std::memory_order_release);
    native.active_layout.store(layout, std::memory_order_release);
    if (!set_visible(native.layout.button, true) || !position_buttons(layout, 5)) {
        native.view.issue = Stratagem5NativeIssue::MemoryFailed;
        return false;
    }
    native.view.issue = Stratagem5NativeIssue::None;
    return true;
}

bool invoke_layout(std::uintptr_t layout) noexcept {
    __try {
        return prepare_layout(layout);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        unpublish_button();
        native.view.issue = Stratagem5NativeIssue::MemoryFailed;
        return false;
    }
}

bool invoke_dialog(std::uintptr_t dialog) noexcept {
    __try {
        return prepare_dialog(dialog);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        unpublish_button();
        native.view.issue = Stratagem5NativeIssue::MemoryFailed;
        return false;
    }
}

bool hide_owned_button() noexcept {
    __try {
        ButtonRecord* const record = current_button_record();

        if (record == nullptr) {
            return !native.layout.registered;
        }
        if (record->stage == ButtonStage::Registering || record->stage == ButtonStage::Registered) {
            const RegistrationState registration = inspect_registration(native.layout.layout, *record);

            if (registration == RegistrationState::Owned) {
                adopt_registered_button(*record);
            } else if (registration != RegistrationState::Unregistered) {
                // Neither an unreadable registry nor a foreign ID 7 proves
                // our button dead. Retain its identity and retry without
                // touching the foreign registration or repeating RegisterLayout.
                native.view.issue = Stratagem5NativeIssue::RegistryFailed;
                return false;
            }
            return set_visible(record->button, false);
        }
        if (record->stage == ButtonStage::Prepared) {
            // Prepared buttons were hidden before any registration attempt.
            return true;
        }
        if (record->stage == ButtonStage::Initialized) {
            std::uintptr_t vtable = 0;

            if (!valid_helper(native.layout.layout) || !read_value(record->button, vtable) ||
                vtable != record->original_vtable) {
                native.view.issue = Stratagem5NativeIssue::MemoryFailed;
                return false;
            }
            return set_visible(record->button, false);
        }
        // Construction/Initialize did not complete. No method or destructor
        // can safely be inferred for this object; quarantine its ledger entry.
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        native.view.issue = Stratagem5NativeIssue::MemoryFailed;
        return false;
    }
}

void __fastcall post_buttons(std::uintptr_t layout) noexcept {
    const CallbackScope scope;

    if (scope.entered) {
        invoke_layout(layout);
    }
}

void __fastcall before_callbacks(std::uintptr_t dialog) noexcept {
    const CallbackScope scope;

    if (scope.entered) {
        invoke_dialog(dialog);
    }
}

void __fastcall refresh_layout(std::uintptr_t layout) noexcept {
    const CallbackScope scope;
    std::uintptr_t dialog_layout = 0;

    if (scope.entered && native.dialog && read_value(native.dialog + 8, dialog_layout) && dialog_layout == layout) {
        invoke_dialog(native.dialog);
    }
}

void validate_expanded_layout(std::uintptr_t maker, std::uintptr_t owner) {
    LayoutGeneration& generation = native.layout;
    std::uintptr_t result_owner = 0;
    std::uint32_t count = 0;

    if (!read_value(maker, generation.descriptors) || !read_value(maker + 8, generation.helpers) ||
        !read_value(maker + 0x10, count) || count != 8 ||
        !read_value(maker + 0x18, result_owner) || result_owner != owner ||
        !generation.descriptors || !generation.helpers ||
        !read_value(generation.helpers + 7 * sizeof(std::uintptr_t), generation.helper) || !generation.helper ||
        !read_value(generation.helper, generation.helper_vtable) || !generation.helper_vtable ||
        !read_value(generation.descriptors + 7 * 0x60, generation.descriptor_prefix)) {
        native.view.issue = Stratagem5NativeIssue::InvalidLayout;
        return;
    }
    for (std::size_t index = 2; index <= 5; ++index) {
        std::uintptr_t helper = 0, vtable = 0;

        if (!read_value(generation.helpers + index * sizeof(std::uintptr_t), helper) || !helper ||
            helper == generation.helper || !read_value(helper, vtable) || vtable != generation.helper_vtable) {
            native.view.issue = Stratagem5NativeIssue::InvalidLayout;
            return;
        }
    }
    std::array<std::uint32_t, 5> fourth_prefix{};

    generation.helper_valid = read_value(generation.descriptors + 5 * 0x60, fourth_prefix) &&
        fourth_prefix == generation.descriptor_prefix;
    native.view.issue = generation.helper_valid ? Stratagem5NativeIssue::None : Stratagem5NativeIssue::InvalidLayout;
}

void __fastcall init_layouts(std::uintptr_t maker, const void* descriptors, int count, std::uintptr_t owner) {
    const InitLayouts original = reinterpret_cast<InitLayouts>(native.original_init.load(std::memory_order_acquire));

    if (!original) {
        return;
    }
    const CallbackScope scope;

    if (!scope.entered) {
        original(maker, descriptors, count, owner);
        return;
    }
    retire_layout();
    native.layout.layout = owner;
    ++native.view.layout_generation;
    if (!native.view.armed || !native.metadata || !valid_session() || !owner || maker != owner + 0x140 || count != 7 ||
        !platform::windows::is_committed_memory(owner, 0x2a8)) {
        original(maker, descriptors, count, owner);
        return;
    }
    if (native.view.quarantined_buttons >= BUTTON_LEDGER_CAPACITY) {
        native.view.issue = Stratagem5NativeIssue::AllocationBudgetExhausted;
        original(maker, descriptors, count, owner);
        return;
    }
    alignas(16) std::array<std::byte, 8 * 0x60> expanded{};
    constexpr std::array<std::array<std::uint32_t, 5>, 7> EXPECTED{{
        {12, 260, 314, 1400, 420}, {13, 260, 75, 1400, 659},
        {0, 406, 364, 268, 148}, {0, 686, 364, 268, 148},
        {0, 966, 364, 268, 148}, {0, 1246, 364, 268, 148}, {0, 480, 536, 960, 152}
    }};

    if (!platform::windows::read_memory(reinterpret_cast<std::uintptr_t>(descriptors), std::span(expanded).first(7 * 0x60))) {
        original(maker, descriptors, count, owner);
        return;
    }
    for (std::size_t index = 0; index < EXPECTED.size(); ++index) {
        if (std::memcmp(expanded.data() + index * 0x60, EXPECTED[index].data(), sizeof(EXPECTED[index])) != 0) {
            native.view.issue = Stratagem5NativeIssue::InvalidLayout;
            original(maker, descriptors, count, owner);
            return;
        }
    }
    std::memcpy(expanded.data() + 7 * 0x60, expanded.data() + 5 * 0x60, 0x60);
    // Preserve original exception propagation: a partially initialized native
    // maker must never be called a second time with its original descriptors.
    original(maker, expanded.data(), 8, owner);
    validate_expanded_layout(maker, owner);
}

bool prepare_unpublished(Stratagem5Native& state, std::uintptr_t module_base,
    std::span<const std::byte> image) {
    if (native.code.patches.count) {
        return native.code.module == module_base;
    }
    if (!stratagem5_native_detail::supported_image(image)) {
        native.view.issue = Stratagem5NativeIssue::UnsupportedBuild;
        native.code.patches.last_result = platform::windows::CodePatchResult::ExpectedBytesMismatch;
        publish_view(state);
        return false;
    }
    const stratagem5_native_detail::EntryPoints entries{
        reinterpret_cast<std::uintptr_t>(&init_layouts),
        reinterpret_cast<std::uintptr_t>(&post_buttons),
        reinterpret_cast<std::uintptr_t>(&before_callbacks),
        reinterpret_cast<std::uintptr_t>(&refresh_layout),
        reinterpret_cast<std::uintptr_t>(&refresh_layout),
        reinterpret_cast<std::uintptr_t>(&native.admission),
        reinterpret_cast<std::uintptr_t>(&native.active_layout),
        reinterpret_cast<std::uintptr_t>(&native.active_button),
        reinterpret_cast<std::uintptr_t>(&native.attack_owner),
        reinterpret_cast<std::uintptr_t>(&native.defense_owner)
    };

    if (!stratagem5_native_detail::prepare_code(native.code, module_base, image, entries)) {
        native.view.issue = Stratagem5NativeIssue::HookFailure;
        publish_view(state);
        return false;
    }
    native.view.issue = Stratagem5NativeIssue::None;
    native.code.patches.last_result = platform::windows::CodePatchResult::AlreadyApplied;
    publish_view(state);
    return true;
}

} // namespace

bool prepare_stratagem5_native_code(Stratagem5Native& state, std::uintptr_t module_base,
    std::span<const std::byte> image) {
    const ControlScope scope(state);

    if (!scope.entered || native.code.published || native.thread.load(std::memory_order_acquire)) {
        return false;
    }
    native.owner = &state;
    return prepare_unpublished(state, module_base, image);
}

bool prepare_stratagem5_native(Stratagem5Native& state, std::uintptr_t module_base,
    std::span<const std::byte> image, Stratagem5NativeCallbacks callbacks) {
    const ControlScope scope(state);

    if (!scope.entered) {
        return false;
    }
    if (native.admission.load(std::memory_order_acquire) == Admission::Abandoned ||
        !callbacks.context || !callbacks.valid_session || !callbacks.prepare_model ||
        (native.code.module && native.code.module != module_base)) {
        state.view.patch_result = platform::windows::CodePatchResult::InvalidInput;
        return false;
    }
    native.owner = &state;
    native.thread.store(GetCurrentThreadId(), std::memory_order_release);
    native.callbacks = callbacks;
    native.quiesced = false;
    if (!prepare_unpublished(state, module_base, image)) {
        return false;
    }
    native.original_init.store(native.code.original_init_layouts, std::memory_order_release);
    native.code.published = true;
    if (!native.code.patches.applied || native.code.patches.uncertain) {
        if (!set_native_patch_group(native.code.patches, true)) {
            native.view.issue = Stratagem5NativeIssue::HookFailure;
            publish_view(state);
            return false;
        }
        // At most one code transaction per dispatch. A native dialog can still
        // run its original four-button path while preparation is pending.
        native.code.patches.last_result = platform::windows::CodePatchResult::Busy;
        publish_view(state);
        return false;
    }
    if (!native.code.bounds.applied || native.code.bounds.uncertain) {
        const bool installed = set_native_patch_group(native.code.bounds, true);

        native.code.patches.last_result = native.code.bounds.last_result;
        if (!installed) {
            native.view.issue = Stratagem5NativeIssue::HookFailure;
            publish_view(state);
            return false;
        }
    }
    if (!open_admission()) {
        native.view.issue = Stratagem5NativeIssue::HookFailure;
        native.code.patches.last_result = platform::windows::CodePatchResult::InvalidInput;
        publish_view(state);
        return false;
    }
    native.view.issue = Stratagem5NativeIssue::None;
    publish_view(state);
    return true;
}

bool set_stratagem5_native_metadata(Stratagem5Native& state, bool enabled) {
    const ControlScope scope(state);

    if (!scope.entered) {
        return false;
    }
    if (enabled && (!native.code.patches.applied || native.code.patches.uncertain ||
        !native.code.bounds.applied || native.code.bounds.uncertain || native.quiesced)) {
        return false;
    }
    native.metadata = enabled;
    if (!enabled) {
        native.view.armed = false;
        unpublish_button();
        if (!hide_owned_button()) {
            native.view.issue = Stratagem5NativeIssue::MemoryFailed;
            publish_view(state);
            return false;
        }
    }
    publish_view(state);
    return true;
}

bool ensure_stratagem5_native_count_capture(Stratagem5Native& state) {
    publish_view(state);
    return native.owner == &state && state.view.installed;
}

bool update_stratagem5_native(Stratagem5Native& state, bool armed,
    std::uint64_t epoch, bool retire_battle) {
    if (!native.owner) {
        return true;
    }
    const ControlScope scope(state);

    if (!scope.entered) {
        return false;
    }
    if (native.epoch != epoch || retire_battle) {
        retire_layout();
        native.epoch = epoch;
    }
    native.view.armed = armed && native.metadata && native.code.patches.applied && !native.code.patches.uncertain &&
        native.code.bounds.applied && !native.code.bounds.uncertain && !native.quiesced;
    if (!native.view.armed) {
        unpublish_button();
    }
    publish_view(state);
    return true;
}

bool quiesce_stratagem5_native(Stratagem5Native& state) {
    if (!native.owner) {
        return true;
    }
    const ControlScope scope(state);

    if (!scope.entered) {
        return false;
    }
    native.quiesced = true;
    native.view.armed = false;
    native.metadata = false;
    unpublish_button();
    if (!hide_owned_button()) {
        native.view.issue = Stratagem5NativeIssue::MemoryFailed;
        publish_view(state);
        return false;
    }
    native.callbacks = {};
    publish_view(state);
    return true;
}

void close_stratagem5_native_admission() noexcept {
    native.admission.store(Admission::Abandoned, std::memory_order_release);
    unpublish_button();
}

bool stop_stratagem5_native(Stratagem5Native& state) {
    if (!quiesce_stratagem5_native(state)) {
        return false;
    }
    if (!native.owner) {
        return true;
    }
    const ControlScope scope(state);

    if (!scope.entered) {
        return false;
    }
    if (native.code.bounds.applied || native.code.bounds.uncertain) {
        const bool restored = set_native_patch_group(native.code.bounds, false);

        native.code.patches.last_result = native.code.bounds.last_result;
        if (!restored) {
            native.view.issue = Stratagem5NativeIssue::HookFailure;
            publish_view(state);
            return false;
        }
        if (native.code.patches.applied || native.code.patches.uncertain) {
            native.code.patches.last_result = platform::windows::CodePatchResult::Busy;
            publish_view(state);
            return false;
        }
    }
    if ((native.code.patches.applied || native.code.patches.uncertain) &&
        !set_native_patch_group(native.code.patches, false)) {
        native.view.issue = Stratagem5NativeIssue::HookFailure;
        publish_view(state);
        return false;
    }
    retire_layout();
    stratagem5_native_detail::discard_unpublished_code(native.code);
    native.view.issue = Stratagem5NativeIssue::None;
    publish_view(state);
    return true;
}

} // namespace s8::game
