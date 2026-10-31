#pragma once

#include <array>
#include <cstdint>
#include <optional>

namespace s8::game {

struct StratagemEntry {
    std::uint64_t data = 0;
    std::uint32_t index = 0;
    std::uint32_t available = 0;
};

struct Stratagem5Plan {
    std::uint32_t original_count = 0;
    std::uint32_t camp_slot = 0;
    StratagemEntry before{};
    StratagemEntry added{};
    bool already_present = false;
};

std::optional<Stratagem5Plan> prepare_stratagem5_plan(
    const std::array<StratagemEntry, 5>& entries, std::uint32_t count,
    const std::array<std::uint64_t, 5>& camp_rows, std::uint64_t table);
bool can_restore_stratagem5_model(const std::array<StratagemEntry, 5>& current,
    std::uint32_t count, const std::array<StratagemEntry, 5>& original,
    const Stratagem5Plan& plan, std::uint64_t camp_row);
int stratagem5_position(int center, int native_step, std::uint32_t total, std::uint32_t index);

} // namespace s8::game
