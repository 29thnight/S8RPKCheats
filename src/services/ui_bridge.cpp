#include "ui_bridge.h"

#include <utility>

namespace s8::services {

void UiBridge::publish_range_images(RangeImages images) {
    own::shared_owner<const RangeImages> view = own::make_shared<RangeImages>(std::move(images));

    {
        std::lock_guard lock(mutex);
        current_range_images.swap(view);
    }
}

own::shared_owner<const RangeImages> UiBridge::range_images_view() {
    std::lock_guard lock(mutex);
    return current_range_images;
}

void UiBridge::publish_officer_names(domain::OfficerNames names) {
    own::shared_owner<const domain::OfficerNames> view = own::make_shared<domain::OfficerNames>(std::move(names));
    own::shared_owner<const domain::OfficerNames> overrides = view;

    {
        const std::lock_guard<std::mutex> lock(mutex);

        current_officer_names.swap(view);
        current_officer_name_overrides.swap(overrides);
    }
    config_wake.signal();
}

own::shared_owner<const domain::OfficerNames> UiBridge::officer_names_view() {
    std::lock_guard lock(mutex);
    return current_officer_names;
}

own::shared_owner<const domain::OfficerNames> UiBridge::officer_name_overrides_view() {
    const std::lock_guard<std::mutex> lock(mutex);

    return current_officer_name_overrides;
}

bool UiBridge::publish_resolved_officer_names(domain::OfficerNames names,
    const own::shared_owner<const domain::OfficerNames>& expected_overrides) {
    own::shared_owner<const domain::OfficerNames> view = own::make_shared<domain::OfficerNames>(std::move(names));

    {
        const std::lock_guard<std::mutex> lock(mutex);

        if (own::identity(current_officer_name_overrides) != own::identity(expected_overrides)) {
            return false;
        }
        current_officer_names.swap(view);
    }
    return true;
}

void UiBridge::publish_trait_effects(domain::TraitEffectDefinitions definitions) {
    own::shared_owner<const domain::TraitEffectDefinitions> view =
        own::make_shared<domain::TraitEffectDefinitions>(std::move(definitions));

    {
        std::lock_guard lock(mutex);

        current_trait_effects.swap(view);
    }
}

own::shared_owner<const domain::TraitEffectDefinitions> UiBridge::trait_effects_view() {
    std::lock_guard lock(mutex);
    return current_trait_effects;
}

void UiBridge::publish_custom_traits(domain::CustomTraits traits) {
    own::shared_owner<const domain::CustomTraits> view =
        own::make_shared<domain::CustomTraits>(std::move(traits));

    {
        std::lock_guard lock(mutex);

        current_custom_traits.swap(view);
    }
}

own::shared_owner<const domain::CustomTraits> UiBridge::custom_traits_view() {
    std::lock_guard lock(mutex);
    return current_custom_traits;
}

void UiBridge::publish_trait_catalog(own::shared_owner<const domain::TraitRuntimeCatalog> catalog) {
    {
        std::lock_guard lock(mutex);

        pending_trait_catalog.swap(catalog);
    }
}

own::shared_owner<const domain::TraitRuntimeCatalog> UiBridge::take_trait_catalog() {
    own::shared_owner<const domain::TraitRuntimeCatalog> catalog;

    {
        std::lock_guard lock(mutex);

        pending_trait_catalog.swap(catalog);
    }
    return catalog;
}

void UiBridge::publish_legacy_trait_texts(own::shared_owner<const domain::TraitTextEdits> rows) {
    own::shared_owner<const domain::TraitTextEdits> pending = rows;

    {
        std::lock_guard lock(mutex);

        current_legacy_trait_texts.swap(rows);
        pending_legacy_trait_texts.swap(pending);
    }
}

own::shared_owner<const domain::TraitTextEdits> UiBridge::legacy_trait_texts_view() {
    std::lock_guard lock(mutex);

    return current_legacy_trait_texts;
}

own::shared_owner<const domain::TraitTextEdits> UiBridge::take_legacy_trait_texts() {
    own::shared_owner<const domain::TraitTextEdits> rows;

    {
        std::lock_guard lock(mutex);

        pending_legacy_trait_texts.swap(rows);
    }
    return rows;
}

bool UiBridge::request_trait_text_save(TraitTextSaveRequest request) {
    {
        std::lock_guard lock(mutex);

        if (!officer_name_requests_open || !request.rows || pending_trait_text_save) {
            return false;
        }
        pending_trait_text_save = std::move(request);
    }
    config_wake.signal();
    return true;
}

std::optional<TraitTextSaveRequest> UiBridge::take_trait_text_save() {
    std::optional<TraitTextSaveRequest> request;

    {
        std::lock_guard lock(mutex);

        pending_trait_text_save.swap(request);
    }
    return request;
}

void UiBridge::publish_trait_text_save_result(ConfigSaveStatus result) {
    std::lock_guard lock(mutex);

    trait_text_save_result = result;
}

std::optional<ConfigSaveStatus> UiBridge::take_trait_text_save_result() {
    std::lock_guard lock(mutex);

    return std::exchange(trait_text_save_result, std::nullopt);
}

void UiBridge::request_trait_text_reload() {
    {
        std::lock_guard lock(mutex);

        trait_text_reload = true;
    }
    config_wake.signal();
}

bool UiBridge::take_trait_text_reload() {
    std::lock_guard lock(mutex);

    return std::exchange(trait_text_reload, false);
}

bool UiBridge::request_officer_name(OfficerNameEdit edit) {
    {
        std::lock_guard lock(mutex);

        if (!officer_name_requests_open || pending_officer_name) {
            return false;
        }
        pending_officer_name = std::move(edit);
    }
    config_wake.signal();
    return true;
}

void UiBridge::close_officer_name_requests() {
    std::lock_guard lock(mutex);
    officer_name_requests_open = false;
}

std::optional<OfficerNameEdit> UiBridge::take_officer_name() {
    std::lock_guard lock(mutex);
    std::optional<OfficerNameEdit> result = std::move(pending_officer_name);

    pending_officer_name.reset();
    return result;
}

void UiBridge::publish_officer_name_result(ConfigSaveStatus status) {
    std::lock_guard lock(mutex);
    officer_name_result = status;
}

std::optional<ConfigSaveStatus> UiBridge::take_officer_name_result() {
    std::lock_guard lock(mutex);
    const std::optional<ConfigSaveStatus> result = officer_name_result;

    officer_name_result.reset();
    return result;
}

void UiBridge::publish_officer_roster(OfficerRosterSnapshot snapshot) {
    own::shared_owner<const OfficerRosterSnapshot> view = own::make_shared<OfficerRosterSnapshot>(std::move(snapshot));

    {
        std::lock_guard lock(mutex);
        current_officer_roster.swap(view);
    }
}

own::shared_owner<const OfficerRosterSnapshot> UiBridge::officer_roster_view() {
    std::lock_guard lock(mutex);
    return current_officer_roster;
}

void UiBridge::publish_skill_counts(domain::SkillCounts counts) {
    std::lock_guard lock(mutex);
    skill_counts = std::move(counts);
}

std::optional<domain::SkillCounts> UiBridge::take_skill_counts() {
    std::lock_guard lock(mutex);
    std::optional<domain::SkillCounts> result = std::move(skill_counts);

    skill_counts.reset();
    return result;
}

void UiBridge::publish_skill_counts_view(const domain::SkillCounts& counts) {
    own::shared_owner<const domain::SkillCounts> view = own::make_shared<domain::SkillCounts>(counts);

    {
        std::lock_guard lock(mutex);
        current_skill_counts.swap(view);
    }
}

own::shared_owner<const domain::SkillCounts> UiBridge::skill_counts_view() {
    std::lock_guard lock(mutex);
    return current_skill_counts;
}

void UiBridge::request_skill_counts_save(domain::SkillCounts counts) {
    std::optional<domain::SkillCounts> incoming{std::move(counts)};

    {
        std::lock_guard lock(mutex);

        pending_skill_counts_save.swap(incoming);
    }
    config_wake.signal();
}

std::optional<domain::SkillCounts> UiBridge::take_skill_counts_save() {
    std::lock_guard lock(mutex);
    std::optional<domain::SkillCounts> result = std::move(pending_skill_counts_save);

    pending_skill_counts_save.reset();
    return result;
}

void UiBridge::publish_notice(std::string message) {
    file_log.push(message);
    PendingNotice incoming{std::move(message), 1};

    {
        std::lock_guard lock(mutex);

        if (notice_count != 0) {
            PendingNotice& previous = notices[(notice_head + notice_count - 1) % notice_capacity];

            if (previous.message == incoming.message && previous.repeat != UINT32_MAX) {
                ++previous.repeat;
                return;
            }
        }

        if (notice_count == notice_capacity) {
            std::swap(notices[notice_head], incoming);
            notice_head = (notice_head + 1) % notice_capacity;
            ++dropped_notices;
        } else {
            std::swap(notices[(notice_head + notice_count) % notice_capacity], incoming);
            ++notice_count;
        }
    }

    // The overwritten string is released after leaving the producer lock.
}

std::optional<PendingNotice> UiBridge::take_notice() {
    std::uint64_t dropped = 0;
    std::optional<PendingNotice> result;

    {
        std::lock_guard lock(mutex);

        dropped = std::exchange(dropped_notices, 0);
        if (dropped == 0 && notice_count != 0) {
            result.emplace(std::move(notices[notice_head]));
            notice_head = (notice_head + 1) % notice_capacity;
            --notice_count;
        }
    }

    if (dropped != 0) {
        result.emplace("알림 " + std::to_string(dropped) + "개가 대기 한도를 초과해 생략되었습니다.", 1);
    }

    return result;
}

void UiBridge::publish_named_specialties(domain::NamedSpecialties named, std::uint32_t random_seed) {
    std::lock_guard lock(mutex);
    named_specialties = std::pair{std::move(named), random_seed};
}

std::optional<std::pair<domain::NamedSpecialties, std::uint32_t>> UiBridge::take_named_specialties() {
    std::lock_guard lock(mutex);
    std::optional<std::pair<domain::NamedSpecialties, std::uint32_t>> result = std::move(named_specialties);

    named_specialties.reset();
    return result;
}

void UiBridge::publish(UiSnapshot snapshot) {
    file_log.set_enabled(snapshot.config.features.get(domain::FeatureId::FileLog));
    std::optional<UiSnapshot> incoming{std::move(snapshot)};

    {
        std::lock_guard lock(mutex);

        current.swap(incoming);
    }
}

std::optional<UiSnapshot> UiBridge::snapshot() {
    std::lock_guard lock(mutex);
    return current;
}

void UiBridge::request_config_save(domain::ConfigSnapshot snapshot) {
    std::optional<domain::ConfigSnapshot> incoming{std::move(snapshot)};

    {
        std::lock_guard lock(mutex);

        pending_save.swap(incoming);
    }
    config_wake.signal();
}

std::optional<domain::ConfigSnapshot> UiBridge::take_config_save() {
    std::lock_guard lock(mutex);
    std::optional<domain::ConfigSnapshot> snapshot = std::move(pending_save);

    pending_save.reset();
    return snapshot;
}

bool UiBridge::has_config_save() {
    std::lock_guard lock(mutex);
    return pending_save.has_value();
}

void UiBridge::publish_config_baseline(ConfigBaseline baseline) {
    std::optional<ConfigBaseline> incoming{std::move(baseline)};

    {
        std::lock_guard lock(mutex);

        pending_baseline.swap(incoming);
    }
    config_wake.signal();
}

void UiBridge::publish_config_load(ConfigLoad config) {
    std::lock_guard lock(mutex);
    pending_load = std::move(config);
}

std::optional<ConfigLoad> UiBridge::take_config_load() {
    std::lock_guard lock(mutex);
    std::optional<ConfigLoad> result = std::move(pending_load);

    pending_load.reset();
    return result;
}

std::optional<ConfigBaseline> UiBridge::take_config_baseline() {
    std::lock_guard lock(mutex);
    std::optional<ConfigBaseline> baseline = std::move(pending_baseline);

    pending_baseline.reset();
    return baseline;
}

void UiBridge::publish_config_notice(ConfigSaveNotice notice) {
    std::lock_guard lock(mutex);
    last_notice = notice;
}

std::optional<ConfigSaveNotice> UiBridge::config_notice() {
    std::lock_guard lock(mutex);
    return last_notice;
}

void UiBridge::request_config_retry() {
    {
        std::lock_guard lock(mutex);

        retry_requested = true;
    }
    config_wake.signal();
}

bool UiBridge::take_config_retry() {
    std::lock_guard lock(mutex);
    const bool requested = retry_requested;
    retry_requested = false;
    return requested;
}

} // namespace s8::services
