#include "../src/domain/stratagem5.h"
#include "../src/domain/secondary_jewel.h"
#include "../src/game/stratagem5_runtime.h"
#include "../src/game/stratagem5_heal.h"
#include "../src/game/stratagem5_config.h"
#include "../src/game/stratagem5_model.h"
#include "../src/game/legacy_config.h"
#include "../src/game/game_base.h"
#include "../src/runtime/stratagem5_lifecycle.h"
#include "../src/services/command_mailbox.h"
#include "../src/runtime/command_commit.h"
#include "../src/services/config_save_queue.h"
#include "../src/services/config_changes.h"
#include "../src/services/config_json.h"
#include "../src/services/config_save_coordinator.h"
#include "../src/services/config_save_pump.h"
#include "../src/game/find_bytes.h"
#include "../src/game/love_hate_targets.h"
#include "../src/game/battle_map_offsets.h"
#include "../src/platform/windows/process_memory.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <memory>
#include <vector>

using namespace s8;

namespace {

struct WriteCapture {
    bool succeeds = true;
    int calls = 0;
    game::StratagemPayload payload{};
};

bool capture_write(void* context, game::StratagemTableId, const game::StratagemPayload& payload) {
    auto& capture = *static_cast<WriteCapture*>(context);
    ++capture.calls;
    capture.payload = payload;
    return capture.succeeds;
}

std::array<domain::StratagemRecord, 5> original_table() {
    std::array<domain::StratagemRecord, 5> records{};
    for (std::size_t i = 0; i < records.size(); ++i)
        records[i].code1 = records[i].code2 = records[i].code3 = static_cast<std::int16_t>(i + 1);
    records[0].target = 2; records[0].effect1 = 10; records[0].power1 = -40; records[0].range = 8;
    records[1].target = 2; records[1].effect1 = 11; records[1].power1 = 200;
    records[1].effect2 = 12; records[1].power2 = 100; records[1].range = 5; records[1].unknown16 = -13;
    records[2].target = 2; records[2].effect1 = 3; records[2].power1 = 3; records[2].duration1 = 1; records[2].range = 5;
    records[3].target = 2; records[3].effect1 = 3; records[3].power1 = 2; records[3].duration1 = 3; records[3].range = 6;
    records[4].unknown16 = 73;
    return records;
}

void normalization_and_record_roundtrip() {
    domain::Stratagem5Settings input;
    input.target = -1; input.effect1 = 20; input.effect2 = 12;
    input.power1 = -40000; input.power2 = 40000;
    input.duration1 = -1; input.duration2 = 31; input.range = 101; input.heal_amount = 70000;
    const auto settings = domain::normalize_stratagem5(input);
    assert(settings.target == 1 && settings.effect1 == 0 && settings.effect2 == 12);
    assert(settings.power1 == -32768 && settings.power2 == 32767);
    assert(settings.duration1 == 0 && settings.duration2 == 30);
    assert(settings.range == 100 && settings.heal_amount == 65535);

    auto record = domain::apply_stratagem5(original_table()[1], settings);
    const auto decoded = game::decode_stratagem(game::encode_stratagem(record));
    assert(domain::matches_stratagem5(decoded, settings));
    assert(decoded.unknown16 == -13);
}

void apply_restore_and_failure() {
    auto records = original_table();
    assert(game::is_expected_stratagem_table(records));
    game::Stratagem5Runtime runtime;
    game::StratagemTableId table{domain::GameEpoch{1}, 0x1000};
    WriteCapture capture;
    runtime.session.requested = true;
    assert(game::apply_stratagem5_table(runtime.session, table, records, runtime.settings,
                                        capture_write, &capture) == game::Stratagem5ApplyResult::applied);
    auto updated = game::decode_stratagem(capture.payload);
    assert(updated.unknown16 == -13 && game::stratagem5_ready(runtime, table, updated));
    records[4] = updated;
    assert(game::apply_stratagem5_table(runtime.session, table, records, runtime.settings,
                                        capture_write, &capture) == game::Stratagem5ApplyResult::already_matches);
    assert(capture.calls == 1);
    assert(game::restore_stratagem5(runtime.session, capture_write, &capture));
    assert(game::decode_stratagem(capture.payload).unknown16 == 73);
    assert(runtime.session.requested && !runtime.session.applied_table);

    records = original_table();
    capture.succeeds = false;
    assert(game::apply_stratagem5_table(runtime.session, table, records, runtime.settings,
                                        capture_write, &capture) == game::Stratagem5ApplyResult::write_failed);
    assert(!runtime.session.applied_table && runtime.session.requested);
    records[2].range = 4;
    const int previous_calls = capture.calls;
    assert(game::apply_stratagem5_table(runtime.session, table, records, runtime.settings,
                                        capture_write, &capture) == game::Stratagem5ApplyResult::invalid_table);
    assert(capture.calls == previous_calls);
}

void morale_healing() {
    game::Stratagem5HealHistory history;
    domain::Stratagem5Settings settings;
    std::array<std::optional<game::Stratagem5UnitSnapshot>, 1> units{
        game::Stratagem5UnitSnapshot{11, 50, 1000, 2500}};
    auto changes = game::observe_stratagem5_healing(history, 100, units, settings);
    assert(!changes[0]);
    units[0]->morale = 90;
    changes = game::observe_stratagem5_healing(history, 100, units, settings);
    assert(changes[0] == 2500);
    units[0]->morale = 100;
    changes = game::observe_stratagem5_healing(history, 100, units, settings);
    assert(changes[0] == 2500);
    units[0]->identity = 12;
    units[0]->morale = 50;
    changes = game::observe_stratagem5_healing(history, 100, units, settings);
    assert(!changes[0]);
    units[0]->morale = 90;
    changes = game::observe_stratagem5_healing(history, 101, units, settings);
    assert(!changes[0]);
}

void legacy_settings() {
    domain::Stratagem5Settings current;
    current.duration2 = 7;
    const auto config = game::read_stratagem5_config(
        "{\n  \"bStratagemFiveEnabled\": true,\n"
        "  \"iStratagem5Target\": +5,\n"
        "  \"iStratagem5Power1\": -40000,\n"
        "  \"iStratagem5Effect2\": 20,\n}", current);
    const auto normalized = domain::normalize_stratagem5(config.settings);
    assert(config.enabled && normalized.target == 3);
    assert(normalized.power1 == -32768 && normalized.effect2 == 0);
    assert(normalized.duration2 == 7);
    assert(game::read_legacy_int("\"days\": +12,\n", "\"days\"") == 12);
    assert(!game::read_legacy_int("\"days\": +-12,\n", "\"days\""));
    assert(!game::read_legacy_int("\"days\": 999999999999999,\n", "\"days\""));
    assert(game::read_legacy_bool("\"enabled\": false,\n", "\"enabled\"") == false);
    assert(!game::read_legacy_bool("{}", "\"enabled\""));
}

void deferred_lifecycle() {
    runtime::Stratagem5Lifecycle lifecycle;
    struct Calls { int count = 0; } calls;
    runtime::Stratagem5LifecycleActions actions{
        &calls,
        [](void* context) { ++static_cast<Calls*>(context)->count; return false; },
        [](void* context, bool) { ++static_cast<Calls*>(context)->count; return false; },
        [](void* context, bool) { ++static_cast<Calls*>(context)->count; return false; },
        [](void* context, bool) { ++static_cast<Calls*>(context)->count; return false; },
    };
    runtime::request_stratagem5(lifecycle, true);
    assert(runtime::update_stratagem5_lifecycle(lifecycle, true, false, actions));
    assert(calls.count == 0 && runtime::should_skip_stratagem5(lifecycle));
    assert(!runtime::update_stratagem5_lifecycle(lifecycle, false, true, actions));
    assert(calls.count == 4 && !runtime::should_skip_stratagem5(lifecycle));
    assert(!lifecycle.last_transition_succeeded.load());
    runtime::request_stratagem5(lifecycle, false);
    assert(!runtime::update_stratagem5_lifecycle(lifecycle, false, true, actions));
    assert(calls.count == 7 && runtime::should_skip_stratagem5(lifecycle));
    assert(runtime::update_stratagem5_lifecycle(lifecycle, false, true, actions));
    assert(calls.count == 7);
}

void model_plan() {
    constexpr std::uint64_t table = 0x1000;
    std::array<game::StratagemEntry, 5> entries{};
    std::array<std::uint64_t, 5> camp{};
    for (std::uint32_t i = 0; i < 4; ++i) {
        entries[i] = {table + i * 0x20, i, 1};
        camp[i] = entries[i].data;
    }
    const auto plan = game::prepare_stratagem5_plan(entries, 4, camp, table);
    assert(plan && !plan->already_present && plan->original_count == 4 && plan->camp_slot == 4);
    auto applied = entries;
    applied[4] = plan->added;
    applied[4].available = 0;
    assert(game::can_restore_stratagem5_model(applied, 5, entries, *plan, plan->added.data));
    camp[4] = plan->added.data;
    const auto existing = game::prepare_stratagem5_plan(applied, 5, camp, table);
    assert(existing && existing->already_present && existing->added.available == 0);
    assert(!game::can_restore_stratagem5_model(applied, 5, entries, *existing, plan->added.data));
    applied[1].index = 0;
    assert(!game::prepare_stratagem5_plan(applied, 5, camp, table));
}

void game_base_pattern() {
    const std::array image{
        std::byte{0x4c}, std::byte{0x8b}, std::byte{0x05},
        std::byte{0x10}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x41}, std::byte{0x0f}, std::byte{0xb7}, std::byte{0x00}};
    assert(game::find_game_base_pointer(image, 0x1000) == 0x1017);
    assert(game::find_game_base_pointer(std::span(image).first<10>(), 0x1000) == 0x1017);
    auto mismatch = image;
    mismatch[9] = std::byte{0xb6};
    assert(!game::find_game_base_pointer(mismatch, 0x1000));
}

