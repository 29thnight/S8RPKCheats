#include "start_setting.h"

#include "../platform/windows/process_memory.h"

#include <array>
#include <cstddef>
#include <span>

namespace s8::game {
namespace {

constexpr std::uintptr_t ROOT_RVA = 0x034c8630;
constexpr std::array<std::uintptr_t, 5> ROOT_CHAIN{0, 0, 8, 0x10, 0};
constexpr std::uint64_t TICK_INTERVAL_MS = 200;

template <typename T>
bool read_value(std::uintptr_t address, T& value) {

    return platform::windows::read_memory(address, std::as_writable_bytes(std::span(&value, 1)));
}

struct DataWrite {
    std::uintptr_t address = 0;
    std::uint64_t value = 0;
    std::size_t size = 0;
    std::uint64_t original = 0;
};

bool apply_writes(std::span<DataWrite> writes) {

    for (DataWrite& write : writes) {

        if (!platform::windows::read_memory(
                write.address, std::as_writable_bytes(std::span(&write.original, 1)).first(write.size))) {
            return false;
        }
    }

    for (std::size_t index = 0; index < writes.size(); ++index) {

        const DataWrite& write = writes[index];

        if (write.original == write.value) {
            continue;
        }

        if (platform::windows::write_data_memory(
                write.address, std::as_bytes(std::span(&write.value, 1)).first(write.size))) {
            continue;
        }

        // A two-field relationship must not be left half swapped on failure.
        for (std::size_t rollback = index + 1; rollback > 0; --rollback) {

            const DataWrite& saved = writes[rollback - 1];
            std::uint64_t current = 0;

            if (platform::windows::read_memory(
                    saved.address, std::as_writable_bytes(std::span(&current, 1)).first(saved.size)) &&
                current == saved.value) {
                platform::windows::write_data_memory(
                    saved.address, std::as_bytes(std::span(&saved.original, 1)).first(saved.size));
            }
        }

        return false;
    }

    return true;
}

std::uintptr_t resolve_root(std::uintptr_t module_base) {

    if (module_base == 0) {
        return 0;
    }

    std::uintptr_t root = module_base + ROOT_RVA;

    for (const std::uintptr_t offset : ROOT_CHAIN) {

        std::uintptr_t next = 0;

        if (!read_value(root + offset, next) || next <= 0x10000) {
            return 0;
        }

        root = next;
    }

    return root;
}

void finish_fix(StartSettingState& state, std::size_t index, bool condition,
                std::span<DataWrite> writes, std::uint8_t& changed) {

    if (!condition) {
        state.applied[index] = false;
        return;
    }

    if (!state.applied[index] && apply_writes(writes)) {
        state.applied[index] = true;
        changed |= static_cast<std::uint8_t>(1u << index);
    }
}

} // namespace

std::uint8_t update_start_setting(StartSettingState& state, std::uintptr_t module_base,
                                 std::uint64_t now_ms, bool enabled) {

    if (!enabled) {
        state = {};
        return 0;
    }

    if (state.last_tick_ms != 0 && now_ms - state.last_tick_ms < TICK_INTERVAL_MS) {
        return 0;
    }

    state.last_tick_ms = now_ms;

    const std::uintptr_t root = resolve_root(module_base);

    if (root != state.root) {
        state.root = root;
        state.applied = {};
    }

    std::int16_t year = -1;

    if (!root || !read_value(root + 0x49eb06, year)) {
        return 0;
    }

    if (year != 0) {
        state.applied = {};
        return 0;
    }

    std::uint8_t changed = 0;
    std::uintptr_t revenge = 0;
    std::uintptr_t enemy = 0;
    const bool relation_read = read_value(root + 0x4172d0, revenge) &&
                               read_value(root + 0x4172d8, enemy);
    constexpr std::uintptr_t GUAN_YU = 0x1f7200;
    constexpr std::uintptr_t CAO_HONG = 0x24ddc0;

    if (relation_read && revenge == root + CAO_HONG && enemy == root + GUAN_YU) {
        state.applied[0] = true;
    } else {

        std::array<DataWrite, 2> writes{{
            {root + 0x4172d0, root + CAO_HONG, 8},
            {root + 0x4172d8, root + GUAN_YU, 8}}};
        const bool matches = relation_read && revenge == root + GUAN_YU && enemy == root + CAO_HONG &&
            platform::windows::is_committed_memory(root + GUAN_YU, 8) &&
            platform::windows::is_committed_memory(root + CAO_HONG, 8);

        finish_fix(state, 0, matches, writes, changed);
    }

    std::uintptr_t lord = 0;
    std::array<DataWrite, 2> technology{{{root + 0xc73e7, 3, 1}, {root + 0xc73e8, 4, 1}}};
    const bool zhuge_lord = read_value(root + 0xc72a0, lord) && lord == root + 0x23cf10;

    finish_fix(state, 1, zhuge_lord, technology, changed);

    std::uint8_t z_status = 0;
    std::uint8_t b_status = 0;
    std::array<DataWrite, 1> ronin{{{root + 0x27e790, 88, 1}}};
    const bool ronin_scenario = read_value(root + 0x1f2200, z_status) &&
        read_value(root + 0x24da00, b_status) && z_status == 200 && b_status == 200;

    finish_fix(state, 2, ronin_scenario, ronin, changed);

    constexpr std::uintptr_t FACTION_STRIDE = 2456;
    std::array<DataWrite, 16> alliance{};
    bool four_lords = true;

    for (std::size_t index = 0; index < 4; ++index) {

        const std::uintptr_t slot = root + 0xc6a0a + index * FACTION_STRIDE;
        const std::uintptr_t friendship = root + 0xc6919 + index * FACTION_STRIDE;
        std::uint8_t alliance_id = 0;

        four_lords = read_value(slot, alliance_id) && alliance_id == 23 && four_lords;
        alliance[index * 4] = {slot, 0, 1};
        alliance[index * 4 + 1] = {slot + 6, 0, 8};
        alliance[index * 4 + 2] = {slot + 14, 0, 8};
        alliance[index * 4 + 3] = {friendship, 0, 4};
    }

    finish_fix(state, 3, four_lords, alliance, changed);

    std::uint8_t second_status = 0;
    std::uintptr_t force = 0;
    std::uintptr_t city = 0;
    const bool transfer_scenario = read_value(root + 0x24da00, b_status) &&
        read_value(root + 0x251700, second_status) && b_status == 200 && second_status == 200 &&
        read_value(root + 0x24da08, force) && read_value(root + 0x24da10, city);
    std::array<DataWrite, 2> transfer{{{root + 0x1fb2e8, force, 8}, {root + 0x1fb2f0, city, 8}}};

    finish_fix(state, 4, transfer_scenario, transfer, changed);
    return changed;
}

} // namespace s8::game
