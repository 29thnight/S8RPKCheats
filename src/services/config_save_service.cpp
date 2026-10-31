#include "config_save_service.h"

#include <utility>

namespace s8::services {

ConfigSaveService::ConfigSaveService(UiBridge& bridge, std::filesystem::path path)
    : bridge(bridge), path(std::move(path)), executor(&bridge.config_wake) {}

void ConfigSaveService::poll() {
    if (auto completion = executor.take_completion()) {
        if (skill_running) {
            skill_completion = std::move(*completion);
            skill_running = false;
        } else if (name_running) {
            name_completion = std::move(*completion);
            name_running = false;
        } else {
            pump.finish(bridge, std::move(*completion));
        }
        running = false;
    }
    if (!running) {
        if (auto job = pump.next(bridge)) {
            executor.start(path, std::move(*job));
            running = true;
        }
    } else if (!skill_running && !name_running) {
        // Coalesce newer snapshots while the worker owns the active revision.
        (void)pump.next(bridge);
    }
}

bool ConfigSaveService::start_officer_name(std::filesystem::path name_path, int id, std::string name) {
    if (running || name_completion) {
        return false;
    }
    executor.start_officer_name(std::move(name_path), id, std::move(name));
    running = true;
    name_running = true;
    return true;
}

std::optional<ConfigSaveResult> ConfigSaveService::take_officer_name_completion() {
    auto result = std::move(name_completion);
    name_completion.reset();
    return result;
}

bool ConfigSaveService::start_skill_counts(std::filesystem::path skill_path,
                                           std::optional<std::string> baseline, domain::SkillCounts counts) {
    if (running || skill_completion) {
        return false;
    }
    executor.start_skill_counts(std::move(skill_path), std::move(baseline), std::move(counts));
    running = true;
    skill_running = true;
    return true;
}

std::optional<ConfigSaveResult> ConfigSaveService::take_skill_counts_completion() {
    auto result = std::move(skill_completion);
    skill_completion.reset();
    return result;
}

bool ConfigSaveService::close_and_wait() {
    if (running) {
        if (!skill_running && !name_running) {
            (void)pump.next(bridge);
        }
        auto completion = executor.wait_completion();
        if (skill_running) {
            skill_completion = std::move(completion);
            skill_running = false;
        } else if (name_running) {
            name_completion = std::move(completion);
            name_running = false;
        } else {
            pump.finish(bridge, std::move(completion));
        }
        running = false;
    }
    while (auto job = pump.next(bridge)) {
        executor.start(path, std::move(*job));
        pump.finish(bridge, executor.wait_completion());
    }
    const bool saved = !pump.failed() && !bridge.has_config_save();
    executor.close_and_wait();
    return saved;
}

} // namespace s8::services