void command_mailbox() {
    services::CommandMailbox mailbox;
    for (std::uint64_t i = 0; i < services::CommandMailbox::CAPACITY; ++i) {
        domain::Command command{domain::RequestId{i}, domain::GameEpoch{1},
                                domain::SetFeature{domain::FeatureId::InfiniteAp, true}};
        assert(mailbox.push(command));
    }
    domain::Command overflow{domain::RequestId{999}, domain::GameEpoch{1},
                             domain::SetFeature{domain::FeatureId::InfiniteAp, false}};
    const auto result = mailbox.push(overflow);
    assert(!result && result.error().code == domain::ErrorCode::QueueFull);
    std::array<services::CompletionToken, services::CommandMailbox::CAPACITY> tokens{};
    for (std::uint64_t i = 0; i < services::CommandMailbox::CAPACITY; ++i) {
        const auto command = mailbox.pop();
        assert(command && command->command.request.value == i);
        tokens[i] = command->completion;
    }
    assert(!mailbox.pop());
    assert(!mailbox.push(overflow));
    for (const auto token : tokens) {
        assert(mailbox.complete(token, {}));
        assert(!mailbox.complete(token, {}));
    }
    assert(!mailbox.push(overflow));
    for (std::uint64_t i = 0; i < services::CommandMailbox::CAPACITY; ++i) {
        const auto completion = mailbox.take_completion();
        assert(completion && completion->request.value == i && completion->result);
    }
    assert(!mailbox.take_completion());
    assert(mailbox.push(overflow));
    const auto accepted = mailbox.pop();
    assert(accepted && accepted->command.request.value == 999);
    assert(!mailbox.complete(tokens[0], {}));
    assert(mailbox.complete(accepted->completion, std::unexpected(domain::Error{domain::ErrorCode::StaleEpoch})));
    const auto completion = mailbox.take_completion();
    assert(completion && !completion->result && completion->result.error().code == domain::ErrorCode::StaleEpoch);
    mailbox.close();
    const auto closed = mailbox.push(overflow);
    assert(!closed && closed.error().code == domain::ErrorCode::Cancelled);
}

