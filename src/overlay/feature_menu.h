#pragma once

#include "../domain/config_snapshot.h"
#include "../domain/ids.h"
#include "../services/command_mailbox.h"
#include "../services/ui_bridge.h"
#include "notifications.h"
#include "trait_text_editor.h"
#include "debug_tools_panel.h"
#include "specialty_tools_panel.h"
#include "officer_tools_panel.h"
#include "city_management_panel.h"
#include "corps_deployment_panel.h"
#include "officer_notice_panel.h"
#include "ui_atlas.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace s8::overlay {

struct MenuFloatDraft {
    float value = 0.0f;
    bool editing = false;
    bool commit_requested = false;
    std::optional<domain::RequestId> request;
    std::optional<float> submitted;
};

struct OfficerFactionColorDraft {
    std::uint16_t officer_id = 0;
    std::uint64_t token = 0;
    std::uint8_t expected_color = 0;
    int value = 0;
    bool dirty = false;
};

struct OfficerRosterIndices {
    std::array<std::uint16_t, domain::OFFICER_ROSTER_LIMIT> ids{};
    std::size_t count = 0;

    void clear() noexcept { count = 0; }
    bool empty() const noexcept { return count == 0; }
    std::size_t size() const noexcept { return count; }
    std::uint16_t front() const noexcept { return ids[0]; }
    std::uint16_t operator[](std::size_t index) const noexcept { return ids[index]; }
    const std::uint16_t* begin() const noexcept { return ids.data(); }
    const std::uint16_t* end() const noexcept { return ids.data() + count; }

    void push_back(std::size_t id) noexcept {
        if (id != 0 && id <= domain::OFFICER_ROSTER_LIMIT && count < ids.size()) {
            ids[count++] = static_cast<std::uint16_t>(id);
        }
    }
};

enum class MenuPage : std::uint8_t {
    Overview, Officers, Traits, Domestic, CouncilSocial, Battle, Scenario, Settings, Count
};

