#pragma once

#include "command_mailbox.h"
#include "../domain/config_snapshot.h"
#include "../domain/named_specialties.h"
#include "../domain/skill_counts.h"
#include "../domain/officer_roster.h"
#include "config_save.h"
#include "config_json.h"

#include <mutex>
#include <memory>
#include <optional>
#include <atomic>
#include <utility>
#include <deque>
#include <string>
#include <vector>
#include <cstddef>

namespace s8::services {

struct UiSnapshot {
    domain::GameEpoch epoch;
    domain::ConfigSnapshot config;
};

struct ConfigBaseline {
    std::optional<std::string> contents;
};

struct ConfigLoad {
    std::optional<std::string> contents;
    domain::SecondaryJewelMask secondary_jewels;
    TerrainBonusChanges terrain_bonus;
    domain::FeatureFlagChanges features;
    DefBuildingChanges def_building;
    ScalarConfigChanges scalars;
};

struct ConfigSaveNotice {
    std::uint64_t revision;
    ConfigSaveStatus status;
    std::uint32_t native_code;
};

struct PendingNotice {
    std::string message;
    std::uint32_t repeat = 1;
};

struct OfficerRosterSnapshot {
    domain::GameEpoch epoch;
    domain::OfficerRoster entries;
    bool loading = false;
};

struct RangeImage {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<std::byte> pixels;
};
using RangeImages = std::array<RangeImage, 11>;
struct OfficerNameEdit {
    int id;
    std::string name;
};

class UiBridge {
public:
    CommandMailbox commands;
    std::atomic<bool> ui_rendered{false};
    std::atomic<bool> officer_detail_visible{false};
    std::atomic<int> officer_roster_scan_cursor{-1};
    void publish_named_specialties(domain::NamedSpecialties named, std::uint32_t random_seed);
    std::optional<std::pair<domain::NamedSpecialties, std::uint32_t>> take_named_specialties();
    void publish_skill_counts(domain::SkillCounts counts);
    std::optional<domain::SkillCounts> take_skill_counts();
    void publish_skill_counts_view(const domain::SkillCounts& counts);
    std::shared_ptr<const domain::SkillCounts> skill_counts_view();
    void publish_officer_roster(OfficerRosterSnapshot snapshot);
    std::shared_ptr<const OfficerRosterSnapshot> officer_roster_view();
    void publish_officer_names(domain::OfficerNames names);
    std::shared_ptr<const domain::OfficerNames> officer_names_view();
    void publish_trait_effects(domain::TraitEffectDefinitions definitions);
    std::shared_ptr<const domain::TraitEffectDefinitions> trait_effects_view();
    void publish_custom_traits(domain::CustomTraits traits);
    std::shared_ptr<const domain::CustomTraits> custom_traits_view();
    bool request_officer_name(OfficerNameEdit edit);
    void close_officer_name_requests();
    std::optional<OfficerNameEdit> take_officer_name();
    void publish_officer_name_result(ConfigSaveStatus status);
    std::optional<ConfigSaveStatus> take_officer_name_result();
    void publish_range_images(RangeImages images);
    std::shared_ptr<const RangeImages> range_images_view();
    void request_skill_counts_save(domain::SkillCounts counts);
    std::optional<domain::SkillCounts> take_skill_counts_save();
    void publish_notice(std::string message);
    std::optional<PendingNotice> take_notice();

    void publish(UiSnapshot snapshot);
    std::optional<UiSnapshot> snapshot();
    void request_config_save(domain::ConfigSnapshot snapshot);
    std::optional<domain::ConfigSnapshot> take_config_save();
    bool has_config_save();
    void publish_config_baseline(ConfigBaseline baseline);
    std::optional<ConfigBaseline> take_config_baseline();
    void publish_config_load(ConfigLoad config);
    std::optional<ConfigLoad> take_config_load();
    void publish_config_notice(ConfigSaveNotice notice);
    std::optional<ConfigSaveNotice> config_notice();
    void request_config_retry();
    bool take_config_retry();

private:
    std::mutex mutex;
    std::optional<UiSnapshot> current;
    std::optional<std::pair<domain::NamedSpecialties, std::uint32_t>> named_specialties;
    std::optional<domain::SkillCounts> skill_counts;
    std::shared_ptr<const domain::SkillCounts> current_skill_counts;
    std::shared_ptr<const OfficerRosterSnapshot> current_officer_roster;
    std::shared_ptr<const domain::OfficerNames> current_officer_names;
    std::shared_ptr<const domain::TraitEffectDefinitions> current_trait_effects;
    std::shared_ptr<const domain::CustomTraits> current_custom_traits;
    std::optional<OfficerNameEdit> pending_officer_name;
    bool officer_name_requests_open = true;
    std::optional<ConfigSaveStatus> officer_name_result;
    std::shared_ptr<const RangeImages> current_range_images;
    std::optional<domain::SkillCounts> pending_skill_counts_save;
    std::deque<PendingNotice> notices;
    std::uint64_t dropped_notices = 0;
    std::optional<domain::ConfigSnapshot> pending_save;
    std::optional<ConfigBaseline> pending_baseline;
    std::optional<ConfigLoad> pending_load;
    std::optional<ConfigSaveNotice> last_notice;
    bool retry_requested = false;
};

} // namespace s8::services