void command_commit() {
    runtime::Runtime state;
    state.epoch = domain::GameEpoch{2};
    services::CommandMailbox mailbox;
    assert(mailbox.push({domain::RequestId{1}, domain::GameEpoch{1},
                         domain::SetFeature{domain::FeatureId::InfiniteAp, true}}));
    assert(mailbox.push({domain::RequestId{2}, domain::GameEpoch{2},
                         domain::SetFeature{domain::FeatureId::InfiniteAp, true}}));
    assert(mailbox.push({domain::RequestId{3}, domain::GameEpoch{2},
                         domain::SetFeature{domain::FeatureId::SpeedHack, true}}));
    assert(runtime::commit_commands(state, mailbox, 0, 1, std::chrono::seconds{1}) == 1);
    auto completion = mailbox.take_completion();
    assert(completion && !completion->result && completion->result.error().code == domain::ErrorCode::StaleEpoch);
    assert(!state.recurring_features.infinite_action_points);
    assert(runtime::commit_commands(state, mailbox, 0, 2, std::chrono::seconds{1}) == 2);
    completion = mailbox.take_completion();
    assert(completion && completion->request.value == 2 && completion->result);
    assert(state.recurring_features.infinite_action_points && state.features.get(domain::FeatureId::InfiniteAp));
    completion = mailbox.take_completion();
    assert(completion && completion->request.value == 3 && !completion->result);
    assert(completion->result.error().code == domain::ErrorCode::InvalidData);
    assert(!state.features.get(domain::FeatureId::SpeedHack));
    assert(mailbox.push({domain::RequestId{4}, domain::GameEpoch{2},
                         domain::SetFeature{domain::FeatureId::InfiniteTalk, true}}));
    assert(mailbox.push({domain::RequestId{5}, domain::GameEpoch{2},
                         domain::SetFeature{domain::FeatureId::BigCity, true}}));
    assert(runtime::commit_commands(state, mailbox, 0, 2, std::chrono::seconds{1}) == 2);
    assert(state.infinite_talk_enabled && state.features.get(domain::FeatureId::InfiniteTalk));
    assert(state.features.get(domain::FeatureId::BigCity) && !state.city_config_applied[0]);
    assert(mailbox.take_completion()->result);
    assert(mailbox.take_completion()->result);
    assert(mailbox.push({domain::RequestId{6}, domain::GameEpoch{2}, domain::SetTroopCountCombat{true}}));
    assert(mailbox.push({domain::RequestId{7}, domain::GameEpoch{2}, domain::SetReinforcementArrival{true}}));
    assert(mailbox.push({domain::RequestId{8}, domain::GameEpoch{2}, domain::SetTechCityVisibility{true}}));
    assert(runtime::commit_commands(state, mailbox, 0, 3, std::chrono::seconds{1}) == 3);
    for (int i = 0; i < 3; ++i) {
        completion = mailbox.take_completion();
        assert(completion && !completion->result && completion->result.error().code == domain::ErrorCode::PlatformFailure);
    }
    assert(!state.troop_count_combat_enabled && !state.reinforcement_arrival_enabled && !state.tech_city_visibility_enabled);
    assert(mailbox.push({domain::RequestId{9}, domain::GameEpoch{2}, domain::SetShortBattleCooldown{false, 7}}));
    assert(runtime::commit_commands(state, mailbox, 0, 1, std::chrono::seconds{1}) == 1);
    completion = mailbox.take_completion();
    assert(completion && completion->result && state.short_battle_cooldown_days == 7);
    assert(mailbox.push({domain::RequestId{10}, domain::GameEpoch{2},
                         domain::SetFeature{domain::FeatureId::MarriageApplied, true}}));
    assert(runtime::commit_commands(state, mailbox, 0, 1, std::chrono::seconds{1}) == 1);
    completion = mailbox.take_completion();
    assert(completion && completion->result && state.marriage_enabled);
    assert(!state.marriage_condition.applied);
    domain::SelfHealSettings heal;
    heal.amount[1] = 4321;
    heal.self[0] = false;
    assert(mailbox.push({domain::RequestId{11}, domain::GameEpoch{2}, domain::SetSelfHealSettings{heal}}));
    assert(runtime::commit_commands(state, mailbox, 0, 1, std::chrono::seconds{1}) == 1);
    completion = mailbox.take_completion();
    assert(completion && !completion->result && completion->result.error().code == domain::ErrorCode::PlatformFailure);
    assert(state.self_heal_settings.amount[1] == 4321 && !state.features.get(domain::FeatureId::HealLv1Self));
    domain::CelestialSettings celestial;
    celestial.amount[2] = 12345;
    assert(mailbox.push({domain::RequestId{12}, domain::GameEpoch{2}, domain::SetCelestialSettings{celestial}}));
    assert(runtime::commit_commands(state, mailbox, 0, 1, std::chrono::seconds{1}) == 1);
    completion = mailbox.take_completion();
    assert(completion && !completion->result && state.celestial_settings.amount[2] == 12345);
}

