#include "stratagem5_binding.h"
#include "../platform/windows/process_memory.h"

#include <array>
#include <algorithm>
#include <cstring>
#include <span>

namespace s8::game {
namespace {

constexpr std::size_t HEAL_UNITS_PER_UPDATE = 8;

void reset_healing(Stratagem5Binding& binding) {
    binding.heal_history = {};
    binding.heal_progress = {};
}

bool finish_healing_write(Stratagem5HealingProgress& progress) {
    std::uint16_t current = 0;

    if (!platform::windows::read_memory(progress.target_unit + 0x38,
            std::as_writable_bytes(std::span(&current, 1)))) {
        return false;
    }
    if (current == progress.troops_after || current != progress.troops_before) {
        // Do not repeat an already visible heal or overwrite a later game write.
        progress.write_pending = false;
        return true;
    }

    platform::windows::write_memory(progress.target_unit + 0x38,
        std::as_bytes(std::span(&progress.troops_after, 1)));

    std::uint16_t verified = 0;

    if (!platform::windows::read_memory(progress.target_unit + 0x38,
            std::as_writable_bytes(std::span(&verified, 1))) || verified != progress.troops_after) {
        return false;
    }
    progress.write_pending = false;
    return true;
}

bool apply_table(Stratagem5Binding& binding, domain::GameEpoch epoch, std::uintptr_t table,
                 bool reset_before_reapply) {
    Stratagem5Memory memory{{epoch, static_cast<std::uint64_t>(table)}, table};
    StratagemTableSnapshot snapshot{};

    if (!read_stratagem_table(memory, snapshot)) {
        return false;
    }

    const std::array<domain::StratagemRecord, 5> records = decode_stratagem_table(snapshot);

    if (!is_expected_stratagem_table(records)) {
        return false;
    }
    const std::optional<StratagemTableId>& applied = binding.runtime.session.applied_table;

    if (applied && applied->epoch == epoch && *applied != memory.table) {
        const bool requested = binding.runtime.session.requested;
        const bool restored = stop_stratagem5(binding, epoch);

        binding.runtime.session.requested = requested;
        if (!restored) {
            return false;
        }
    }
    if (reset_before_reapply && binding.runtime.session.applied_table) {
        if (stratagem5_ready(binding.runtime, memory.table, records[4])) {
            return true;
        }
    }
    // Publish recovery identity before the write. A failed or partial write is
    // still ours to recover; the session keeps original/attempted payloads.
    binding.memory = memory;

    const Stratagem5ApplyResult result = apply_stratagem5_table(binding.runtime.session, memory.table, records,
                                               binding.runtime.settings, write_stratagem5, &memory);
    if (result != Stratagem5ApplyResult::applied && result != Stratagem5ApplyResult::already_matches) {
        return false;
    }
    if (result == Stratagem5ApplyResult::applied) {
        reset_healing(binding);
    }
    return true;
}

} // namespace

bool enable_stratagem5(Stratagem5Binding& binding, domain::GameEpoch epoch, std::uintptr_t module_base) {
    binding.runtime.session.requested = true;
    refresh_stratagem5(binding, epoch, module_base);
    return true;
}

bool refresh_stratagem5(Stratagem5Binding& binding, domain::GameEpoch epoch, std::uintptr_t module_base) {
    if (!binding.runtime.session.requested) {
        return false;
    }

    std::uintptr_t address = module_base + 0x034c8630;
    constexpr std::array<std::uintptr_t, 5> OFFSETS{0x0bb8, 0x0000, 0x0170, 0x0010, 0x0d38};
    for (const std::uintptr_t offset : OFFSETS) {
        std::uintptr_t next = 0;
        if (!platform::windows::read_memory(address, std::as_writable_bytes(std::span(&next, 1))) || !next) {
            return false;
        }
        address = next + offset;
    }
    return apply_table(binding, epoch, address, true);
}

bool apply_stratagem5_metadata(Stratagem5Binding& binding, domain::GameEpoch epoch, std::uintptr_t metadata_table) {
    binding.runtime.session.requested = true;
    return apply_table(binding, epoch, metadata_table + 0x08, false);
}

Stratagem5SettingsResult set_stratagem5_settings(Stratagem5Binding& binding, domain::Stratagem5Settings settings) {
    StratagemPayload payload{};
    domain::StratagemRecord current{};
    if (binding.memory && platform::windows::read_memory(
            binding.memory->payload_table + 4 * STRATAGEM_RECORD_STRIDE, payload)) {
        current = decode_stratagem(payload);
        const Stratagem5SettingsResult result = set_stratagem5_settings(binding.runtime, settings, &current,
            write_stratagem5, &*binding.memory);

        if (result == Stratagem5SettingsResult::updated) {
            reset_healing(binding);
        }
        return result;
    }
    return set_stratagem5_settings(binding.runtime, settings, nullptr, write_stratagem5, nullptr);
}

bool disable_stratagem5(Stratagem5Binding& binding) {
    const domain::GameEpoch epoch = binding.runtime.session.applied_table
        ? binding.runtime.session.applied_table->epoch : domain::GameEpoch{0};

    return stop_stratagem5(binding, epoch);
}

bool stop_stratagem5(Stratagem5Binding& binding, domain::GameEpoch epoch) {

    Stratagem5Session& session = binding.runtime.session;

    session.requested = false;
    reset_healing(binding);

    if (!session.applied_table) {
        return session.last_restore != Stratagem5RestoreStatus::Failed;
    }

    if (session.applied_table->epoch != epoch) {
        reset_stratagem5_session(session);
        binding.memory.reset();
        reset_healing(binding);
        return true;
    }

    if (!binding.memory || binding.memory->table != *session.applied_table) {
        return false;
    }

    StratagemTableSnapshot snapshot{};

    if (!read_stratagem_table(*binding.memory, snapshot)) {
        return false;
    }

    const std::array<domain::StratagemRecord, 5> records = decode_stratagem_table(snapshot);
    const StratagemPayload original = encode_stratagem(session.original);
    const StratagemPayload current = encode_stratagem(records[4]);
    const StratagemPayload last_written = encode_stratagem(session.last_written);
    const StratagemPayload attempted = encode_stratagem(session.attempted);
    bool owned = true;

    for (std::size_t index = 0; owned && index < current.size(); ++index) {
        owned = current[index] == original[index] || current[index] == last_written[index] ||
            (session.uncertain && current[index] == attempted[index]);
    }

    if (!is_expected_stratagem_table(records) || !owned) {
        reset_stratagem5_session(session);
        binding.memory.reset();
        reset_healing(binding);
        return true;
    }

    if (current != original) {

        StratagemPayload verified{};

        if (!write_stratagem5(&*binding.memory, *session.applied_table, original) ||
            !platform::windows::read_memory(binding.memory->payload_table + 4 * STRATAGEM_RECORD_STRIDE, verified) ||
            verified != original) {
            session.last_restore = Stratagem5RestoreStatus::Failed;
            return false;
        }
    }

    session.last_restore = Stratagem5RestoreStatus::Restored;
    session.applied_table.reset();
    session.original = {};
    session.last_written = {};
    session.attempted = {};
    session.uncertain = false;
    binding.memory.reset();
    reset_healing(binding);
    return true;
}

void end_stratagem5_battle(Stratagem5Binding& binding) {
    const bool requested = binding.runtime.session.requested;

    disable_stratagem5(binding);
    binding.runtime.session.requested = requested;
    reset_healing(binding);
}

void change_stratagem5_session(Stratagem5Binding& binding) {
    reset_stratagem5_session(binding.runtime.session);
    binding.memory.reset();
    reset_healing(binding);
}

void update_stratagem5_healing(Stratagem5Binding& binding, int unit_count, std::uintptr_t unit_list,
                              const domain::WorkBudget& work_budget) {
    if (!binding.runtime.session.requested || binding.runtime.session.uncertain ||
        !binding.runtime.session.applied_table || unit_list <= 0x10000 || unit_count <= 0) {
        return;
    }

    const StratagemTableId table = *binding.runtime.session.applied_table;
    const std::size_t observed_count = static_cast<std::size_t>(std::min(unit_count, 60));
    Stratagem5HealingProgress& progress = binding.heal_progress;

    if (progress.unit_list != unit_list || progress.table != table) {
        reset_healing(binding);
        progress.unit_list = unit_list;
        progress.table = table;
    }
    if (!progress.pending) {
        progress.pending = true;
        progress.any_valid = false;
        progress.unit_count = observed_count;
        progress.unit_cursor = 0;
        progress.write_pending = false;
        progress.history = binding.heal_history;
        begin_stratagem5_healing(progress.history, static_cast<std::uint64_t>(unit_list));
    }

    const bool bounded = work_budget.deadline != (std::chrono::steady_clock::time_point::max)();
    const std::size_t count = std::min(progress.unit_count, observed_count);
    std::size_t processed = 0;

    while (progress.unit_cursor < count) {
        if (work_budget.exhausted() || (bounded && processed >= HEAL_UNITS_PER_UPDATE)) {
            return;
        }
        ++processed;

        const std::size_t index = progress.unit_cursor;
        std::uintptr_t unit = 0;

        if (!platform::windows::read_memory(unit_list + 0x08 + index * 0x10,
                std::as_writable_bytes(std::span(&unit, 1)))) {
            if (progress.write_pending) {
                return;
            }
            observe_stratagem5_unit(progress.history, index, std::nullopt, binding.runtime.settings);
            ++progress.unit_cursor;
            continue;
        }
        if (progress.write_pending) {
            if (unit == progress.target_unit) {
                if (!finish_healing_write(progress)) {
                    return;
                }
                ++progress.unit_cursor;
                continue;
            }
            progress.write_pending = false;
        }
        if (!unit) {
            observe_stratagem5_unit(progress.history, index, std::nullopt, binding.runtime.settings);
            ++progress.unit_cursor;
            continue;
        }

        std::array<std::byte, 0x23c> data{};

        if (!platform::windows::read_memory(unit, data)) {
            observe_stratagem5_unit(progress.history, index, std::nullopt, binding.runtime.settings);
            ++progress.unit_cursor;
            continue;
        }

        std::uintptr_t max_troops_address = 0;
        std::uint16_t troops = 0;

        std::memcpy(&max_troops_address, data.data() + 0x18, sizeof(max_troops_address));
        std::memcpy(&troops, data.data() + 0x38, sizeof(troops));

        Stratagem5UnitSnapshot snapshot{static_cast<std::uint64_t>(unit),
                                         std::to_integer<std::uint8_t>(data[0x80]),
                                         troops, std::nullopt};
        std::uint16_t max_troops = 0;

        if (max_troops_address && platform::windows::read_memory(
                max_troops_address, std::as_writable_bytes(std::span(&max_troops, 1)))) {
            snapshot.max_troops = max_troops;
        }
        progress.any_valid = true;

        const std::optional<std::uint16_t> change = observe_stratagem5_unit(
            progress.history, index, snapshot, binding.runtime.settings);

        if (change) {
            progress.target_unit = unit;
            progress.troops_before = troops;
            progress.troops_after = *change;
            progress.write_pending = true;
            if (!finish_healing_write(progress)) {
                return;
            }
        }
        ++progress.unit_cursor;
    }
    finish_stratagem5_healing(progress.history, count, progress.any_valid);
    binding.heal_history = progress.history;
    progress.write_pending = false;
    progress.pending = false;
}

} // namespace s8::game
