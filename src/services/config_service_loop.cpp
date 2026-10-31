#include "config_service_loop.h"

#include "config_save_service.h"
#include "config_json.h"
#include "trait_catalog_service.h"
#include "trait_text_service.h"
#include "city_support_service.h"
#include "native_officer_name_service.h"
#include "../platform/windows/config_commit.h"
#include "../platform/windows/speed_clock.h"
#include <chrono>
#include <algorithm>

namespace s8::services {

bool run_config_service(UiBridge& bridge, const std::filesystem::path& path,
                        std::optional<std::string> skill_baseline, std::stop_token stop) {
    const platform::windows::RealClockScope real_clock;
    ConfigSaveService service(bridge, path);
    TraitTextService trait_texts(path.parent_path());
    TraitCatalogService trait_catalogs(path.parent_path());
    CitySupportService city_support(path.parent_path());
    NativeOfficerNameService native_names;
    FileLogWriter file_log(path.parent_path() / L"S8RPK_cheat.log");
    bool log_failure_reported = false;
    const auto skill_path = path.parent_path() / L"S8RPK_skill_counts.json";
    const auto names_path = path.parent_path() / L"S8RPK_cheat_char.json";
    std::optional<domain::SkillCounts> pending_counts;
    std::optional<domain::SkillCounts> active_counts;
    auto last_change = std::chrono::steady_clock::now();
    bool skill_save_failed = false;
    bool config_dispatched = false;
    bool config_failure_reported = false;
    std::optional<OfficerNameEdit> pending_name;
    bool name_active = false;
    bool name_save_failed = false;
    auto next_config_attempt = std::chrono::steady_clock::time_point{};
    auto next_trait_check = std::chrono::steady_clock::time_point{};
    const auto poll_skills = [&](bool stopping) {
        if (auto completion = service.take_skill_counts_completion()) {
            if (completion->status == ConfigSaveStatus::Completed) {
                skill_baseline = std::move(completion->persisted_content);
            } else {
                if (!pending_counts) {
                    pending_counts = std::move(active_counts);
                }
                skill_save_failed = true;
                bridge.publish_notice("무장 전법 설정 저장 실패");
            }
            active_counts.reset();
        }
        if (auto counts = bridge.take_skill_counts_save()) {
            pending_counts = std::move(*counts);
            last_change = std::chrono::steady_clock::now();
            skill_save_failed = false;
        }
        if (!active_counts && pending_counts && !skill_save_failed &&
            (stopping || std::chrono::steady_clock::now() - last_change >= std::chrono::milliseconds(300)) &&
            service.start_skill_counts(skill_path, skill_baseline, *pending_counts)) {
            active_counts = std::move(pending_counts);
            pending_counts.reset();
        }
    };
    const auto poll_names = [&]() {
        if (auto completion = service.take_officer_name_completion()) {
            const bool succeeded = completion->status == ConfigSaveStatus::Completed;
            name_save_failed = !succeeded;
            if (succeeded) {
                if (auto names = read_officer_names(*completion->persisted_content)) {
                    bridge.publish_officer_names(std::move(*names));
                }
            }
            bridge.publish_officer_name_result(completion->status);
            name_active = false;
        }
        if (!pending_name && !name_active) {
            pending_name = bridge.take_officer_name();
        }
        if (pending_name && !name_active && service.start_officer_name(names_path, pending_name->id, pending_name->name)) {
            pending_name.reset();
            name_active = true;
        }
    };
    trait_texts.load_legacy(bridge);
    city_support.load(bridge);
    while (!stop.stop_requested()) {
        const std::uint64_t observed = bridge.config_wake.observe();
        const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();

        const bool reload_traits = bridge.take_trait_text_reload();

        if (reload_traits) {
            trait_texts.load_legacy(bridge);
        }
        if (reload_traits || now >= next_trait_check) {
            next_trait_check = now + std::chrono::seconds(2);
            trait_catalogs.poll(bridge, trait_texts, reload_traits);
        }
        if (!config_dispatched && now >= next_config_attempt) {
            domain::Result<std::optional<std::string>> baseline = platform::windows::read_config_for_save(path);

            if (baseline) {
                SecondaryJewelConfig secondary;
                TerrainBonusChanges terrain_bonus{};
                DefBuildingChanges def_building{};
                ScalarConfigChanges scalars{};
                const domain::Result<std::string> lines = config_lines_for_load(
                    *baseline ? std::string_view(**baseline) : std::string_view{},
                    &secondary, &terrain_bonus, &def_building, &scalars);

                if (lines) {
                    domain::FeatureFlagChanges features = domain::read_feature_flag_changes(*lines);

                    bridge.publish_config_load({std::move(*baseline), secondary.mask,
                                                std::move(terrain_bonus), std::move(features), std::move(def_building),
                                                std::move(scalars)});
                    config_dispatched = true;

                    if (config_failure_reported) {
                        bridge.publish_notice("설정 파일을 다시 읽었습니다.");
                    }
                } else if (!config_failure_reported) {
                    bridge.publish_notice("설정 파일의 JSON 형식 또는 중복 항목을 확인해 주세요. 수정하면 자동으로 다시 읽습니다.");
                    config_failure_reported = true;
                }
            } else if (!config_failure_reported) {
                bridge.publish_notice("설정 파일을 읽지 못했습니다. 파일 접근 권한을 확인해 주세요. 오류 " +
                    std::to_string(baseline.error().native_code));
                config_failure_reported = true;
            }
            next_config_attempt = now + std::chrono::milliseconds(500);
        }
        service.poll();
        poll_skills(false);
        poll_names();
        poll_native_officer_names(native_names, bridge);
        trait_texts.poll(bridge);
        city_support.poll(bridge);
        file_log.poll(bridge.file_log);
        if (bridge.file_log.failed() && !log_failure_reported) {
            bridge.publish_notice("파일 로그 저장에 실패했습니다. 경로를 확인한 뒤 옵션을 껐다 켜면 재시도합니다.");
            log_failure_reported = true;
        } else if (!bridge.file_log.failed()) {
            log_failure_reported = false;
        }
        std::chrono::steady_clock::time_point deadline = next_trait_check;

        if (!config_dispatched) {
            deadline = std::min(deadline, next_config_attempt);
        }
        if (pending_counts && !active_counts && !skill_save_failed) {
            const std::chrono::steady_clock::time_point debounce = last_change + std::chrono::milliseconds(300);

            if (debounce > now) {
                deadline = std::min(deadline, debounce);
            }
        }
        if (bridge.file_log.pending() || bridge.native_officer_names.pending()) {
            deadline = now;
        }
        bridge.config_wake.wait_until(observed, deadline, stop);
    }
    do {
        const std::uint64_t observed = bridge.config_wake.observe();

        service.poll();
        poll_skills(true);
        poll_names();
        if (active_counts || (pending_counts && !skill_save_failed) || name_active || pending_name) {
            bridge.config_wake.wait_until(observed, std::chrono::steady_clock::time_point::max());
        }
    } while (active_counts || (pending_counts && !skill_save_failed) || name_active || pending_name);
    const bool config_saved = service.close_and_wait();

    poll_native_officer_names(native_names, bridge);
    trait_texts.poll(bridge);
    city_support.poll(bridge);
    file_log.close(bridge.file_log);
    return config_saved && !pending_counts && !name_save_failed && city_support.saved();
}

} // namespace s8::services