void config_save_queue() {
    runtime::Runtime state;
    services::ConfigSaveQueue queue;
    state.features.set(domain::FeatureId::InfiniteAp, true);
    const auto first = queue.submit(runtime::config_snapshot(state));
    auto save = queue.start_next();
    assert(save && save->revision == first && save->snapshot.features.get(domain::FeatureId::InfiniteAp));
    state.short_battle_cooldown_days = 4;
    const auto second = queue.submit(runtime::config_snapshot(state));
    state.short_battle_cooldown_days = 5;
    const auto third = queue.submit(runtime::config_snapshot(state));
    assert(second < third && !queue.start_next());
    assert(!queue.finish(third));
    assert(queue.finish(first));
    save = queue.start_next();
    assert(save && save->revision == third && save->snapshot.short_battle_cooldown_days == 5);
    assert(queue.finish(third) && !queue.start_next());
}

void ui_bridge() {
    runtime::Runtime state;
    state.epoch = domain::GameEpoch{3};
    services::UiBridge bridge;
    assert(!bridge.snapshot());
    assert(runtime::commit_ui_commands(state, bridge, 0, 1, std::chrono::seconds{1}) == 0);
    assert(bridge.snapshot() && bridge.snapshot()->epoch == state.epoch);
    assert(bridge.commands.push({domain::RequestId{1}, state.epoch,
                                 domain::SetFeature{domain::FeatureId::InfiniteAp, true}}));
    assert(runtime::commit_ui_commands(state, bridge, 0, 1, std::chrono::seconds{1}) == 1);
    const auto completion = bridge.commands.take_completion();
    const auto snapshot = bridge.snapshot();
    assert(completion && completion->result && snapshot);
    assert(snapshot->config.features.get(domain::FeatureId::InfiniteAp));
    const auto pending = bridge.take_config_save();
    assert(pending && pending->features.get(domain::FeatureId::InfiniteAp));
    assert(!bridge.take_config_save());
    assert(bridge.commands.push({domain::RequestId{2}, domain::GameEpoch{2},
                                 domain::SetFeature{domain::FeatureId::InfiniteAp, false}}));
    assert(runtime::commit_ui_commands(state, bridge, 0, 1, std::chrono::seconds{1}) == 1);
    assert(!bridge.take_config_save());
}

void find_bytes_boundary() {
    constexpr std::array image{std::byte{1}, std::byte{2}, std::byte{3}};
    constexpr std::array tail{std::byte{2}, std::byte{3}};
    const auto found = game::find_bytes(image, tail, image.size());
    assert(found && *found == 1);
    assert(!game::find_bytes(image, tail, 2));
}

void tech_zero_toggle() {
    game::TechZero state;
    game::set_tech_zero(state, true);
    state.done = true;
    state.pending = true;
    game::set_tech_zero(state, true);
    assert(state.done && state.pending);
    game::set_tech_zero(state, false);
    assert(!state.enabled && !state.done && !state.pending);
    game::set_tech_zero(state, true);
    assert(state.enabled && !state.done && !state.pending);
}

void battle_ready_refresh() {
    runtime::Runtime state;
    runtime::on_battle_ready(state, 0, 0, 1);
    runtime::on_battle_ready(state, 0, 1, 0);
    assert(!state.battle_mods_refreshed);
    runtime::on_battle_ready(state, 0, 1, 1);
    assert(state.battle_mods_refreshed);
    runtime::on_battle_end(state);
    assert(!state.battle_mods_refreshed);
    runtime::on_battle_ready(state, 0, 30, 1);
    assert(state.battle_mods_refreshed);
    runtime::change_game_session(state);
    assert(!state.battle_mods_refreshed);
}

void battle_stabilization() {
    game::BattleReadiness state;
    game::BattleObservation observation{0x1000, 0x2000, 1, 3};
    assert(!game::update_battle_readiness(state, observation, true, 100));
    assert(!game::update_battle_readiness(state, observation, true, 1099));
    assert(game::update_battle_readiness(state, observation, true, 1100));
    observation.unit_count = 4;
    assert(game::update_battle_readiness(state, observation, true, 1200));
    observation.day_base = 0x3000;
    assert(!game::update_battle_readiness(state, observation, true, 1300));
    assert(!game::update_battle_readiness(state, observation, false, 2400));
}

void battle_mode() {
    game::BattleState state;
    assert(game::classify_battle_state(state, true, 5, std::nullopt, 100) == game::BattleMode::Unknown);
    assert(game::classify_battle_state(state, true, 5, std::nullopt, 699) == game::BattleMode::Unknown);
    assert(game::classify_battle_state(state, true, 5, std::nullopt, 700) == game::BattleMode::NonBattle);
    assert(state.game_state == 5 && !state.native_present);
    assert(game::classify_battle_state(state, true, 5, game::NativeBattleState{3, 0}, 800) == game::BattleMode::Battle);
    assert(state.native_present);
    assert(game::classify_battle_state(state, true, 5, game::NativeBattleState{19, 0}, 900) == game::BattleMode::NonBattle);
    assert(game::classify_battle_state(state, false, 5, std::nullopt, 1000) == game::BattleMode::Unknown);
}

void config_change_keys() {
    domain::ConfigSnapshot snapshot;
    snapshot.features.set(domain::FeatureId::InfiniteAp, true);
    snapshot.stratagem5_settings.range = 9;
    const auto changes = services::config_changes(snapshot);
    bool found_ap = false;
    bool found_range = false;
    for (const auto& change : changes) {
        if (change.key == "bInfiniteAP") {
            found_ap = std::get<bool>(change.value);
        }
        if (change.key == "iStratagem5Range") {
            found_range = std::get<int>(change.value) == 9;
        }
        assert(change.key != "bRulerTransferProposal");
    }
    assert(found_ap && found_range);
}