struct FeatureMenu {
    std::array<void*, 11> range_textures{};
    UiAtlasTextures ui_atlases{};
    MenuPage page = MenuPage::Overview;
    int status_filter = 0;
    float dpi_scale = 1.0f;
    domain::ConfigSnapshot snapshot;
    own::shared_owner<const domain::SkillCounts> skill_counts;
    own::unique_owner<domain::OfficerRosterSnapshot> officer_roster =
        own::make_unique<domain::OfficerRosterSnapshot>();
    own::shared_owner<const domain::OfficerNames> officer_names;
    domain::GameEpoch epoch{0};
    std::uint64_t next_request = 0;
    std::optional<domain::RequestId> pending;
    std::optional<std::string> pending_notice;
    std::optional<std::string> pending_failure_notice;
    std::optional<domain::Error> error;
    std::optional<services::ConfigSaveNotice> save_notice;
    Notifications notifications;
    TraitTextEditor trait_text_editor;
    DebugToolsPanel debug_tools;
    SpecialtyToolsPanel specialty_tools;
    std::array<std::uint32_t, domain::PLAYER_RESOURCE_COUNT> resource_draft{};
    domain::GameEpoch resource_epoch{};
    std::uint16_t resource_officer = 0;
    OfficerToolsPanel officer_tools;
    CityManagementPanel city_management;
    CorpsDeploymentPanel corps_deployment;
    OfficerNoticePanel officer_notices;
    std::optional<domain::OfficerToolsView> officer_tools_view;
    bool open = false;
    bool ready = false;
    bool file_log_failed = false;
    std::uint64_t file_log_dropped = 0;
    bool collapsed = false;
    bool toggle_collapse = false;
    bool auto_load_applied = false;
    bool auto_load_collapse = false;
    bool compact_layout_checked = false;
    float window_x = 0;
    float window_y = 0;
    bool retry_save_requested = false;
    int cooldown_days_draft = 3;
    int skip_days_draft = 0;
    int siege_heal_rate_draft = 10;
    int mission_appointment_limit_draft = 10;
    MenuFloatDraft ui_scale_draft{1.0f};
    MenuFloatDraft notification_speed_draft{100.0f};
    bool mission_appointment_limit_editing = false;
    bool secondary_jewel_window = false;
    bool protagonist_detail = false;
    bool officer_list_open = false;
    std::optional<std::uint16_t> officer_open_request;
    bool officer_open_refresh_requested = false;
    bool officer_open_focus = false;
    std::array<char, 64> officer_search{};
    int officer_search_result = -1;
    bool officer_search_pending = false;
    std::string officer_pending_search;
    int officer_filter = -1;
    int officer_target_city = 0;
    int officer_cached_filter = -2;
    bool officer_reselect_pending = false;
    std::optional<std::uint16_t> officer_next_selection;
    std::uint64_t officer_filtered_generation = 0;
    std::uint64_t officer_filtered_revision = 0;
    OfficerRosterIndices officer_filtered_indices;
    std::array<bool, 5103> selected_officer_ids{};
    domain::GameEpoch selection_epoch{0};
    std::vector<domain::CommandPayload> pending_officer_edits;
    std::size_t next_officer_edit = 0;
    std::size_t completed_officer_edits = 0;
    std::size_t failed_officer_edits = 0;
    std::vector<domain::RequestId> officer_edits_in_flight;
    std::optional<domain::Error> officer_edit_error;
    domain::GameEpoch officer_edit_epoch{0};
    std::uint16_t selected_basic_id = 0;
    OfficerFactionColorDraft selected_color_draft;
    OfficerFactionColorDraft protagonist_color_draft;
    std::uint16_t officer_name_edit_id = 0;
    std::array<char, 384> officer_name_draft{};
    bool officer_name_save_pending = false;
    bool officer_name_focus = false;
    bool officer_name_edit_protagonist = false;
    bool officer_name_edit_list = false;
    own::shared_owner<const domain::TraitEffectDefinitions> trait_effects;
    own::shared_owner<const domain::CustomTraits> custom_traits;
    std::array<int, domain::officer_basic_fields.size()> selected_basic_draft{};
    std::array<bool, domain::officer_basic_fields.size()> selected_basic_editing{};
    std::array<int, 11> selected_aptitude_draft{};
    std::array<bool, 11> selected_aptitude_editing{};
    std::array<int, 11> protagonist_aptitude_draft{};
    std::array<bool, 11> protagonist_aptitude_editing{};
    std::array<int, domain::officer_basic_fields.size()> protagonist_basic_draft{};
    std::array<bool, domain::officer_basic_fields.size()> protagonist_basic_editing{};
    std::array<char, 128> secondary_jewel_filter{};
    domain::Stratagem5Settings stratagem5_draft;
    domain::SelfHealSettings self_heal_draft;
    domain::DongtoSettings dongto_draft;
    domain::CelestialSettings celestial_draft;
    domain::CatapultSettings catapult_draft;
    domain::TerrainTacticsSettings terrain_tactics_draft;
    domain::DefBuildingSettings def_building_draft;
    domain::TerrainBonus terrain_bonus_draft;
    domain::TengiSettings tengi_draft;
    bool tengi_editing = false;
    bool special_assignment_results_open = false;
    std::uint64_t special_assignment_generation_seen = 0;
};

// Snapshot and completions are delivered by the owner of the Present context.
void on_feature_menu_completion(FeatureMenu& menu, const services::CommandCompletion& completion);
void update_feature_menu(FeatureMenu& menu, services::UiBridge& bridge);
void request_feature_menu_toggle(FeatureMenu& menu);
void draw_feature_menu(FeatureMenu& menu, services::CommandMailbox& mailbox);
void draw_protagonist_abilities(FeatureMenu& menu, services::UiBridge& bridge);
void draw_officer_roster(FeatureMenu& menu, services::UiBridge& bridge);
void draw_officer_tools(FeatureMenu& menu, services::UiBridge& bridge);

} // namespace s8::overlay
