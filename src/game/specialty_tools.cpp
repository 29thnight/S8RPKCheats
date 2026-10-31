#include "specialty_tools.h"

#include "officer_roster.h"
#include "../platform/windows/process_memory.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <limits>
#include <span>

namespace s8::game {
namespace {

using Status = domain::SpecialtyStatus;
constexpr std::uintptr_t CITY_STRIDE = 0x2a0;
constexpr std::uintptr_t OFFICER_STRIDE = 0x3d0;
constexpr std::uintptr_t POINTER_MASK = 0x0000ffffffffffffULL;
constexpr std::size_t READS_PER_UPDATE = 16;
constexpr std::chrono::microseconds UPDATE_BUDGET{600};

template<class T> bool read_value(std::uintptr_t address, T& value) {
    return platform::windows::read_memory(address, std::as_writable_bytes(std::span(&value, 1)));
}

template<class T, std::size_t N> T field(const std::array<std::byte, N>& bytes, std::size_t offset) {
    T result{};

    std::memcpy(&result, bytes.data() + offset, sizeof(result));
    return result;
}

std::uintptr_t city_base(std::uintptr_t module) {
    std::uintptr_t result = module + 0x034c8630;

    for (unsigned index = 0; index < 3; ++index) {
        if (!read_value(result, result) || result <= 0x10000) {
            return 0;
        }
    }
    return result;
}

std::uintptr_t slot_address(const SpecialtyTools& state, std::size_t index) {
    return state.cities + (index / 3) * CITY_STRIDE + 0x248 + (index % 3) * 0x18;
}

bool same_context(const SpecialtyTools& state, const SpecialtyContext& context) {
    if (context.epoch != state.context.epoch || context.game_base != state.context.game_base ||
        context.player != state.context.player || context.module != state.context.module ||
        context.game_base <= 0x10000 || context.player <= 0x10000 || !context.game_base_pointer) {
        return false;
    }
    std::uintptr_t root = 0;
    std::uintptr_t player = 0;

    return read_value(context.game_base_pointer, root) && root == context.game_base &&
        read_value(root + 0xe0, player) && player == context.player && city_base(context.module) == state.cities;
}

void finish(SpecialtyTools& state, Status status) {
    state.phase = SpecialtyPhase::Idle;
    state.snapshot.status = status;
    state.snapshot.busy = false;
    ++state.snapshot.revision;
}

bool read_slot(SpecialtyTools& state, std::size_t index) {
    std::array<std::byte, 20> bytes{};
    domain::SpecialtySlot row;

    if (!platform::windows::read_memory(slot_address(state, index) - 8, bytes)) {
        state.snapshot.slots[index] = row;
        return false;
    }
    row.enabled = field<std::uint32_t>(bytes, 0);
    row.object = field<std::uintptr_t>(bytes, 8) & POINTER_MASK;
    row.bought = field<std::uint32_t>(bytes, 16);
    row.valid = true;
    if (row.object > 0x10000 && !read_value(row.object + 8, row.id)) {
        row.id = 0;
    }
    state.snapshot.slots[index] = row;
    return true;
}

bool valid_registry(std::uintptr_t base, std::uintptr_t stride) {
    std::uint16_t first = 0;
    std::uint16_t second = 0;

    return base > 0x10000 && read_value(base + 8, first) && first == 1 &&
        read_value(base + stride + 8, second) && second == 2;
}

bool resolve_items(SpecialtyTools& state) {
    const domain::SpecialtySlot* sample = nullptr;

    for (const domain::SpecialtySlot& slot : state.snapshot.slots) {
        if (!slot.valid || slot.id < 1 || slot.id >= domain::SPECIALTY_COUNT || slot.object <= 0x10000) {
            continue;
        }
        if (!sample) {
            sample = &slot;
        } else if (sample->id != slot.id) {
            const std::int64_t difference = static_cast<std::int64_t>(slot.object) -
                static_cast<std::int64_t>(sample->object);
            const int ids = static_cast<int>(slot.id) - static_cast<int>(sample->id);
            const std::int64_t stride = difference / ids;

            if (difference % ids == 0 && stride >= 0x40 && stride <= 0x100 && stride % 8 == 0) {
                state.stride = static_cast<std::uintptr_t>(stride);
            }
            break;
        }
    }
    if (sample) {
        const std::uintptr_t offset = (sample->id - 1) * state.stride;

        if (sample->object > offset && valid_registry(sample->object - offset, state.stride)) {
            state.items = sample->object - offset;
            return true;
        }
    }
    constexpr std::array<std::uintptr_t, 3> ANCHORS{0x034c8630, 0x037b0000, 0x037ff430};
    constexpr std::array<std::uintptr_t, 4> CHAIN{0, 0, 0x20, 0};

    for (const std::uintptr_t anchor : ANCHORS) {
        std::uintptr_t address = state.context.module + anchor;

        for (const std::uintptr_t offset : CHAIN) {
            if (!read_value(address + offset, address) || address <= 0x10000) {
                address = 0;
                break;
            }
        }
        if (valid_registry(address, state.stride)) {
            state.items = address;
            return true;
        }
        if (address && valid_registry(address + 0x19af60, state.stride)) {
            state.items = address + 0x19af60;
            return true;
        }
    }
    return false;
}

bool read_item(SpecialtyTools& state, std::uint16_t id) {
    std::array<std::byte, 0x40> bytes{};
    domain::SpecialtyRow row;
    const std::uintptr_t object = state.items + (id - 1) * state.stride;

    if (!platform::windows::read_memory(object, bytes) || field<std::uint16_t>(bytes, 8) != id) {
        state.snapshot.items[id] = row;
        return false;
    }
    row.object = object;
    row.vtable = field<std::uintptr_t>(bytes, 0);
    row.owner = field<std::uintptr_t>(bytes, 0x30);
    row.owner_type = field<std::uint32_t>(bytes, 0x38);
    row.type = field<std::uint8_t>(bytes, 0x0e);
    row.trait = field<std::uint8_t>(bytes, 0x10);
    row.trait_level = field<std::uint8_t>(bytes, 0x20);
    row.ability = field<std::uint8_t>(bytes, 0x22);
    row.ability_value = field<std::uint8_t>(bytes, 0x23);
    row.effect = field<std::uint8_t>(bytes, 0x24);
    row.effect_value = field<std::uint8_t>(bytes, 0x25);
    row.value = field<std::uint8_t>(bytes, 0x26);
    row.valid = row.vtable > 0x10000 && row.type >= 1 && row.type <= 26;
    const std::uintptr_t owner = row.owner & POINTER_MASK;

    if (owner >= state.cities && owner < state.cities + 51 * CITY_STRIDE &&
        (owner - state.cities) % CITY_STRIDE == 0) {
        row.city = static_cast<std::uint8_t>((owner - state.cities) / CITY_STRIDE);
    } else if (owner > 0x10000) {
        if (!read_value(owner + 8, row.owner_id) || row.owner_id > 5102) {
            row.owner_id = 0;
        }
    }
    state.snapshot.items[id] = row;
    return row.valid;
}

bool unchanged_item(const SpecialtyTools& state, std::uint16_t id) {
    const domain::SpecialtyRow& row = state.snapshot.items[id];
    std::array<std::byte, 0x40> bytes{};

    return row.valid && platform::windows::read_memory(row.object, bytes) &&
        field<std::uintptr_t>(bytes, 0) == row.vtable && field<std::uint16_t>(bytes, 8) == id &&
        field<std::uintptr_t>(bytes, 0x30) == row.owner && field<std::uint32_t>(bytes, 0x38) == row.owner_type;
}

bool target_valid(const SpecialtyTools& state) {
    std::uintptr_t vtable = 0;
    std::uint16_t id = 0;

    return state.target > 0x10000 && read_value(state.target, vtable) && vtable == state.target_vtable &&
        read_value(state.target + 8, id) && id == state.target_id &&
        (state.target_is_player ? state.target == state.context.player :
            resolve_officer_roster_base(state.context.module) == state.target_roster);
}

bool select_target(SpecialtyTools& state, std::uintptr_t address) {
    std::uint16_t id = 0;
    std::uintptr_t vtable = 0;

    if (!read_value(address + 8, id) || id != state.target_id ||
        !read_value(address, vtable) || vtable <= 0x10000) {
        return false;
    }
    state.target = address;
    state.target_vtable = vtable;
    state.cursor = 1;
    state.phase = SpecialtyPhase::Give;
    return true;
}

struct DataWrite {
    std::uintptr_t address = 0;
    std::array<std::byte, 8> before{};
    std::array<std::byte, 8> after{};
    std::size_t size = 0;
};

template<class T> void record_write(DataWrite& write, std::uintptr_t address, T before, T after) {
    write.address = address;
    write.size = sizeof(T);
    std::memcpy(write.before.data(), &before, sizeof(T));
    std::memcpy(write.after.data(), &after, sizeof(T));
}

Status commit_writes(std::span<const DataWrite> writes) {
    std::array<std::byte, 8> current{};
    std::size_t attempted = 0;

    for (const DataWrite& write : writes) {
        const std::span<std::byte> bytes(current.data(), write.size);

        if (!platform::windows::read_memory(write.address, bytes) ||
            std::memcmp(current.data(), write.before.data(), write.size) != 0) {
            break;
        }
        ++attempted;
        if (!platform::windows::write_data_memory(write.address, {write.after.data(), write.size}) ||
            !platform::windows::read_memory(write.address, bytes) ||
            std::memcmp(current.data(), write.after.data(), write.size) != 0) {
            break;
        }
    }
    // The last write can fail after being attempted; verify the entire small
    // transaction before treating its count as success.
    bool committed = attempted == writes.size();

    for (const DataWrite& write : writes) {
        if (!platform::windows::read_memory(write.address, {current.data(), write.size}) ||
            std::memcmp(current.data(), write.after.data(), write.size) != 0) {
            committed = false;
            break;
        }
    }
    if (committed) {
        return Status::Complete;
    }
    bool restored = true;

    while (attempted != 0) {
        const DataWrite& write = writes[--attempted];

        if (!platform::windows::read_memory(write.address, {current.data(), write.size})) {
            restored = false;
            continue;
        }
        if (std::memcmp(current.data(), write.before.data(), write.size) == 0) {
            continue;
        }
        if (std::memcmp(current.data(), write.after.data(), write.size) != 0 ||
            !platform::windows::write_data_memory(write.address, {write.before.data(), write.size}) ||
            !platform::windows::read_memory(write.address, {current.data(), write.size}) ||
            std::memcmp(current.data(), write.before.data(), write.size) != 0) {
            restored = false;
        }
    }
    return restored ? Status::WriteFailed : Status::RollbackFailed;
}

Status give_item(SpecialtyTools& state, std::uint16_t id) {
    if (!target_valid(state) || !unchanged_item(state, id)) {
        return Status::Changed;
    }
    const domain::SpecialtyRow& row = state.snapshot.items[id];
    const std::uintptr_t item_object = row.object;
    std::array<DataWrite, domain::SPECIALTY_SLOT_COUNT + 2> writes{};
    std::size_t count = 2;

    record_write(writes[0], row.object + 0x30, row.owner, state.target);
    record_write(writes[1], row.object + 0x38, row.owner_type, std::uint32_t{1});
    for (std::size_t index = 0; index < state.snapshot.slots.size(); ++index) {
        const domain::SpecialtySlot& slot = state.snapshot.slots[index];

        if (slot.object == row.object) {
            std::uintptr_t object = 0;

            if (!read_value(slot_address(state, index), object) || (object & POINTER_MASK) != row.object) {
                return Status::Changed;
            }
            record_write(writes[count++], slot_address(state, index) + 8, slot.bought, std::uint32_t{1});
        }
    }
    const Status result = commit_writes({writes.data(), count});

    if (result == Status::Complete) {
        read_item(state, id);
        for (domain::SpecialtySlot& slot : state.snapshot.slots) {
            if (slot.object == item_object) {
                slot.bought = 1;
            }
        }
    }
    return result;
}

void plan_distribution(SpecialtyTools& state, std::mt19937& random) {
    state.pool_size = 0;
    state.slot_count = 0;
    for (std::size_t index = 0; index < state.snapshot.slots.size(); ++index) {
        const domain::SpecialtySlot& slot = state.snapshot.slots[index];

        if (slot.valid && (!slot.enabled || slot.bought || slot.object <= 0x10000)) {
            state.empty_slots[state.slot_count++] = static_cast<std::uint16_t>(index);
        }
    }
    for (std::uint16_t id = 1; id < domain::SPECIALTY_COUNT; ++id) {
        const domain::SpecialtyRow& item = state.snapshot.items[id];
        bool assigned = false;

        if (!state.named[id] || !item.valid || (item.owner & POINTER_MASK) > 0x10000) {
            continue;
        }
        for (const domain::SpecialtySlot& slot : state.snapshot.slots) {
            if (slot.object == item.object) {
                assigned = true;
                break;
            }
        }
        if (!assigned) {
            state.pool[state.pool_size++] = id;
        }
    }
    std::shuffle(state.pool.begin(), state.pool.begin() + state.pool_size, random);
    std::shuffle(state.empty_slots.begin(), state.empty_slots.begin() + state.slot_count, random);
    state.snapshot.total = std::min(state.pool_size, state.slot_count);
    state.snapshot.completed = 0;
    state.snapshot.status = Status::Distributing;
    state.cursor = 0;
    state.phase = SpecialtyPhase::Distribute;
}

Status distribute_item(SpecialtyTools& state, std::size_t index) {
    const std::uint16_t id = state.pool[index];
    const std::size_t selected_slot = state.empty_slots[index];
    const domain::SpecialtyRow& item = state.snapshot.items[id];
    const domain::SpecialtySlot expected = state.snapshot.slots[selected_slot];

    if (!unchanged_item(state, id) || (item.owner & POINTER_MASK) > 0x10000) {
        return Status::Changed;
    }
    // Cities can receive an item between planning and committing. A compact
    // read per city prevents duplicate placement without rebuilding containers.
    for (std::size_t city = 0; city < 51; ++city) {
        std::array<std::byte, 72> slots{};

        if (!platform::windows::read_memory(state.cities + city * CITY_STRIDE + 0x240, slots)) {
            return Status::ReadFailed;
        }
        for (std::size_t slot = 0; slot < 3; ++slot) {
            if ((field<std::uintptr_t>(slots, slot * 24 + 8) & POINTER_MASK) == item.object) {
                return Status::Changed;
            }
        }
    }
    if (!read_slot(state, selected_slot)) {
        return Status::ReadFailed;
    }
    const domain::SpecialtySlot& current = state.snapshot.slots[selected_slot];

    if (current.object != expected.object || current.enabled != expected.enabled || current.bought != expected.bought) {
        return Status::Changed;
    }
    std::array<DataWrite, 3> writes{};
    std::uintptr_t raw_object = 0;
    const std::uintptr_t address = slot_address(state, selected_slot);

    if (!read_value(address, raw_object) || (raw_object & POINTER_MASK) != current.object) {
        return Status::Changed;
    }
    record_write(writes[0], address - 8, current.enabled, std::uint32_t{1});
    record_write(writes[1], address, raw_object, item.object);
    record_write(writes[2], address + 8, current.bought, std::uint32_t{0});
    const Status status = commit_writes(writes);

    read_slot(state, selected_slot);
    return status;
}

void record_result(SpecialtyTools& state, Status status) {
    ++state.snapshot.completed;
    if (status == Status::Complete) {
        ++state.snapshot.succeeded;
    } else {
        ++state.snapshot.failed;
        state.snapshot.last_error = status;
    }
    if (status == Status::RollbackFailed) {
        finish(state, status);
    }
}

} // namespace

bool request_specialty_tools(SpecialtyTools& state, const SpecialtyContext& context,
    const domain::SpecialtyRequest& request, const domain::NamedSpecialties& named) {
    if (request.epoch != context.epoch) {
        return false;
    }
    if (request.action == domain::SpecialtyAction::Cancel) {
        finish(state, Status::Cancelled);
        return true;
    }
    if (state.phase != SpecialtyPhase::Idle) {
        state.snapshot.last_error = Status::Busy;
        ++state.snapshot.revision;
        return false;
    }
    if (request.action == domain::SpecialtyAction::Give &&
        (!state.snapshot.available || request.generation != state.snapshot.generation ||
         state.snapshot.epoch != context.epoch || !same_context(state, context))) {
        state.snapshot.last_error = Status::Changed;
        ++state.snapshot.revision;
        return false;
    }
    state.context = context;
    state.cities = city_base(context.module);
    if (!state.cities || !same_context(state, context)) {
        finish(state, Status::SessionChanged);
        state.snapshot.available = false;
        return false;
    }
    state.snapshot.completed = 0;
    state.snapshot.succeeded = 0;
    state.snapshot.failed = 0;
    state.snapshot.last_error = Status::Ready;
    state.snapshot.epoch = context.epoch;
    state.snapshot.busy = true;
    ++state.snapshot.revision;

    if (request.action == domain::SpecialtyAction::Give) {
        state.selected = request.selected;
        state.selected.reset(0);
        state.snapshot.total = state.selected.count();
        state.snapshot.status = Status::Giving;
        state.target_is_player = request.player;
        state.target_id = request.target;
        state.target_roster = request.player ? 0 : resolve_officer_roster_base(context.module);
        if (request.player && !read_value(context.player + 8, state.target_id)) {
            finish(state, Status::InvalidTarget);
            return false;
        }
        if (state.target_id < 1 || state.target_id > 5102 || (!request.player && !state.target_roster)) {
            finish(state, Status::InvalidTarget);
            return false;
        }
        const std::uintptr_t target = request.player ? context.player :
            state.target_roster + (state.target_id - 1) * OFFICER_STRIDE;

        if (!select_target(state, target)) {
            if (request.player) {
                finish(state, Status::InvalidTarget);
                return false;
            }
            state.phase = SpecialtyPhase::Target;
            state.cursor = 0;
        }
    } else {
        state.named = named;
        state.distribute_after_refresh = request.action == domain::SpecialtyAction::Distribute;
        state.snapshot.available = false;
        state.snapshot.status = Status::Reading;
        state.snapshot.total = domain::SPECIALTY_SLOT_COUNT + domain::SPECIALTY_COUNT - 1;
        state.items = 0;
        state.stride = 0x40;
        state.cursor = 0;
        state.phase = SpecialtyPhase::Slots;
        ++state.snapshot.generation;
    }
    return true;
}

void update_specialty_tools(SpecialtyTools& state, const SpecialtyContext& context, std::mt19937& random,
    const domain::WorkBudget& work_budget) {
    if (work_budget.exhausted()) {
        return;
    }
    if (state.snapshot.epoch != context.epoch) {
        state.snapshot.epoch = context.epoch;
        state.snapshot.available = false;
        finish(state, Status::SessionChanged);
        return;
    }
    if (state.phase == SpecialtyPhase::Idle) {
        return;
    }
    if (!same_context(state, context)) {
        state.snapshot.available = false;
        finish(state, Status::SessionChanged);
        return;
    }
    const std::chrono::steady_clock::time_point deadline = std::min(work_budget.deadline, std::chrono::steady_clock::now() + UPDATE_BUDGET);

    for (std::size_t step = 0; step < READS_PER_UPDATE && state.phase != SpecialtyPhase::Idle; ++step) {
        if (state.phase == SpecialtyPhase::Slots) {
            read_slot(state, state.cursor++);
            ++state.snapshot.completed;
            if (state.cursor == domain::SPECIALTY_SLOT_COUNT) {
                if (!resolve_items(state)) {
                    finish(state, Status::ReadFailed);
                    break;
                }
                state.cursor = 1;
                state.phase = SpecialtyPhase::Items;
            }
        } else if (state.phase == SpecialtyPhase::Items) {
            read_item(state, static_cast<std::uint16_t>(state.cursor++));
            ++state.snapshot.completed;
            if (state.cursor == domain::SPECIALTY_COUNT) {
                state.snapshot.available = true;
                if (state.distribute_after_refresh) {
                    plan_distribution(state, random);
                } else {
                    finish(state, Status::Complete);
                }
                break;
            }
        } else if (state.phase == SpecialtyPhase::Target) {
            if (resolve_officer_roster_base(context.module) != state.target_roster) {
                finish(state, Status::SessionChanged);
                break;
            }
            if (select_target(state, state.target_roster + state.cursor * OFFICER_STRIDE)) {
                break;
            }
            if (++state.cursor == 5102) {
                finish(state, Status::InvalidTarget);
            }
        } else if (state.phase == SpecialtyPhase::Distribute) {
            if (state.cursor == state.snapshot.total) {
                finish(state, Status::Complete);
                break;
            }
            record_result(state, distribute_item(state, state.cursor++));
            break;
        } else if (state.phase == SpecialtyPhase::Give) {
            while (state.cursor < domain::SPECIALTY_COUNT && !state.selected.test(state.cursor)) {
                ++state.cursor;
            }
            if (state.cursor == domain::SPECIALTY_COUNT) {
                finish(state, Status::Complete);
                break;
            }
            record_result(state, give_item(state, static_cast<std::uint16_t>(state.cursor++)));
            break;
        }
        if (std::chrono::steady_clock::now() >= deadline) {
            break;
        }
    }
    ++state.snapshot.revision;
}

void stop_specialty_tools(SpecialtyTools& state) {
    if (state.phase != SpecialtyPhase::Idle) {
        finish(state, Status::Cancelled);
    }
}

} // namespace s8::game
