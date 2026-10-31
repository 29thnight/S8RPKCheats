#pragma once

namespace s8::domain {

struct SpeedHackSettings {
    bool enabled = false;
    float multiplier = 1.0f;
};

constexpr float normalize_speed_multiplier(float value) {
    if (!(value >= 1.0f)) {
        return 1.0f;
    }

    if (value >= 5.0f) {
        return 5.0f;
    }

    const int half_steps = static_cast<int>((value - 1.0f) * 2.0f + 0.5f);

    return 1.0f + static_cast<float>(half_steps) * 0.5f;
}

} // namespace s8::domain
