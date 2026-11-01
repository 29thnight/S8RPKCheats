#include "sample_session.h"

#include <algorithm>
#include <type_traits>

namespace s8::preview {
namespace {
domain::Result<void> unsupported() {
    return std::unexpected(domain::Error{domain::ErrorCode::UnsupportedVersion});
}
}

SampleSession::SampleSession(std::string_view scenario) {
    config.config_loaded = scenario != "loading";
    config.features.values.fill(false);
    config.features.set(domain::FeatureId::ShowWidgetNotif, true);
    config.features.set(domain::FeatureId::InfiniteAp, true);
    config.features.set(domain::FeatureId::AttitudeHack, true);
    config.features.set(domain::FeatureId::Loyalty, true);
    config.features.set(domain::FeatureId::TraitViewer, true);
    config.features.set(domain::FeatureId::IsolatedTerritoryMovement, true);
    config.features.set(domain::FeatureId::AiWarImprove, true);
    config.mission_appointment_limit = 10;
    config.mission_appointment_limit_managed = true;
    config.game_memory_status = scenario == "loading" ? domain::GameMemoryStatus::Searching
        : scenario == "title" ? domain::GameMemoryStatus::WaitingForPlayer : domain::GameMemoryStatus::Ready;
    config.game_memory_scan_cursor = 12 * 1024 * 1024;
    config.game_memory_image_size = 36 * 1024 * 1024;
    config.game_memory_scan_passes = 1;
    config.secondary_jewels = domain::SecondaryJewelMask{};
    config.jewel_bitmap = std::array<std::uint8_t, 34>{};

    names = {{1, "조조"}, {2, "유비"}, {3, "관우"}, {4, "장비"}, {5, "제갈량"},
        {6, "손권"}, {7, "주유"}, {8, "손상향"}, {9, "조운"}, {10, "사마의"},
        {11, "여포"}, {12, "초선"}, {13, "곽가"}, {14, "육손"}, {15, "마초"}, {16, "황충"}};
    for (const auto& [id, name] : names) {
        domain::OfficerDetail officer{};
        officer.id = static_cast<std::uint16_t>(id);
        officer.status = (id == 1 || id == 2 || id == 6) ? 0xc8
            : (id == 5 || id == 7 || id == 10) ? 0x18 : 0x28;
        officer.gender = (id == 8 || id == 12) ? 1 : 0;
        officer.voice = static_cast<std::uint8_t>(id % 10);
        officer.strategic_tendency = 1;
        officer.personality = 1;
        officer.greed = 1;
        officer.priority = 1;
        officer.city = static_cast<std::uint8_t>(id % domain::CITY_COUNT);
        officer.basic = {90, 95, 88, 87, 92, 3650, 3270, 0, 500, 29017, 100, 120, 90, 180, 100, 100, 0, 0, 0};
        officer.tactic_levels.fill(2);
        officer.skill_levels.fill(2);
        officer.aptitudes.fill(75);
        officer.family = {0, 0, 0};
        officer.traits[0] = domain::Trait{static_cast<std::uint16_t>(id % 20), {}};
        officer.traits[1] = domain::Trait{static_cast<std::uint16_t>((id + 7) % 20), {}};
        officers.emplace(officer.id, officer);
    }
    bridge.publish_officer_names(names);
    bridge.publish_custom_traits({{201, {"미리보기", 1}}});
    bridge.publish_trait_effects({{1, {"공격력 증가", {{10, "10"}}, {}}}});
    bridge.publish_skill_counts_view({});
    config.traits.legacy_rows = own::make_shared<domain::TraitTextEdits>(domain::make_legacy_trait_text_rows());
    config.traits.embedded_rows = own::make_shared<domain::TraitTextEdits>(domain::TraitTextEdits{
        {201, true, "미리보기", "확장 기재의 편집 화면을 확인하는 샘플입니다.",
            "미리보기", "확장 기재의 편집 화면을 확인하는 샘플입니다."}});
    config.traits.source_generation = 1;
    config.traits.viewer = scenario == "loading" ? domain::TraitApplyStatus::Pending
        : domain::TraitApplyStatus::Applied;
    config.traits.viewer_applied = scenario != "loading";
    config.traits.catalog = scenario == "title" ? domain::TraitApplyStatus::WaitingForTable
        : domain::TraitApplyStatus::Applied;
    config.traits.legacy_text = domain::TraitApplyStatus::Applied;
    if (scenario == "external") config.traits.catalog = domain::TraitApplyStatus::ExternalRuntime;

    for (std::size_t i = 0; i < config.features.values.size(); ++i) {
        const auto id = static_cast<domain::FeatureId>(i);
        if (!config.features.get(id)) continue;
        if (const auto group = domain::native_feature_group(id)) {
            config.native_features[static_cast<std::size_t>(*group)] = {
                domain::NativeFeaturePhase::Applied, domain::NativeFeatureError::None, true};
        }
        if (const auto group = domain::legacy_feature_group(id)) {
            const bool waiting = scenario == "title";
            config.legacy_features[static_cast<std::size_t>(*group)] = {
                waiting ? domain::LegacyFeaturePhase::WaitingForPlayer : domain::LegacyFeaturePhase::Applied,
                !waiting};
        }
    }
    if (scenario == "errors") {
        config.native_features[0] = {domain::NativeFeaturePhase::Failed,
            domain::NativeFeatureError::SignatureMismatch, false};
        config.native_features[1] = {domain::NativeFeaturePhase::Pending,
            domain::NativeFeatureError::Busy, false};
        config.legacy_features[0] = {domain::LegacyFeaturePhase::Retrying, false};
        config.traits.catalog = domain::TraitApplyStatus::MemoryFailed;
    }

    if (scenario != "title" && scenario != "loading") {
        const auto& hero = officers.at(8);
        config.protagonist_id = hero.id;
        config.protagonist_basic = hero.basic;
        config.protagonist_gender = hero.gender;
        config.protagonist_tactic_levels = hero.tactic_levels;
        config.protagonist_skill_levels = hero.skill_levels;
        config.protagonist_aptitudes = hero.aptitudes;
        config.protagonist_traits = hero.traits;
        config.tengi_layout_supported = config.tengi_ready = true;
    }

    cities.epoch = epoch;
    cities.available = scenario != "loading";
    cities.revision = 1;
    cities.year = 212;
    cities.month = 7;
    cities.routes = own::make_shared<domain::CitySupportRoutes>();
    for (std::size_t id = 0; id < cities.cities.size(); ++id) {
        auto& row = cities.cities[id];
        row.valid = cities.available;
        row.occupied = config.protagonist_id.has_value();
        row.force_lord = row.occupied ? 6 : domain::NO_CITY_OFFICER;
        row.governor = row.occupied ? static_cast<std::uint16_t>((id % 16) + 1) : domain::NO_CITY_OFFICER;
        row.values = {29017, 182243, 61765, 30000, 30000, 30000, 30000, 100000, 0};
    }
    bridge.city_management.publish(cities);
    if (scenario != "loading") {
        publish_roster();
        publish_selected(8);
    }
    bridge.publish({epoch, config});
}

void SampleSession::publish_roster() {
    domain::OfficerRosterUpdate update{};
    update.epoch = epoch;
    update.generation = ++roster_generation;
    update.available = true;
    update.loading = true;
    update.kind = domain::OfficerRosterUpdateKind::Reset;
    bridge.publish_officer_roster(update);
    update.kind = domain::OfficerRosterUpdateKind::Rows;
    for (const auto& [id, officer] : officers) update.entries[update.count++] = {id, officer.status};
    bridge.publish_officer_roster(update);
    update.kind = domain::OfficerRosterUpdateKind::Complete;
    update.count = 0;
    update.loading = false;
    bridge.publish_officer_roster(update);
}

void SampleSession::publish_selected(std::uint16_t id) {
    if (const auto it = officers.find(id); it != officers.end()) {
        config.selected_officer = it->second;
        config.selected_faction_color = {id, 1, 2, config.protagonist_id.has_value(),
            !config.protagonist_id.has_value(), false};
    }
}

domain::Result<void> SampleSession::apply(const domain::CommandPayload& payload) {
    return std::visit([&](const auto& value) -> domain::Result<void> {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, domain::SetFeature>) {
            config.features.set(value.feature, value.enabled);
            if (const auto group = domain::native_feature_group(value.feature)) {
                config.native_features[static_cast<std::size_t>(*group)] = {
                    value.enabled ? domain::NativeFeaturePhase::Applied : domain::NativeFeaturePhase::Disabled,
                    domain::NativeFeatureError::None, value.enabled};
            }
            if (const auto group = domain::legacy_feature_group(value.feature)) {
                config.legacy_features[static_cast<std::size_t>(*group)] = {
                    value.enabled ? domain::LegacyFeaturePhase::Applied : domain::LegacyFeaturePhase::Disabled,
                    value.enabled};
            }
            if (value.feature == domain::FeatureId::TraitViewer) {
                config.traits.viewer = value.enabled ? domain::TraitApplyStatus::Applied : domain::TraitApplyStatus::Disabled;
                config.traits.viewer_applied = value.enabled;
            }
        }
        else if constexpr (std::is_same_v<T, domain::SetUiScale>) config.ui_scale = value.scale;
        else if constexpr (std::is_same_v<T, domain::SetNotificationSpeed>) config.notification_speed = value.speed;
        else if constexpr (std::is_same_v<T, domain::SetSpeedMultiplier>) config.speed_multiplier = value.multiplier;
        else if constexpr (std::is_same_v<T, domain::SetTengiSettings>) config.tengi_settings = value.settings;
        else if constexpr (std::is_same_v<T, domain::SetAiGrowthEnabled>) config.ai_growth_settings.enabled = value.enabled;
        else if constexpr (std::is_same_v<T, domain::SetAiGrowthSpeed>) config.ai_growth_settings.speed = value.speed;
        else if constexpr (std::is_same_v<T, domain::SetAnnualSpecialAbilities>) config.ai_growth_settings.annual_special_abilities = value.enabled;
        else if constexpr (std::is_same_v<T, domain::SetShortBattleCooldown>) {
            config.short_battle_cooldown_enabled = value.enabled;
            config.short_battle_cooldown_days = value.days;
        }
        else if constexpr (std::is_same_v<T, domain::SetTroopCountCombat>) config.troop_count_combat_enabled = value.enabled;
        else if constexpr (std::is_same_v<T, domain::SetReinforcementArrival>) config.reinforcement_arrival_enabled = value.enabled;
        else if constexpr (std::is_same_v<T, domain::SetTechCityVisibility>) config.tech_city_visibility_enabled = value.enabled;
        else if constexpr (std::is_same_v<T, domain::SetStratagem5Enabled>) config.stratagem5_enabled = value.enabled;
        else if constexpr (std::is_same_v<T, domain::SetStratagem5Settings>) config.stratagem5_settings = value.settings;
        else if constexpr (std::is_same_v<T, domain::SetSelfHealSettings>) config.self_heal_settings = value.settings;
        else if constexpr (std::is_same_v<T, domain::SetDongtoSettings>) config.dongto_settings = value.settings;
        else if constexpr (std::is_same_v<T, domain::SetCelestialSettings>) config.celestial_settings = value.settings;
        else if constexpr (std::is_same_v<T, domain::SetCatapultSettings>) config.catapult_settings = value.settings;
        else if constexpr (std::is_same_v<T, domain::SetTerrainTacticsSettings>) config.terrain_tactics_settings = value.settings;
        else if constexpr (std::is_same_v<T, domain::SetDefBuildingSettings>) config.def_building_settings = value.settings;
        else if constexpr (std::is_same_v<T, domain::SetSkipDays>) config.skip_days = value.days;
        else if constexpr (std::is_same_v<T, domain::SetTerrainBonus>) config.terrain_bonus = value.table;
        else if constexpr (std::is_same_v<T, domain::SetSiegeHealRate>) config.siege_heal_rate = value.rate;
        else if constexpr (std::is_same_v<T, domain::SetAllJewelsOpen>) config.all_jewels_open = value.enabled;
        else if constexpr (std::is_same_v<T, domain::SetSecondaryJewels>) config.secondary_jewels = value.mask;
        else if constexpr (std::is_same_v<T, domain::SetDomesticsMultipliers>) config.domestics = value.multipliers;
        else if constexpr (std::is_same_v<T, domain::SetMissionAppointmentLimit>) config.mission_appointment_limit = value.value;
        else if constexpr (std::is_same_v<T, domain::RestoreMissionAppointmentLimit>) config.mission_appointment_limit = 10;
        else if constexpr (std::is_same_v<T, domain::SetDomesticRewardMode>) config.domestic_reward_mode = value.mode;
        else if constexpr (std::is_same_v<T, domain::RefreshOfficerRoster>) publish_roster();
        else if constexpr (std::is_same_v<T, domain::SelectRosterOfficer>) publish_selected(value.officer_id);
        else if constexpr (std::is_same_v<T, domain::SetRosterOfficerBasic>) {
            if (!officers.contains(value.officer_id) || value.index >= domain::officer_basic_fields.size()) return unsupported();
            officers.at(value.officer_id).basic[value.index] = value.value;
            publish_selected(value.officer_id);
        }
        else if constexpr (std::is_same_v<T, domain::SetRosterOfficerGender>) {
            if (!officers.contains(value.officer_id)) return unsupported();
            officers.at(value.officer_id).gender = value.gender;
            publish_selected(value.officer_id);
        }
        else if constexpr (std::is_same_v<T, domain::SetRosterOfficerVoice>) {
            if (!officers.contains(value.officer_id)) return unsupported();
            officers.at(value.officer_id).voice = value.voice;
            publish_selected(value.officer_id);
        }
        else if constexpr (std::is_same_v<T, domain::SetRosterOfficerTrait>) {
            if (!officers.contains(value.officer_id) || value.slot >= 3) return unsupported();
            officers.at(value.officer_id).traits[value.slot] = domain::Trait{value.trait_id, {}};
            publish_selected(value.officer_id);
        }
        else if constexpr (std::is_same_v<T, domain::SetRosterOfficerTraining>) {
            if (!officers.contains(value.officer_id)) return unsupported();
            auto& officer = officers.at(value.officer_id);
            if (value.kind == domain::OfficerTrainingKind::Tactic && value.index < officer.tactic_levels.size()) officer.tactic_levels[value.index] = value.value;
            else if (value.kind == domain::OfficerTrainingKind::Skill && value.index < officer.skill_levels.size()) officer.skill_levels[value.index] = value.value;
            else if (value.kind == domain::OfficerTrainingKind::Aptitude && value.index < officer.aptitudes.size()) officer.aptitudes[value.index] = value.value;
            else return unsupported();
            publish_selected(value.officer_id);
        }
        else if constexpr (std::is_same_v<T, domain::SetProtagonistBasic>) {
            if (!config.protagonist_basic || value.index >= config.protagonist_basic->size()) return unsupported();
            (*config.protagonist_basic)[value.index] = value.value;
        }
        else if constexpr (std::is_same_v<T, domain::SetProtagonistGender>) config.protagonist_gender = value.gender;
        else if constexpr (std::is_same_v<T, domain::ApplyTraitTextEdits>) {
            if (value.embedded) config.traits.embedded_rows = value.rows;
            else config.traits.legacy_rows = value.rows;
            ++config.traits.source_generation;
        }
        else if constexpr (std::is_same_v<T, domain::SetCitySettings>) {
            config.city_settings = value.settings;
            cities.settings = value.settings;
            bridge.city_management.publish(cities);
        }
        else if constexpr (std::is_same_v<T, domain::CityRequest>) {
            if (value.action == domain::CityAction::Write && value.city < cities.cities.size()
                && static_cast<std::size_t>(value.field) < domain::CITY_FIELD_COUNT) {
                cities.cities[value.city].values[static_cast<std::size_t>(value.field)] = value.value;
            } else if (value.action != domain::CityAction::Refresh && value.action != domain::CityAction::Cancel) return unsupported();
            ++cities.revision;
            cities.last_request = domain::CityWorkStatus::Complete;
            bridge.city_management.publish(cities);
        }
        else return unsupported();
        return {};
    }, payload);
}

