#pragma once

#include "command_mailbox.h"
#include "../domain/config_snapshot.h"
#include "../domain/named_specialties.h"
#include "../domain/skill_counts.h"
#include "../domain/officer_roster.h"
#include "../domain/officer_roster_publication.h"
#include "../domain/ownership.h"
#include "../domain/trait_runtime.h"
#include "../domain/trait_text_edit.h"
#include "../domain/city_management.h"
#include "config_save.h"
#include "config_json.h"
#include "service_wake.h"
#include "file_log.h"
#include "officer_tools_bridge.h"
#include "debug_bridge.h"
#include "city_bridge.h"
#include "corps_bridge.h"
#include "specialty_bridge.h"
#include "native_officer_name_bridge.h"

#include <mutex>
#include <array>
#include <optional>
#include <atomic>
#include <utility>
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

struct RangeImage {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<std::byte> pixels;
};
using RangeImages = std::array<RangeImage, 11>;
using UiAtlasImages = std::array<RangeImage, 5>;
struct OfficerNameEdit {
    int id;
    std::string name;
};

struct TraitTextSaveRequest {
    bool embedded = false;
    own::shared_owner<const domain::TraitTextEdits> rows;
};

class UiBridge {
public:
    CommandMailbox commands;
    ServiceWake config_wake;
    FileLogChannel file_log{config_wake};
    NativeOfficerNameBridge native_officer_names{config_wake};
    DebugBridge debug_tools;
    CityBridge city_management;
    CorpsBridge corps_deployment;
    SpecialtyBridge specialties;
    void publish_specialty_definitions(own::shared_owner<const domain::SpecialtyDefinitions> definitions);
    own::shared_owner<const domain::SpecialtyDefinitions> specialty_definitions_view();
    OfficerToolsBridge officer_tools;
    OfficerNoticesBridge officer_notices;
    std::atomic<bool> external_trait_runtime{false};
    std::atomic<bool> ui_rendered{false};
    std::atomic<bool> officer_detail_visible{false};
    std::atomic<bool> protagonist_detail_visible{false};
    std::atomic<int> officer_roster_scan_cursor{-1};
    void publish_named_specialties(domain::NamedSpecialties named, std::uint32_t random_seed);
    std::optional<std::pair<domain::NamedSpecialties, std::uint32_t>> take_named_specialties();
    void publish_skill_counts(domain::SkillCounts counts);
    std::optional<domain::SkillCounts> take_skill_counts();
    void publish_skill_counts_view(const domain::SkillCounts& counts);
    own::shared_owner<const domain::SkillCounts> skill_counts_view();
    // Game producer / Present consumer only. Full channels retain the producer's
    // packet for a later boundary; no roster or owner allocation occurs here.
    bool publish_officer_roster(const domain::OfficerRosterUpdate& update);
    bool consume_officer_roster(domain::OfficerRosterSnapshot& snapshot, domain::GameEpoch epoch);
    void publish_officer_names(domain::OfficerNames names);
    own::shared_owner<const domain::OfficerNames> officer_names_view();
    own::shared_owner<const domain::OfficerNames> officer_name_overrides_view();
    bool publish_resolved_officer_names(domain::OfficerNames names,
        const own::shared_owner<const domain::OfficerNames>& expected_overrides);
    void publish_trait_effects(domain::TraitEffectDefinitions definitions);
    own::shared_owner<const domain::TraitEffectDefinitions> trait_effects_view();
    void publish_custom_traits(domain::CustomTraits traits);
    own::shared_owner<const domain::CustomTraits> custom_traits_view();
    void publish_trait_catalog(own::shared_owner<const domain::TraitRuntimeCatalog> catalog);
    own::shared_owner<const domain::TraitRuntimeCatalog> take_trait_catalog();
    void publish_legacy_trait_texts(own::shared_owner<const domain::TraitTextEdits> rows);
    own::shared_owner<const domain::TraitTextEdits> legacy_trait_texts_view();
    own::shared_owner<const domain::TraitTextEdits> take_legacy_trait_texts();
    bool request_trait_text_save(TraitTextSaveRequest request);
    std::optional<TraitTextSaveRequest> take_trait_text_save();
    void publish_trait_text_save_result(ConfigSaveStatus result);
    std::optional<ConfigSaveStatus> take_trait_text_save_result();
    void request_trait_text_reload();
    bool take_trait_text_reload();
    void publish_city_support_routes(own::shared_owner<const domain::CitySupportRoutes> routes);
    own::shared_owner<const domain::CitySupportRoutes> take_city_support_routes();
    bool request_city_support_save(own::shared_owner<const domain::CitySupportRoutes> routes);
    own::shared_owner<const domain::CitySupportRoutes> take_city_support_save();
    void publish_city_support_save_result(ConfigSaveStatus result);
    std::optional<ConfigSaveStatus> take_city_support_save_result();
    void request_city_support_reload();
    bool take_city_support_reload();
    bool request_officer_name(OfficerNameEdit edit);
    void close_officer_name_requests();
    std::optional<OfficerNameEdit> take_officer_name();
    void publish_officer_name_result(ConfigSaveStatus status);
    std::optional<ConfigSaveStatus> take_officer_name_result();
    void publish_range_images(RangeImages images);
    own::shared_owner<const RangeImages> range_images_view();
    void publish_ui_atlas_images(UiAtlasImages images);
    own::shared_owner<const UiAtlasImages> ui_atlas_images_view();
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
    own::shared_owner<const domain::SpecialtyDefinitions> current_specialty_definitions;
    std::optional<UiSnapshot> current;
    std::optional<std::pair<domain::NamedSpecialties, std::uint32_t>> named_specialties;
    std::optional<domain::SkillCounts> skill_counts;
    own::shared_owner<const domain::SkillCounts> current_skill_counts;
    static constexpr std::size_t ROSTER_PACKET_CAPACITY = 4;
    static constexpr std::size_t ROSTER_PACKETS_PER_FRAME = 2;
    std::array<domain::OfficerRosterUpdate, ROSTER_PACKET_CAPACITY> roster_packets{};
    std::atomic<std::size_t> roster_write{0};
    std::atomic<std::size_t> roster_read{0};
    own::shared_owner<const domain::OfficerNames> current_officer_names;
    own::shared_owner<const domain::OfficerNames> current_officer_name_overrides;
    own::shared_owner<const domain::TraitEffectDefinitions> current_trait_effects;
    own::shared_owner<const domain::CustomTraits> current_custom_traits;
    own::shared_owner<const domain::TraitRuntimeCatalog> pending_trait_catalog;
    own::shared_owner<const domain::TraitTextEdits> current_legacy_trait_texts;
    own::shared_owner<const domain::TraitTextEdits> pending_legacy_trait_texts;
    std::optional<TraitTextSaveRequest> pending_trait_text_save;
    std::optional<ConfigSaveStatus> trait_text_save_result;
    bool trait_text_reload = false;
    own::shared_owner<const domain::CitySupportRoutes> pending_city_support_routes;
    own::shared_owner<const domain::CitySupportRoutes> pending_city_support_save;
    std::optional<ConfigSaveStatus> city_support_save_result;
    bool city_support_reload = false;
    std::optional<OfficerNameEdit> pending_officer_name;
    bool officer_name_requests_open = true;
    std::optional<ConfigSaveStatus> officer_name_result;
    own::shared_owner<const RangeImages> current_range_images;
    own::shared_owner<const UiAtlasImages> current_ui_atlas_images;
    std::optional<domain::SkillCounts> pending_skill_counts_save;
    static constexpr std::size_t notice_capacity = 128;
    std::array<PendingNotice, notice_capacity> notices{};
    std::size_t notice_head = 0;
    std::size_t notice_count = 0;
    std::uint64_t dropped_notices = 0;
    std::optional<domain::ConfigSnapshot> pending_save;
    std::optional<ConfigBaseline> pending_baseline;
    std::optional<ConfigLoad> pending_load;
    std::optional<ConfigSaveNotice> last_notice;
    bool retry_requested = false;
};

} // namespace s8::services
