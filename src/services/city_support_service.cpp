#include "city_support_service.h"

#include "city_support_config.h"
#include "../platform/windows/config_commit.h"

#include <utility>

namespace s8::services {

CitySupportService::CitySupportService(std::filesystem::path directory)
    : path(std::move(directory) / L"S8RPK_rear_support_routes.json") {}

void CitySupportService::load(UiBridge& bridge) {
    const domain::Result<std::optional<std::string>> source =
        platform::windows::read_config_for_save(path);

    if (!source) {
        bridge.publish_notice("후방 지원 경로 파일을 읽지 못했습니다. 현재 경로를 유지합니다.");
        return;
    }
    domain::Result<domain::CitySupportRoutes> routes = *source
        ? read_city_support_config(**source)
        : domain::Result<domain::CitySupportRoutes>{domain::CitySupportRoutes{}};

    if (!routes) {
        bridge.publish_notice("후방 지원 경로 파일이 유효하지 않습니다. 현재 경로를 유지합니다.");
        return;
    }
    own::shared_owner<const domain::CitySupportRoutes> published =
        own::make_shared<domain::CitySupportRoutes>(std::move(*routes));

    baseline = *source;
    loaded = true;
    bridge.publish_city_support_routes(std::move(published));
}

void CitySupportService::poll(UiBridge& bridge) {
    // Save an already admitted edit against its original baseline before a
    // simultaneous reload can replace that baseline.
    const own::shared_owner<const domain::CitySupportRoutes> request = bridge.take_city_support_save();

    if (request) {
        domain::Result<std::string> contents = write_city_support_config(*request);
        ConfigSaveStatus status = ConfigSaveStatus::InvalidData;

        if (loaded && contents) {
            const platform::windows::ConfigCommitResult committed =
                platform::windows::commit_config(path, baseline, *contents, {});

            switch (committed.status) {
            case platform::windows::ConfigCommitStatus::Completed:
                baseline = std::move(*contents);
                status = ConfigSaveStatus::Completed;
                break;
            case platform::windows::ConfigCommitStatus::Conflict:
                status = ConfigSaveStatus::Conflict;
                break;
            case platform::windows::ConfigCommitStatus::Cancelled:
                status = ConfigSaveStatus::Cancelled;
                break;
            case platform::windows::ConfigCommitStatus::Failed:
                status = ConfigSaveStatus::Failed;
                break;
            }
        }
        save_failed = status != ConfigSaveStatus::Completed;
        if (!save_failed) {
            bridge.publish_city_support_routes(request);
        }
        bridge.publish_city_support_save_result(status);
    }
    if (bridge.take_city_support_reload()) {
        load(bridge);
    }
}

bool CitySupportService::saved() const noexcept {
    return !save_failed;
}

} // namespace s8::services