void SampleSession::pump() {
    if (!held_command) {
        held_command = bridge.commands.pop();
        response_wait = response_delay_frames;
    }
    if (held_command && response_wait > 0) --response_wait;
    while (held_command && response_wait == 0) {
        const auto accepted = std::move(held_command);
        held_command.reset();
        auto result = accepted->command.epoch == epoch ? apply(accepted->command.payload)
            : domain::Result<void>{std::unexpected(domain::Error{domain::ErrorCode::StaleEpoch})};
        if (result) ++accepted_commands;
        else {
            ++unsupported_commands;
            bridge.publish_notice("미리보기: 이 작업은 게임 연결이 필요하여 실행하지 않았습니다.");
        }
        bridge.commands.complete(accepted->completion, result);
        held_command = bridge.commands.pop();
        response_wait = response_delay_frames;
    }
    if (bridge.take_config_save() || bridge.take_config_retry()) {
        bridge.publish_config_notice({accepted_commands, services::ConfigSaveStatus::Completed, 0});
        bridge.publish_notice("미리보기 설정을 메모리에 반영했습니다. 파일은 저장하지 않습니다.");
    }
    if (const auto edit = bridge.take_officer_name()) {
        names[edit->id] = edit->name;
        bridge.publish_officer_names(names);
        bridge.publish_officer_name_result(services::ConfigSaveStatus::Completed);
    }
    if (const auto save = bridge.take_trait_text_save()) {
        if (save->embedded) config.traits.embedded_rows = save->rows;
        else config.traits.legacy_rows = save->rows;
        ++config.traits.source_generation;
        bridge.publish_trait_text_save_result(services::ConfigSaveStatus::Completed);
        bridge.publish_notice("미리보기: 기재 텍스트는 현재 창에서만 유지됩니다.");
    }
    if (bridge.take_trait_text_reload()) ++config.traits.source_generation;
    bridge.publish({epoch, config});
}

} // namespace s8::preview
