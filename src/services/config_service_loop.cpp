#include "config_service_loop.h"

#include "config_save_service.h"
#include "config_json.h"
#include "trait_runtime_config.h"
#include "../platform/windows/config_commit.h"
#include "../platform/windows/speed_clock.h"
#include "../../third_party/nlohmann_json/single_include/nlohmann/json.hpp"
#include <charconv>
#include <chrono>
#include <fstream>
#include <iterator>
#include <algorithm>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

namespace s8::services {

bool run_config_service(UiBridge& bridge, const std::filesystem::path& path,
                        std::optional<std::string> skill_baseline, std::stop_token stop) {
    const platform::windows::RealClockScope real_clock;
    ConfigSaveService service(bridge, path);
    const auto skill_path = path.parent_path() / L"S8RPK_skill_counts.json";
    const auto names_path = path.parent_path() / L"S8RPK_cheat_char.json";
    std::optional<domain::SkillCounts> pending_counts;
    std::optional<domain::SkillCounts> active_counts;
    auto last_change = std::chrono::steady_clock::now();
    bool skill_save_failed = false;
    bool config_dispatched = false;
    std::optional<OfficerNameEdit> pending_name;
    bool name_active = false;
    bool name_save_failed = false;
    auto next_config_attempt = std::chrono::steady_clock::time_point{};
    auto next_trait_check = std::chrono::steady_clock::time_point{};
    bool trait_loaded = false;
    bool trait_from_default = false;
    std::optional<std::filesystem::file_time_type> trait_write_time;
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
    while (!stop.stop_requested()) {
        const std::uint64_t observed = bridge.config_wake.observe();
        const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();

        if (now >= next_trait_check) {
            next_trait_check = now + std::chrono::seconds(2);
            std::error_code error;
            const auto cwd = std::filesystem::current_path(error);
            auto custom_path = path.parent_path() / L"san8r_traits_config.json";
            if (!std::filesystem::exists(custom_path, error) && !error) {
                custom_path = cwd / L"san8r_traits_config.json";
            }
            error.clear();
            const bool custom_exists = std::filesystem::exists(custom_path, error) && !error;
            std::optional<std::filesystem::file_time_type> write_time;
            if (custom_exists) {
                const auto value = std::filesystem::last_write_time(custom_path, error);
                if (!error) {
                    write_time = value;
                }
            }
            const bool unchanged_external = trait_loaded && !trait_from_default && custom_exists && write_time &&
                                            trait_write_time && *write_time == *trait_write_time;
            const bool unchanged_default = trait_loaded && trait_from_default && !custom_exists;
            if (!unchanged_external && !unchanged_default) {
                std::ifstream custom_file(custom_path, std::ios::binary);
                std::string text;
                bool from_default = false;
                if (custom_file) {
                    text.assign(std::istreambuf_iterator<char>(custom_file), std::istreambuf_iterator<char>());
                }
                if (text.empty()) {
                    std::array<wchar_t, 32768> executable{};
                    const auto length = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
                    const auto version = length && length < executable.size()
                        ? std::filesystem::path(executable.data()).parent_path() / L"version.dll" : std::filesystem::path{};
                    error.clear();
                    if (version.empty() || !std::filesystem::is_regular_file(version, error)) {
                        auto defaults = path.parent_path() / L"S8RPK_traits_default.json";
                        error.clear();
                        if (!std::filesystem::is_regular_file(defaults, error)) {
                            defaults = cwd / L"S8RPK_traits_default.json";
                        }
                        std::ifstream file(defaults, std::ios::binary);
                        if (file) {
                            text.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
                            from_default = !text.empty();
                        }
                    }
                }
                domain::CustomTraits traits;
                const auto json = nlohmann::json::parse(text, nullptr, false);
                if (json.is_object() && json.contains("customNames") && json["customNames"].is_object()) {
                    const auto& names = json["customNames"];
                    for (auto it = names.begin(); it != names.end(); ++it) {
                        unsigned index = 0;
                        const auto& key = it.key();
                        const auto parsed = std::from_chars(key.data(), key.data() + key.size(), index);
                        if (parsed.ec != std::errc{} || parsed.ptr != key.data() + key.size() || index >= 65535 ||
                            !it.value().is_object() || !it.value().contains("name") || !it.value()["name"].is_string()) {
                            continue;
                        }
                        const auto& name = it.value()["name"].get_ref<const std::string&>();
                        if (name.empty()) {
                            continue;
                        }
                        int grade = 0;
                        if (it.value().contains("grade") && it.value()["grade"].is_number_integer()) {
                            grade = it.value()["grade"].get<int>();
                        }
                        traits[static_cast<std::uint16_t>(index + 1)] = {name, grade};
                    }
                }
                if (!text.empty()) {
                    domain::Result<domain::TraitRuntimeCatalog> catalog = read_trait_runtime_config(text);

                    if (catalog) {
                        bridge.publish_trait_catalog(own::make_shared<domain::TraitRuntimeCatalog>(std::move(*catalog)));
                        bridge.publish_custom_traits(std::move(traits));
                    } else {
                        bridge.publish_notice("특기 설정 형식이 올바르지 않아 이전 설정을 유지합니다.");
                    }
                } else {
                    bridge.publish_trait_catalog(own::make_shared<domain::TraitRuntimeCatalog>());
                    bridge.publish_custom_traits({});
                }
                trait_loaded = true;
                trait_from_default = from_default;
                trait_write_time = !from_default && !text.empty() ? write_time : std::nullopt;
            }
        }
        if (!config_dispatched && now >= next_config_attempt) {
            auto baseline = platform::windows::read_config_for_save(path);
            if (baseline) {
                SecondaryJewelConfig secondary;
                TerrainBonusChanges terrain_bonus{};
                DefBuildingChanges def_building{};
                ScalarConfigChanges scalars{};
                auto lines = config_lines_for_load(*baseline ? std::string_view(**baseline) : std::string_view{},
                                                   &secondary, &terrain_bonus, &def_building, &scalars);
                if (lines) {
                    auto features = domain::read_feature_flag_changes(*lines);
                    bridge.publish_config_load({std::move(*baseline), secondary.mask,
                                                std::move(terrain_bonus), std::move(features), std::move(def_building),
                                                std::move(scalars)});
                    config_dispatched = true;
                }
            }
            next_config_attempt = now + std::chrono::milliseconds(500);
        }
        service.poll();
        poll_skills(false);
        poll_names();
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
    return config_saved && !pending_counts && !name_save_failed;
}

} // namespace s8::services
