#include "stratagem5.h"

#include <algorithm>

namespace s8::domain {

Stratagem5Settings normalize_stratagem5(Stratagem5Settings settings) {
    const auto normalize_effect = [](int effect) {
        switch (effect) {
        case 0:
        case 3:
        case 10:
        case 11:
        case 12:
            return effect;
        default:
            return 0;
        }
    };

    settings.target = std::clamp(settings.target, 1, 3);
    settings.effect1 = normalize_effect(settings.effect1);
    settings.effect2 = normalize_effect(settings.effect2);
    settings.power1 = std::clamp(settings.power1, -32768, 32767);
    settings.power2 = std::clamp(settings.power2, -32768, 32767);
    settings.duration1 = std::clamp(settings.duration1, 0, 30);
    settings.duration2 = std::clamp(settings.duration2, 0, 30);
    settings.range = std::clamp(settings.range, 1, 100);
    settings.heal_amount = std::clamp(settings.heal_amount, 0, 65535);
    return settings;
}

StratagemRecord apply_stratagem5(StratagemRecord record, const Stratagem5Settings& settings) {
    record.code1 = 5;
    record.code2 = 5;
    record.code3 = 5;
    record.target = static_cast<std::int16_t>(settings.target);
    record.effect1 = static_cast<std::int16_t>(settings.effect1);
    record.power1 = static_cast<std::int16_t>(settings.power1);
    record.duration1 = static_cast<std::int16_t>(settings.duration1);
    record.effect2 = static_cast<std::int16_t>(settings.effect2);
    record.power2 = static_cast<std::int16_t>(settings.power2);
    record.duration2 = static_cast<std::int16_t>(settings.duration2);
    record.range = static_cast<std::int16_t>(settings.range);
    return record;
}

bool matches_stratagem5(const StratagemRecord& record, const Stratagem5Settings& settings) {
    return record.code1 == 5 && record.code2 == 5 && record.code3 == 5 &&
           record.target == settings.target &&
           record.effect1 == settings.effect1 && record.power1 == settings.power1 &&
           record.duration1 == settings.duration1 &&
           record.effect2 == settings.effect2 && record.power2 == settings.power2 &&
           record.duration2 == settings.duration2 && record.range == settings.range;
}

} // namespace s8::domain
