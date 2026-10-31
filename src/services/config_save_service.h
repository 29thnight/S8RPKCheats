#pragma once

#include "config_save_executor.h"

namespace s8::services {

class ConfigSaveService {
public:
    ConfigSaveService(UiBridge& bridge, std::filesystem::path path);
    void poll();
    bool start_skill_counts(std::filesystem::path path, std::optional<std::string> baseline,
                            domain::SkillCounts counts);
    std::optional<ConfigSaveResult> take_skill_counts_completion();
    bool start_officer_name(std::filesystem::path name_path, int id, std::string name);
    std::optional<ConfigSaveResult> take_officer_name_completion();
    // Call after producers stop submitting; false means a save remains unsatisfied.
    bool close_and_wait();

private:
    UiBridge& bridge;
    std::filesystem::path path;
    ConfigSavePump pump;
    ConfigSaveExecutor executor;
    bool running = false;
    bool skill_running = false;
    bool name_running = false;
    std::optional<ConfigSaveResult> skill_completion;
    std::optional<ConfigSaveResult> name_completion;
};

} // namespace s8::services