void config_json_update() {
    constexpr std::array changes{
        services::ConfigChange{"known", true}, services::ConfigChange{"added", 42}};
    const std::string source = R"({"unknown":{"num":12345678901234567890,"str":"\uac00"},"known":false})";
    const auto updated = services::update_config_json(source, changes);
    assert(updated);
    assert(updated->find(R"("unknown":{"num":12345678901234567890,"str":"\uac00"})") != std::string::npos);
    assert(updated->find(R"("known":true)") != std::string::npos);
    assert(updated->find(R"("added": 42)") != std::string::npos);
    assert(!services::update_config_json(R"({"known":false,"known":true})", changes));
    assert(!services::update_config_json(R"({"known":})", changes));
    assert(!services::update_config_json("[]", changes));
    assert(services::update_config_json("", changes));
    const auto lines = services::config_lines_for_load(R"({"bInfiniteAP":false,"bFastJewel":true,"iStratagem5Power1":-40})");
    assert(lines);
    domain::FeatureFlags flags;
    domain::load_feature_flags(flags, *lines);
    assert(!flags.get(domain::FeatureId::InfiniteAp) && flags.get(domain::FeatureId::FastJewel));
    assert(game::read_legacy_int(*lines, "\"iStratagem5Power1\"") == -40);
    assert(!services::config_lines_for_load(R"({"key":1,"key":2})"));
}

void terrain_bonus_config_and_modes() {
    runtime::Runtime state;
    state.epoch = domain::GameEpoch{4};
    services::CommandMailbox mailbox;
    assert(state.terrain_bonus.values[11][16] == 10);
    assert(state.terrain_bonus.values[11][17] == 0);
    auto table = state.terrain_bonus;
    table.values[1][3] = -17;
    assert(mailbox.push({domain::RequestId{1}, state.epoch, domain::SetTerrainBonus{table}}));
    assert(mailbox.push({domain::RequestId{2}, state.epoch,
                         domain::SetFeature{domain::FeatureId::TerrainAbilityAtkDef, true}}));
    assert(mailbox.push({domain::RequestId{3}, state.epoch,
                         domain::SetFeature{domain::FeatureId::TerrainAbilityAll, true}}));
    assert(runtime::commit_commands(state, mailbox, 0, 3, std::chrono::seconds{1}) == 3);
    for (int i = 0; i < 3; ++i) {
        const auto completion = mailbox.take_completion();
        assert(completion && completion->result);
    }
    assert(!state.features.get(domain::FeatureId::TerrainAbilityAtkDef));
    assert(state.features.get(domain::FeatureId::TerrainAbilityAll));
    const auto changes = services::config_changes(runtime::config_snapshot(state));
    const auto json = services::update_config_json(R"({"unknown":42,"TB_1_3":10})", changes);
    assert(json && json->find(R"("TB_1_3":-17)") != std::string::npos);
    assert(json->find(R"("unknown":42)") != std::string::npos);
    assert(json->find(R"("TB_12_17": 10)") != std::string::npos);
}

void siege_modes_and_battle_identity() {
    runtime::Runtime state;
    state.epoch = domain::GameEpoch{6};
    services::CommandMailbox mailbox;
    assert(mailbox.push({domain::RequestId{1}, state.epoch,
                         domain::SetFeature{domain::FeatureId::SiegeWarfare, true}}));
    assert(mailbox.push({domain::RequestId{2}, state.epoch,
                         domain::SetFeature{domain::FeatureId::SiegeWarfare2, true}}));
    assert(mailbox.push({domain::RequestId{3}, state.epoch, domain::SetSiegeHealRate{25}}));
    assert(runtime::commit_commands(state, mailbox, 0, 3, std::chrono::seconds{1}) == 3);
    for (int i = 0; i < 3; ++i) {
        const auto completion = mailbox.take_completion();
        assert(completion && completion->result);
    }
    assert(!state.features.get(domain::FeatureId::SiegeWarfare));
    assert(state.features.get(domain::FeatureId::SiegeWarfare2));
    assert(runtime::config_snapshot(state).siege_heal_rate == 25);
    state.siege[1].move_applied = true;
    state.siege[1].terrain_applied = true;
    state.siege[1].previous_day = 4;
    state.battle_readiness.unit_list = 0x1000;
    state.battle_readiness.day_base = 0x2000;
    runtime::on_active_battle_observed(state, 0, 0, game::BattleObservation{0x3000, 0x4000, 1, 1}, 2000);
    assert(!state.siege[1].move_applied && !state.siege[1].terrain_applied);
    assert(state.siege[1].previous_day == -1);
    const auto changes = services::config_changes(runtime::config_snapshot(state));
    const auto json = services::update_config_json("{}", changes);
    assert(json && json->find(R"("iSiegeHealRate": 25)") != std::string::npos);
}

