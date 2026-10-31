#pragma once

#include "../domain/officer_roster.h"
#include "../domain/ownership.h"
#include "../domain/ids.h"

#include <array>
#include <string>

namespace s8::services {

class UiBridge;

struct NativeOfficerNameService {
    std::array<std::string, 5103> native;
    own::shared_owner<const domain::OfficerNames> overrides;
    domain::GameEpoch epoch{};
    std::uint64_t generation = 0;
    bool changed = false;
    bool publish_ready = false;
};

void poll_native_officer_names(NativeOfficerNameService& state, UiBridge& bridge);

} // namespace s8::services
