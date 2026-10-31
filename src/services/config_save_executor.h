#pragma once

#include "config_save_pump.h"

#include "../../third_party/stdexec/include/exec/static_thread_pool.hpp"
#include "../../third_party/stdexec/include/stdexec/execution.hpp"

#include <filesystem>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <stop_token>

namespace s8::services {

// One job at a time. The owner drains the scope outside callbacks before destruction.
class ConfigSaveExecutor {
public:
    ConfigSaveExecutor() = default;
    ConfigSaveExecutor(const ConfigSaveExecutor&) = delete;
    ConfigSaveExecutor& operator=(const ConfigSaveExecutor&) = delete;

    void start(std::filesystem::path path, ConfigSaveJob job);
    void start_skill_counts(std::filesystem::path path, std::optional<std::string> baseline,
                            domain::SkillCounts counts);
    void start_officer_name(std::filesystem::path path, int id, std::string name);
    std::optional<ConfigSaveResult> take_completion();
    ConfigSaveResult wait_completion();
    void request_stop();
    void close_and_wait();

private:
    exec::static_thread_pool pool{1};
    stdexec::counting_scope scope;
    std::stop_source stop;
    std::mutex mutex;
    std::condition_variable completed;
    std::optional<ConfigSaveResult> completion;
};

} // namespace s8::services