void tavern_phase_and_borrowing() {
    auto module = std::make_unique<std::byte[]>(0x034c8630 + sizeof(std::uintptr_t));
    std::array<std::uintptr_t, 1> first{};
    std::array<std::uintptr_t, 1> second{};
    std::vector<std::byte> cities(51 * 0x2a0);
    std::array<std::byte, 0x100> game_base{};
    std::array<std::byte, 2> names{};
    std::array<std::byte, 2> data{};
    const auto city_base = reinterpret_cast<std::uintptr_t>(cities.data());
    first[0] = reinterpret_cast<std::uintptr_t>(second.data());
    second[0] = city_base;
    const auto root = reinterpret_cast<std::uintptr_t>(first.data());
    std::memcpy(module.get() + 0x034c8630, &root, sizeof(root));
    const auto slot = [&](int city, int index) {
        return cities.data() + city * 0x2a0 + 0x178 + index * 0x28;
    };
    const game::TavernRequest native{reinterpret_cast<std::uintptr_t>(&names[0]),
                                      reinterpret_cast<std::uintptr_t>(&data[0])};
    const std::array<std::uintptr_t, 2> pair{native.name, native.data};
    std::memcpy(slot(0, 0), pair.data(), sizeof(pair));
    game::TavernRequests state;
    game_base[0xd0] = std::byte{5};
    const auto exe = reinterpret_cast<std::uintptr_t>(module.get());
    const auto base = reinterpret_cast<std::uintptr_t>(game_base.data());
    game::update_tavern_requests(state, exe, base, 1000, true);
    assert(state.last_phase == 5 && !state.waiting);
    game_base[0xd0] = std::byte{7};
    game::update_tavern_requests(state, exe, base, 1500, true);
    assert(state.waiting);
    game::update_tavern_requests(state, exe, base, 3500, true);
    assert(state.waiting);
    game::update_tavern_requests(state, exe, base, 4000, true);
    assert(!state.waiting && state.backups[1][0].borrowed);
    assert(state.backups[1][0].source_city == 0 && state.injected[1][0] == native);
    game::TavernRequest borrowed{};
    std::memcpy(&borrowed.name, slot(1, 0), sizeof(borrowed.name));
    std::memcpy(&borrowed.data, slot(1, 0) + 8, sizeof(borrowed.data));
    assert(borrowed == native);
    game::update_tavern_requests(state, exe, base, 4500, false);
    assert(!state.enabled && state.city_base == 0);
    std::memcpy(&borrowed.name, slot(1, 0), sizeof(borrowed.name));
    assert(borrowed.name == native.name);
}

void specialty_slot_preserves_other_bytes() {
    std::array<std::byte, 0x14> bytes{};
    bytes[4] = std::byte{0x4a};
    bytes[5] = std::byte{0x5b};
    bytes[6] = std::byte{0x6c};
    bytes[7] = std::byte{0x7d};
    const auto slot = reinterpret_cast<std::uintptr_t>(bytes.data() + 8);
    constexpr std::uintptr_t OBJECT = 0x12345678;
    assert(platform::windows::write_specialty_slot(slot, OBJECT));
    assert(bytes[4] == std::byte{0x4a} && bytes[5] == std::byte{0x5b});
    assert(bytes[6] == std::byte{0x6c} && bytes[7] == std::byte{0x7d});
    std::uint32_t enabled = 0, bought = 1;
    std::uintptr_t object = 0;
    std::memcpy(&enabled, bytes.data(), sizeof(enabled));
    std::memcpy(&object, bytes.data() + 8, sizeof(object));
    std::memcpy(&bought, bytes.data() + 16, sizeof(bought));
    assert(enabled == 1 && object == OBJECT && bought == 0);
}

void battle_map_session_change_discards_old_addresses() {
    runtime::Runtime state;
    state.battle_map_shuffle.records.push_back({0x1234, 0x5678});
    state.battle_map_shuffle.pool.push_back(0x5678);
    state.battle_map_shuffle.active = true;
    state.battle_map_shuffle.applied_this_council = true;
    state.battle_map_shuffle.last_council = true;
    state.last_relevant_phase = 5;
    runtime::change_game_session(state);
    assert(state.battle_map_shuffle.records.empty());
    assert(state.battle_map_shuffle.pool.empty());
    assert(!state.battle_map_shuffle.active && !state.battle_map_shuffle.applied_this_council);
    assert(!state.battle_map_shuffle.last_council && state.last_relevant_phase == 0);
}

void battle_map_shuffle_and_restore() {
    auto module = std::make_unique<std::byte[]>(0x034c8630 + sizeof(std::uintptr_t));
    std::array<std::byte, 0x20> p1{}, p2{}, p3{}, p4{};
    std::vector<std::byte> map(0x8400);
    const auto store = [](std::byte* target, std::uintptr_t value) {
        std::memcpy(target, &value, sizeof(value));
    };
    store(module.get() + 0x034c8630, reinterpret_cast<std::uintptr_t>(p1.data()));
    store(p1.data(), reinterpret_cast<std::uintptr_t>(p2.data()));
    store(p2.data() + 8, reinterpret_cast<std::uintptr_t>(p3.data()));
    store(p3.data() + 0x10, reinterpret_cast<std::uintptr_t>(p4.data()));
    store(p4.data(), reinterpret_cast<std::uintptr_t>(map.data()));
    for (std::size_t i = 0; i < game::BATTLE_MAP_OFFSETS.size(); ++i) {
        const std::uint64_t value = 100 + i % 3;
        std::memcpy(map.data() + game::BATTLE_MAP_OFFSETS[i], &value, sizeof(value));
    }
    game::BattleMapShuffle state;
    std::mt19937 random{42};
    const auto exe = reinterpret_cast<std::uintptr_t>(module.get());
    game::update_battle_map_shuffle(state, exe, 3000, true, true, random);
    assert(state.active && state.applied_this_council);
    assert(state.records.size() == game::BATTLE_MAP_OFFSETS.size() && state.pool.size() == 3);
    for (const auto offset : game::BATTLE_MAP_OFFSETS) {
        std::uint64_t value = 0;
        std::memcpy(&value, map.data() + offset, sizeof(value));
        assert(value >= 100 && value <= 102);
    }
    game::update_battle_map_shuffle(state, exe, 3500, true, false, random);
    assert(!state.active && !state.applied_this_council);
    for (std::size_t i = 0; i < game::BATTLE_MAP_OFFSETS.size(); ++i) {
        std::uint64_t value = 0;
        std::memcpy(&value, map.data() + game::BATTLE_MAP_OFFSETS[i], sizeof(value));
        assert(value == 100 + i % 3);
    }
}

