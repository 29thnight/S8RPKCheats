#pragma once

#include "services/ui_bridge.h"

#include <map>
#include <string_view>

namespace s8::preview {

// Deliberately local sample backend. It does not open a game process, patch
// memory, load a save file, or read/write the user's trainer settings.
class SampleSession {
public:
    services::UiBridge bridge;
    domain::ConfigSnapshot config;
    domain::GameEpoch epoch{1};
    std::size_t accepted_commands = 0;
    std::size_t unsupported_commands = 0;
    int response_delay_frames = 0;

    explicit SampleSession(std::string_view scenario);
    void pump();

private:
    domain::OfficerNames names;
    std::map<std::uint16_t, domain::OfficerDetail> officers;
    domain::CitySnapshot cities;
    std::uint64_t roster_generation = 0;
    std::optional<services::AcceptedCommand> held_command;
    int response_wait = 0;
    void publish_roster();
    void publish_selected(std::uint16_t id);
    domain::Result<void> apply(const domain::CommandPayload& payload);
};

} // namespace s8::preview
