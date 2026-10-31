#pragma once

#include "../domain/work_budget.h"

#include "../domain/native_officer_names.h"

#include <array>
#include <bitset>

namespace s8::game {

struct NativeOfficerNameContext {
    std::uintptr_t module = 0;
    std::uintptr_t game_base = 0;
    std::uintptr_t game_base_pointer = 0;
    domain::GameEpoch epoch{};
    std::uint64_t now_ms = 0;
};

struct NativeOfficerNames {
    domain::NativeOfficerNameBatch output;
    domain::GameEpoch epoch{};
    std::uintptr_t module = 0;
    std::uintptr_t game_base = 0;
    std::uintptr_t roster = 0;
    std::uintptr_t scenario = 0;
    std::uintptr_t prepared_module = 0;
    std::uintptr_t layout_module = 0;
    std::uint64_t generation = 0;
    std::uint64_t last_full_ms = 0;
    std::uint64_t last_children_ms = 0;
    std::size_t cursor = 0;
    std::size_t child_cursor = 0;
    bool getters_ready = false;
    bool layout_supported = false;
    bool scanning = false;
    bool polling_children = false;
    bool child_names_read = false;
    bool pending = false;
    std::bitset<5103> seen;
    std::array<std::uint16_t, 20> child_slots{};
};

// Serialized game-owner only. No strings, file access or heap allocation.
// Keep pending output until the service queue has accepted the value batch.
void update_native_officer_names(NativeOfficerNames& state, const NativeOfficerNameContext& context,
    const domain::WorkBudget& work_budget = {});

} // namespace s8::game
