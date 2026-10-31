#pragma once

#include "../domain/corps_deployment.h"
#include "../domain/officer_roster.h"
#include "../services/command_mailbox.h"
#include "../services/corps_bridge.h"

#include <cstdint>
#include <optional>

namespace s8::overlay {

struct CorpsDeploymentPanel {
    domain::GameEpoch epoch{};
    std::optional<domain::RequestId> pending;
    int officer = 1;
    int candidate = 1;
    std::uint8_t target_city = domain::NO_CITY;
    bool open = false;
    bool was_open = false;
    const char* error = nullptr;
};

bool on_corps_deployment_completion(CorpsDeploymentPanel& panel, const services::CommandCompletion& completion);
void draw_corps_deployment(CorpsDeploymentPanel& panel, services::CorpsBridge& bridge,
    services::CommandMailbox& commands, domain::GameEpoch epoch, std::uint64_t& next_request,
    const domain::OfficerNames* names = nullptr);

} // namespace s8::overlay
