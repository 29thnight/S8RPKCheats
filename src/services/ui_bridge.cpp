#include "ui_bridge.h"

#include <utility>

namespace s8::services {

void UiBridge::publish_range_images(RangeImages images) {
    std::shared_ptr<const RangeImages> view = std::make_shared<RangeImages>(std::move(images));
    {
        std::lock_guard lock(mutex);
        current_range_images.swap(view);
    }
}

std::shared_ptr<const RangeImages> UiBridge::range_images_view() {
    std::lock_guard lock(mutex);
    return current_range_images;
}

void UiBridge::publish_officer_names(domain::OfficerNames names) {
    std::shared_ptr<const domain::OfficerNames> view = std::make_shared<domain::OfficerNames>(std::move(names));
    {
        std::lock_guard lock(mutex);
        current_officer_names.swap(view);
    }
}

std::shared_ptr<const domain::OfficerNames> UiBridge::officer_names_view() {
    std::lock_guard lock(mutex);
    return current_officer_names;
}

void UiBridge::publish_trait_effects(domain::TraitEffectDefinitions definitions) {
    auto view = std::make_shared<const domain::TraitEffectDefinitions>(std::move(definitions));
    std::lock_guard lock(mutex);
    current_trait_effects = std::move(view);
}

std::shared_ptr<const domain::TraitEffectDefinitions> UiBridge::trait_effects_view() {
    std::lock_guard lock(mutex);
    return current_trait_effects;
}

void UiBridge::publish_custom_traits(domain::CustomTraits traits) {
    auto view = std::make_shared<const domain::CustomTraits>(std::move(traits));
    std::lock_guard lock(mutex);
    current_custom_traits = std::move(view);
}

std::shared_ptr<const domain::CustomTraits> UiBridge::custom_traits_view() {
    std::lock_guard lock(mutex);
    return current_custom_traits;
}

bool UiBridge::request_officer_name(OfficerNameEdit edit) {
    std::lock_guard lock(mutex);
    if (!officer_name_requests_open || pending_officer_name) {
        return false;
    }
    pending_officer_name = std::move(edit);
    return true;
}

void UiBridge::close_officer_name_requests() {
    std::lock_guard lock(mutex);
    officer_name_requests_open = false;
}

std::optional<OfficerNameEdit> UiBridge::take_officer_name() {
    std::lock_guard lock(mutex);
    auto result = std::move(pending_officer_name);
    pending_officer_name.reset();
    return result;
}

void UiBridge::publish_officer_name_result(ConfigSaveStatus status) {
    std::lock_guard lock(mutex);
    officer_name_result = status;
}

std::optional<ConfigSaveStatus> UiBridge::take_officer_name_result() {
    std::lock_guard lock(mutex);
    auto result = officer_name_result;
    officer_name_result.reset();
    return result;
}

void UiBridge::publish_officer_roster(OfficerRosterSnapshot snapshot) {
    std::shared_ptr<const OfficerRosterSnapshot> view = std::make_shared<OfficerRosterSnapshot>(std::move(snapshot));
    {
        std::lock_guard lock(mutex);
        current_officer_roster.swap(view);
    }
}

std::shared_ptr<const OfficerRosterSnapshot> UiBridge::officer_roster_view() {
    std::lock_guard lock(mutex);
    return current_officer_roster;
}

void UiBridge::publish_skill_counts(domain::SkillCounts counts) {
    std::lock_guard lock(mutex);
    skill_counts = std::move(counts);
}

std::optional<domain::SkillCounts> UiBridge::take_skill_counts() {
    std::lock_guard lock(mutex);
    auto result = std::move(skill_counts);
    skill_counts.reset();
    return result;
}

void UiBridge::publish_skill_counts_view(const domain::SkillCounts& counts) {
    std::shared_ptr<const domain::SkillCounts> view = std::make_shared<const domain::SkillCounts>(counts);
    {
        std::lock_guard lock(mutex);
        current_skill_counts.swap(view);
    }
}

std::shared_ptr<const domain::SkillCounts> UiBridge::skill_counts_view() {
    std::lock_guard lock(mutex);
    return current_skill_counts;
}

void UiBridge::request_skill_counts_save(domain::SkillCounts counts) {
    std::lock_guard lock(mutex);
    pending_skill_counts_save = std::move(counts);
}

std::optional<domain::SkillCounts> UiBridge::take_skill_counts_save() {
    std::lock_guard lock(mutex);
    auto result = std::move(pending_skill_counts_save);
    pending_skill_counts_save.reset();
    return result;
}

void UiBridge::publish_notice(std::string message) {
    std::lock_guard lock(mutex);
    if (!notices.empty() && notices.back().message == message && notices.back().repeat != UINT32_MAX) {
        ++notices.back().repeat;
        return;
    }
    if (notices.size() == 128) {
        notices.pop_front();
        ++dropped_notices;
    }
    notices.push_back({std::move(message), 1});
}

std::optional<PendingNotice> UiBridge::take_notice() {
    std::lock_guard lock(mutex);
    if (dropped_notices) {
        auto message = "알림 " + std::to_string(dropped_notices) + "개가 대기 한도를 초과해 생략되었습니다.";
        dropped_notices = 0;
        return PendingNotice{std::move(message), 1};
    }
    if (notices.empty()) {
        return std::nullopt;
    }
    auto notice = std::move(notices.front());
    notices.pop_front();
    return notice;
}

void UiBridge::publish_named_specialties(domain::NamedSpecialties named, std::uint32_t random_seed) {
    std::lock_guard lock(mutex);
    named_specialties = std::pair{std::move(named), random_seed};
}

std::optional<std::pair<domain::NamedSpecialties, std::uint32_t>> UiBridge::take_named_specialties() {
    std::lock_guard lock(mutex);
    auto result = std::move(named_specialties);
    named_specialties.reset();
    return result;
}

void UiBridge::publish(UiSnapshot snapshot) {
    std::lock_guard lock(mutex);
    current = std::move(snapshot);
}

std::optional<UiSnapshot> UiBridge::snapshot() {
    std::lock_guard lock(mutex);
    return current;
}

void UiBridge::request_config_save(domain::ConfigSnapshot snapshot) {
    std::lock_guard lock(mutex);
    pending_save = std::move(snapshot);
}

std::optional<domain::ConfigSnapshot> UiBridge::take_config_save() {
    std::lock_guard lock(mutex);
    auto snapshot = std::move(pending_save);
    pending_save.reset();
    return snapshot;
}

bool UiBridge::has_config_save() {
    std::lock_guard lock(mutex);
    return pending_save.has_value();
}

void UiBridge::publish_config_baseline(ConfigBaseline baseline) {
    std::lock_guard lock(mutex);
    pending_baseline = std::move(baseline);
}

void UiBridge::publish_config_load(ConfigLoad config) {
    std::lock_guard lock(mutex);
    pending_load = std::move(config);
}

std::optional<ConfigLoad> UiBridge::take_config_load() {
    std::lock_guard lock(mutex);
    auto result = std::move(pending_load);
    pending_load.reset();
    return result;
}

std::optional<ConfigBaseline> UiBridge::take_config_baseline() {
    std::lock_guard lock(mutex);
    auto baseline = std::move(pending_baseline);
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
    std::lock_guard lock(mutex);
    retry_requested = true;
}

bool UiBridge::take_config_retry() {
    std::lock_guard lock(mutex);
    const bool requested = retry_requested;
    retry_requested = false;
    return requested;
}

} // namespace s8::services
