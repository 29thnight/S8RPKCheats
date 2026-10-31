#pragma once

#include "../domain/stratagem5.h"

#include <string_view>

namespace s8::game {

struct Stratagem5Config {
    bool enabled = false;
    domain::Stratagem5Settings settings;
};

// Reads legacy keys; missing integers retain current values. The caller normalizes them.
Stratagem5Config read_stratagem5_config(std::string_view text, domain::Stratagem5Settings current);

} // namespace s8::game