void faction_lord_bonus_cleanup() {
    auto module = std::make_unique<std::byte[]>(0x034c8630 + sizeof(std::uintptr_t));
    auto root_data = std::make_unique<std::byte[]>(0x197a774 + 48);
    std::array<std::byte, 0x20> p1{}, p2{}, p3{}, p4{};
    const auto store = [](std::byte* target, std::uintptr_t value) {
        std::memcpy(target, &value, sizeof(value));
    };
    store(module.get() + 0x034c8630, reinterpret_cast<std::uintptr_t>(p1.data()));
    store(p1.data(), reinterpret_cast<std::uintptr_t>(p2.data()));
    store(p2.data() + 8, reinterpret_cast<std::uintptr_t>(p3.data()));
    store(p3.data() + 0x10, reinterpret_cast<std::uintptr_t>(p4.data()));
    const auto root = reinterpret_cast<std::uintptr_t>(root_data.get());
    store(p4.data(), root);
    std::uintptr_t own_bonus = root + 0x4ec6d8 + 24;
    std::uintptr_t external_bonus = 0x12345678;
    std::fill_n(root_data.get() + 0x197a774, 48, std::byte{0x33});
    for (std::size_t i = 0; i < 5; ++i) {
        std::fill_n(root_data.get() + 0x4ec6d8 + i * 24 + 9, 15, std::byte{0x44});
    }
    game::FactionLordBonus state;
    state.enabled = true;
    state.names_applied = true;
    state.assigned = {reinterpret_cast<std::uintptr_t>(&own_bonus),
                      reinterpret_cast<std::uintptr_t>(&external_bonus)};
    game::set_faction_lord_bonus(state, reinterpret_cast<std::uintptr_t>(module.get()), false);
    assert(!state.enabled && !state.names_applied && state.assigned.empty());
    assert(own_bonus == 0 && external_bonus == 0x12345678);
    for (std::size_t i = 0; i < 48; ++i) {
        assert(root_data[0x197a774 + i] == std::byte{0});
    }
    for (std::size_t i = 0; i < 5; ++i) {
        for (std::size_t j = 0; j < 15; ++j) {
            assert(root_data[0x4ec6d8 + i * 24 + 9 + j] == std::byte{0});
        }
    }
}

void ronin_notice_delivery() {
    runtime::Runtime state;
    services::UiBridge bridge;
    state.undiscovered_ronin.completed_modified = 12;
    assert(runtime::commit_ui_commands(state, bridge, 0, 0, std::chrono::seconds{1}) == 0);
    assert(!state.undiscovered_ronin.completed_modified);
    const auto notice = bridge.take_notice();
    assert(notice && notice->message == "12명의 미발견 무장이 재야로 변경되었습니다.");
    assert(!bridge.take_notice());
    bridge.publish_notice("동일 알림");
    bridge.publish_notice("동일 알림");
    const auto repeated = bridge.take_notice();
    assert(repeated && repeated->repeat == 2 && repeated->message == "동일 알림");
}

void jewel_open_mask_and_preference() {
    std::vector<std::byte> game_memory(0x71e0 + 34);
    const auto base = reinterpret_cast<std::uintptr_t>(game_memory.data());
    game_memory[0x71e0] = std::byte{0x5a};
    game_memory[0x71e8] = std::byte{0x01};
    game::JewelOpen state;
    assert(game::set_all_jewels_open(state, base, true));
    assert(state.preferred && game_memory[0x71e0] == std::byte{0x5a});
    assert(game_memory[0x71e8] == std::byte{0xfd});
    game_memory[0x71e8] = std::byte{0x01};
    game::update_jewel_open(state, base, 250);
    assert(game_memory[0x71e8] == std::byte{0xfd});
    assert(game::set_all_jewels_open(state, base, false));
    assert(!state.preferred && game_memory[0x71e8] == std::byte{0x01});
    assert(game_memory[0x71e0] == std::byte{0x5a});
    assert(!game::set_all_jewels_open(state, 0, true) && state.preferred);
}

void secondary_jewel_mask_roundtrip() {
    const auto all = domain::all_secondary_jewels();
    const auto hex = domain::secondary_jewel_mask_hex(all);
    assert(hex.size() == 128);
    const auto parsed = domain::parse_secondary_jewel_mask(hex);
    assert(parsed && parsed->words == all.words);
    for (const auto id : domain::SECONDARY_JEWEL_IDS) {
        assert(domain::secondary_jewel_selected(*parsed, id));
    }
    assert(!domain::parse_secondary_jewel_mask("123"));
    auto invalid = hex;
    invalid[17] = 'X';
    assert(!domain::parse_secondary_jewel_mask(invalid));
    const auto sanitized = domain::parse_secondary_jewel_mask(std::string(128, 'f'));
    assert(sanitized && sanitized->words == all.words);
    const auto legacy = services::read_secondary_jewel_config(R"({"bAllSecondaryJewels":true})");
    assert(legacy && !legacy->mask_present && legacy->mask.words == all.words);
    const auto empty_mask = domain::secondary_jewel_mask_hex({});
    const auto source = std::string(R"({"bAllSecondaryJewels":true,"secondaryJewelForceMask": ")") + empty_mask + "\"}";
    const auto explicit_mask = services::read_secondary_jewel_config(source);
    assert(explicit_mask && explicit_mask->mask_present && explicit_mask->mask.words == domain::SecondaryJewelMask{}.words);
    const auto malformed = services::read_secondary_jewel_config(R"({"bAllSecondaryJewels":true,"secondaryJewelForceMask":"bad"})");
    assert(malformed && malformed->mask_present && !malformed->valid);
    const std::array changes{services::ConfigChange{"secondaryJewelForceMask", empty_mask}};
    const auto updated = services::update_config_json(R"({"other":1})", changes);
    assert(updated && services::read_secondary_jewel_config(*updated)->mask_present);
    domain::ConfigSnapshot snapshot;
    snapshot.secondary_jewels = all;
    const auto saved_changes = services::config_changes(snapshot);
    const auto saved = services::update_config_json("{}", saved_changes);
    assert(saved);
    const auto loaded = services::read_secondary_jewel_config(*saved);
    assert(loaded && loaded->mask_present && loaded->mask.words == all.words);
}

