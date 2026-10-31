#include "config_save_executor.h"
#include "config_json.h"
#include "../platform/windows/unicode.h"
#include "../platform/windows/speed_clock.h"
#include "../../third_party/nlohmann_json/single_include/nlohmann/json.hpp"

#include <utility>

namespace s8::services {

ConfigSaveExecutor::~ConfigSaveExecutor() {
    // The service thread owns this object. Exceptions in unrelated decoding or
    // resource loading still drain jobs before their captured context dies.
    close_and_wait();
}

void ConfigSaveExecutor::start_officer_name(std::filesystem::path path, int id, std::string name) {
    auto work = stdexec::starts_on(pool.get_scheduler(),
        stdexec::just() | stdexec::then([path = std::move(path), id, name = std::move(name)]() {
            const platform::windows::RealClockScope real_clock;

            auto baseline = platform::windows::read_config_for_save(path);
            if (!baseline) {
                return ConfigSaveResult{0, ConfigSaveStatus::Failed};
            }
            auto normalized = platform::windows::normalize_utf8(name);
            auto bytes = update_officer_name(*baseline ? std::string_view(**baseline) : std::string_view{}, id, normalized);
            if (!bytes) {
                return ConfigSaveResult{0, ConfigSaveStatus::InvalidData};
            }
            const auto commit = platform::windows::commit_config(path, *baseline, *bytes, {});
            switch (commit.status) {
            case platform::windows::ConfigCommitStatus::Completed:
                return ConfigSaveResult{0, ConfigSaveStatus::Completed, 0, {}, std::move(*bytes)};
            case platform::windows::ConfigCommitStatus::Cancelled:
                return ConfigSaveResult{0, ConfigSaveStatus::Cancelled};
            case platform::windows::ConfigCommitStatus::Conflict:
                return ConfigSaveResult{0, ConfigSaveStatus::Conflict};
            case platform::windows::ConfigCommitStatus::Failed:
                return ConfigSaveResult{0, ConfigSaveStatus::Failed, commit.native_code, commit.retained_temp};
            }
            return ConfigSaveResult{0, ConfigSaveStatus::Failed};
        })) | stdexec::upon_error([](auto&&) noexcept { return ConfigSaveResult{0, ConfigSaveStatus::Failed}; })
        | stdexec::upon_stopped([]() noexcept { return ConfigSaveResult{0, ConfigSaveStatus::Cancelled}; })
        | stdexec::then([this](ConfigSaveResult result) noexcept {
            {
                std::lock_guard lock(mutex);
                completion = std::move(result);
            }
            completed.notify_one();
            if (wake) {
                wake->signal();
            }
        });
    stdexec::spawn(std::move(work), scope.get_token());
}

void ConfigSaveExecutor::start_skill_counts(std::filesystem::path path, std::optional<std::string> baseline,
                                            domain::SkillCounts counts) {
    auto work = stdexec::starts_on(pool.get_scheduler(),
        stdexec::just() | stdexec::then([path = std::move(path), baseline = std::move(baseline),
                                         counts = std::move(counts)]() {
            const platform::windows::RealClockScope real_clock;

            auto json = nlohmann::json::object();
            for (const auto& [officer, skills] : counts) {
                auto entries = nlohmann::json::object();
                for (const auto& [offset, count] : skills) {
                    entries[std::to_string(offset)] = count;
                }
                json[std::to_string(officer)] = std::move(entries);
            }
            auto bytes = json.dump(2) + "\n";
            const auto commit = platform::windows::commit_config(path, baseline, bytes, {});
            ConfigSaveResult result{0, commit.status == platform::windows::ConfigCommitStatus::Completed
                ? ConfigSaveStatus::Completed : ConfigSaveStatus::Failed};
            if (result.status == ConfigSaveStatus::Completed) {
                result.persisted_content = std::move(bytes);
            }
            return result;
        })) | stdexec::upon_error([](auto&&) noexcept { return ConfigSaveResult{0, ConfigSaveStatus::Failed}; })
        | stdexec::upon_stopped([]() noexcept { return ConfigSaveResult{0, ConfigSaveStatus::Cancelled}; })
        | stdexec::then([this](ConfigSaveResult result) noexcept {
            {
                std::lock_guard lock(mutex);
                completion = std::move(result);
            }
            completed.notify_one();
            if (wake) {
                wake->signal();
            }
        });
    stdexec::spawn(std::move(work), scope.get_token());
}

void ConfigSaveExecutor::start(std::filesystem::path path, ConfigSaveJob job) {
    const auto revision = job.save.revision;
    const auto token = stop.get_token();
    auto work = stdexec::starts_on(pool.get_scheduler(),
        stdexec::just() | stdexec::then([path = std::move(path), job = std::move(job), token]() {
            const platform::windows::RealClockScope real_clock;

            return save_config(path, job.baseline, job.save, token);
        }))
        | stdexec::upon_error([revision](auto&&) noexcept {
            return ConfigSaveResult{revision, ConfigSaveStatus::Failed};
        })
        | stdexec::upon_stopped([revision]() noexcept {
            return ConfigSaveResult{revision, ConfigSaveStatus::Cancelled};
        })
        | stdexec::then([this](ConfigSaveResult result) noexcept {
            {
                std::lock_guard lock(mutex);
                completion = std::move(result);
            }
            completed.notify_one();
            if (wake) {
                wake->signal();
            }
        });
    stdexec::spawn(std::move(work), scope.get_token());
}

ConfigSaveResult ConfigSaveExecutor::wait_completion() {
    std::unique_lock lock(mutex);
    completed.wait(lock, [this] { return completion.has_value(); });
    ConfigSaveResult result = std::move(*completion);

    completion.reset();
    return result;
}

std::optional<ConfigSaveResult> ConfigSaveExecutor::take_completion() {
    std::lock_guard lock(mutex);
    std::optional<ConfigSaveResult> result = std::move(completion);

    completion.reset();
    return result;
}

void ConfigSaveExecutor::request_stop() {
    stop.request_stop();
}

void ConfigSaveExecutor::close_and_wait() {
    if (closed) {
        return;
    }
    scope.close();
    (void)stdexec::sync_wait(scope.join());
    closed = true;
}

} // namespace s8::services
