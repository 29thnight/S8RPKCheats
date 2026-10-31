#include "config_save.h"

#include "config_changes.h"
#include "config_json.h"

namespace s8::services {

ConfigSaveResult save_config(const std::filesystem::path& path, const std::optional<std::string>& baseline,
                             const ConfigSave& save, std::stop_token stop) {
    if (stop.stop_requested()) {
        return {save.revision, ConfigSaveStatus::Cancelled};
    }
    const auto changes = config_changes(save.snapshot);
    const auto content = update_config_json(baseline ? std::string_view(*baseline) : std::string_view{}, changes);
    if (!content) {
        return {save.revision, ConfigSaveStatus::InvalidData};
    }
    const auto commit = platform::windows::commit_config(path, baseline, *content, stop);
    switch (commit.status) {
    case platform::windows::ConfigCommitStatus::Completed:
        return {save.revision, ConfigSaveStatus::Completed, 0, {}, *content};
    case platform::windows::ConfigCommitStatus::Cancelled:
        return {save.revision, ConfigSaveStatus::Cancelled};
    case platform::windows::ConfigCommitStatus::Conflict:
        return {save.revision, ConfigSaveStatus::Conflict};
    case platform::windows::ConfigCommitStatus::Failed:
        return {save.revision, ConfigSaveStatus::Failed, commit.native_code, commit.retained_temp};
    }
    return {save.revision, ConfigSaveStatus::Failed};
}

} // namespace s8::services