void domestics_multiplier_config() {
    domain::ConfigSnapshot snapshot;
    snapshot.domestics.player = 2.5f;
    snapshot.domestics.force = 1.25f;
    const auto changes = services::config_changes(snapshot);
    const auto json = services::update_config_json(R"({"other":true})", changes);
    assert(json);
    const auto lines = services::config_lines_for_load(*json);
    assert(lines);
    assert(game::read_legacy_float(*lines, "\"fDomesticsPlayer\"") == 2.5f);
    assert(game::read_legacy_float(*lines, "\"fDomesticsForce\"") == 1.25f);
}

void love_hate_pattern_boundaries() {
    std::array<std::byte, 25> image{
        std::byte{0x0f},std::byte{0xb6},std::byte{0x84},std::byte{0x18},
        std::byte{0xf0},std::byte{0},std::byte{0},std::byte{0},
        std::byte{0x41},std::byte{0x80},std::byte{0x84},std::byte{0x07},
        std::byte{0x48},std::byte{0xaf},std::byte{0x06},std::byte{0},std::byte{0x64},
        std::byte{0x75},std::byte{0x02},std::byte{0x48},std::byte{0x8b},
        std::byte{0x11},std::byte{0x22},std::byte{0x33},std::byte{0xe8}};
    const auto found = game::find_love_hate_targets(image);
    assert(found.affinity == 0 && found.relationship == 8 && found.relationship_offset == 0x6af48);
    const auto short_image = game::find_love_hate_targets(std::span(image).first(24));
    assert(short_image.affinity == 0 && !short_image.relationship);
}

void resonance_partial_install_restores_before_retry() {
    std::array<std::byte, 6> site{std::byte{0xe9},std::byte{1},std::byte{2},
                                  std::byte{3},std::byte{4},std::byte{0x90}};
    game::InstantResonance state;
    state.select_address = reinterpret_cast<std::uintptr_t>(site.data());
    state.select_original = {std::byte{0x8b},std::byte{0x86},std::byte{0x20},
                             std::byte{0x03},std::byte{0},std::byte{0}};
    state.select_applied = true;
    state.locked_id = 7;
    assert(!game::set_instant_resonance(state, 1, {}, true));
    assert(!state.select_applied && !state.getter_applied && state.locked_id == 0);
    assert(site == state.select_original);
}

void config_save_coordinator() {
    services::ConfigSaveCoordinator coordinator{std::string{"old"}};
    domain::ConfigSnapshot snapshot;
    const auto first = coordinator.submit(snapshot);
    const auto job = coordinator.start_next();
    assert(job && job->save.revision == first && job->baseline == "old");
    const auto second = coordinator.submit(snapshot);
    assert(coordinator.finish({first, services::ConfigSaveStatus::Completed, 0, {}, std::string{"new"}}));
    assert(coordinator.persisted_revision() == first);
    const auto next = coordinator.start_next();
    assert(next && next->save.revision == second && next->baseline == "new");
    assert(coordinator.finish({second, services::ConfigSaveStatus::Conflict}));
    assert(coordinator.failure() && !coordinator.start_next());
    assert(!coordinator.retry_failure());
    assert(coordinator.failure());

    services::ConfigSaveCoordinator retry{std::string{"old"}};
    retry.submit(snapshot);
    const auto failed = retry.start_next();
    assert(failed && retry.finish({failed->save.revision, services::ConfigSaveStatus::Failed}));
    assert(retry.retry_failure());
    const auto retry_job = retry.start_next();
    assert(retry_job && retry_job->baseline == "old");
}

void config_save_pump() {
    services::UiBridge bridge;
    services::ConfigSavePump pump;
    domain::ConfigSnapshot snapshot;
    bridge.request_config_save(snapshot);
    assert(!pump.next(bridge));
    bridge.publish_config_baseline({std::string{"{}"}});
    auto job = pump.next(bridge);
    assert(job && job->baseline == "{}");
    pump.finish(bridge, {job->save.revision, services::ConfigSaveStatus::Conflict});
    assert(bridge.config_notice() && bridge.config_notice()->status == services::ConfigSaveStatus::Conflict);
    bridge.request_config_save(snapshot);
    assert(!pump.next(bridge));
}

} // namespace

int main() {
    normalization_and_record_roundtrip();
    apply_restore_and_failure();
    morale_healing();
    legacy_settings();
    deferred_lifecycle();
    model_plan();
    game_base_pattern();
    command_mailbox();
    command_commit();
    config_save_queue();
    ui_bridge();
    find_bytes_boundary();
    tech_zero_toggle();
    battle_ready_refresh();
    battle_stabilization();
    battle_mode();
    config_change_keys();
    config_json_update();
    terrain_bonus_config_and_modes();
    siege_modes_and_battle_identity();
    tavern_phase_and_borrowing();
    specialty_slot_preserves_other_bytes();
    battle_map_session_change_discards_old_addresses();
    battle_map_shuffle_and_restore();
    faction_lord_bonus_cleanup();
    ronin_notice_delivery();
    jewel_open_mask_and_preference();
    secondary_jewel_mask_roundtrip();
    domestics_multiplier_config();
    love_hate_pattern_boundaries();
    resonance_partial_install_restores_before_retry();
    config_save_coordinator();
    config_save_pump();
}
