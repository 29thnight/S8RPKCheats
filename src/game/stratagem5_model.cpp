#include "stratagem5_model.h"

#include <algorithm>

namespace s8::game {
namespace {

std::optional<std::uint32_t> row_id(std::uint64_t data, std::uint64_t table) {
    if (!table || data < table || data - table >= 5 * 0x20 || (data - table) % 0x20)
        return std::nullopt;
    return static_cast<std::uint32_t>((data - table) / 0x20 + 1);
}

bool same_identity(const StratagemEntry& left, const StratagemEntry& right) {
    return left.data == right.data && left.index == right.index;
}

} // namespace

std::optional<Stratagem5Plan> prepare_stratagem5_plan(
    const std::array<StratagemEntry, 5>& entries, std::uint32_t count,
    const std::array<std::uint64_t, 5>& camp_rows, std::uint64_t table) {
    if (count < 1 || count > 5)
        return std::nullopt;
    std::uint32_t seen = 0;
    int fifth_entry = -1, fifth_camp = -1, empty_camp = -1;
    for (std::uint32_t i = 0; i < 5; ++i) {
        if (!camp_rows[i]) {
            if (empty_camp < 0)
                empty_camp = static_cast<int>(i);
            continue;
        }
        const auto id = row_id(camp_rows[i], table);
        if (!id)
            return std::nullopt;
        if (*id == 5) {
            if (fifth_camp >= 0)
                return std::nullopt;
            fifth_camp = static_cast<int>(i);
        }
    }
    for (std::uint32_t i = 0; i < count; ++i) {
        const auto id = row_id(entries[i].data, table);
        if (!id || entries[i].index != i || (seen & (1u << *id)))
            return std::nullopt;
        seen |= 1u << *id;
        if (std::find(camp_rows.begin(), camp_rows.end(), entries[i].data) == camp_rows.end())
            return std::nullopt;
        if (*id == 5)
            fifth_entry = static_cast<int>(i);
    }

    Stratagem5Plan plan;
    if (fifth_entry >= 0) {
        if (fifth_camp < 0 || count < 2 || fifth_entry != static_cast<int>(count - 1))
            return std::nullopt;
        plan.original_count = count - 1;
        plan.camp_slot = static_cast<std::uint32_t>(fifth_camp);
        plan.added = entries[fifth_entry];
        plan.already_present = true;
        return plan;
    }
    if (count > 4 || (fifth_camp < 0 && empty_camp < 0))
        return std::nullopt;
    if (entries[count].data || entries[count].index || entries[count].available > 1)
        return std::nullopt;
    plan.original_count = count;
    plan.camp_slot = static_cast<std::uint32_t>(fifth_camp >= 0 ? fifth_camp : empty_camp);
    plan.before = entries[count];
    plan.added = {table + 4 * 0x20, count, 1};
    return plan;
}

bool can_restore_stratagem5_model(const std::array<StratagemEntry, 5>& current,
    std::uint32_t count, const std::array<StratagemEntry, 5>& original,
    const Stratagem5Plan& plan, std::uint64_t camp_row) {
    if (plan.already_present || plan.original_count < 1 || plan.original_count > 4 ||
        count != plan.original_count + 1 || camp_row != plan.added.data ||
        !same_identity(current[plan.original_count], plan.added))
        return false;
    for (std::uint32_t i = 0; i < plan.original_count; ++i)
        if (!same_identity(current[i], original[i]))
            return false;
    return true;
}

int stratagem5_position(int center, int native_step, std::uint32_t total, std::uint32_t index) {
    const int step = total == 5 ? native_step * 7 / 8 : native_step;
    return center - step * static_cast<int>(total - 1) / 2 + step * static_cast<int>(index);
}

} // namespace s8::game
