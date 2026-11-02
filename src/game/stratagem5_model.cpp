#include "stratagem5_model.h"

#include <algorithm>

namespace s8::game {

std::optional<Stratagem5Plan> prepare_stratagem5_plan(
    const std::array<StratagemEntry, 5>& entries, std::uint32_t count,
    const std::array<std::uint64_t, 5>& camp_rows, std::uint64_t table) {
    if (!table || !count || count > entries.size())
        return std::nullopt;

    // Index the five metadata rows by their addresses. Each model row must
    // refer to an admitted camp row and may occur only once in the model.
    std::array<std::uint64_t, 5> addresses;
    std::array<bool, 5> admitted{};
    std::array<bool, 5> used{};
    std::optional<std::uint32_t> insertion_slot;
    std::optional<std::uint32_t> existing_slot;

    for (std::size_t row = 0; row < addresses.size(); ++row)
        addresses[row] = table + row * 0x20;

    for (std::uint32_t slot = 0; slot < camp_rows.size(); ++slot) {
        const std::uint64_t address = camp_rows[slot];
        if (!address) {
            if (!insertion_slot)
                insertion_slot = slot;
        } else {
            const auto row = std::find(addresses.begin(), addresses.end(), address);
            if (row == addresses.end())
                return std::nullopt;
            admitted[row - addresses.begin()] = true;
            if (address == addresses.back()) {
                if (existing_slot)
                    return std::nullopt;
                existing_slot = slot;
            }
        }
    }

    for (std::uint32_t slot = 0; slot < count; ++slot) {
        const StratagemEntry& entry = entries[slot];
        const auto row = std::find(addresses.begin(), addresses.end(), entry.data);
        if (row == addresses.end() || entry.index != slot)
            return std::nullopt;
        const std::size_t id = row - addresses.begin();
        if (!admitted[id] || used[id])
            return std::nullopt;
        used[id] = true;
    }

    if (used.back()) {
        if (count == 1 || entries[count - 1].data != addresses.back())
            return std::nullopt;
        return Stratagem5Plan{count - 1, *existing_slot, {}, entries[count - 1], true};
    }

    const std::optional<std::uint32_t> destination = existing_slot ? existing_slot : insertion_slot;
    if (!destination || count == entries.size())
        return std::nullopt;
    const StratagemEntry& spare = entries[count];
    if (spare.data != 0 || spare.index != 0 || spare.available > 1)
        return std::nullopt;
    return Stratagem5Plan{count, *destination, spare, {addresses.back(), count, 1}, false};
}

bool can_restore_stratagem5_model(const std::array<StratagemEntry, 5>& current,
    std::uint32_t count, const std::array<StratagemEntry, 5>& original,
    const Stratagem5Plan& plan, std::uint64_t camp_row) {
    if (plan.already_present || !plan.original_count || plan.original_count >= current.size() ||
        count - 1 != plan.original_count || camp_row != plan.added.data)
        return false;
    const StratagemEntry& inserted = current[plan.original_count];
    return inserted.data == plan.added.data && inserted.index == plan.added.index &&
        std::equal(current.begin(), current.begin() + plan.original_count, original.begin(),
            [](const StratagemEntry& live, const StratagemEntry& before) {
                return live.index == before.index && live.data == before.data;
            });
}

int stratagem5_position(int center, int native_step, std::uint32_t total, std::uint32_t index) {
    const int spacing = total == 5 ? native_step * 7 / 8 : native_step;
    const int left = center - spacing * static_cast<int>(total - 1) / 2;
    return left + spacing * static_cast<int>(index);
}

} // namespace s8::game
