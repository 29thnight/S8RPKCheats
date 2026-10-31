#pragma once

#include <cstdint>

namespace s8::domain {

struct Stratagem5Settings {
    int target = 1;
    int effect1 = 10;
    int power1 = 40;
    int duration1 = 0;
    int effect2 = 0;
    int power2 = 0;
    int duration2 = 0;
    int range = 5;
    int heal_amount = 2000;
};

// Value model only; the game adapter is responsible for its own memory layout.
struct StratagemRecord {
    std::int16_t code1;
    std::int16_t code2;
    std::int16_t code3;
    std::int16_t target;
    std::int16_t effect1;
    std::int16_t power1;
    std::int16_t duration1;
    std::int16_t effect2;
    std::int16_t power2;
    std::int16_t duration2;
    std::int16_t range;
    std::int16_t unknown16;
};

// Normalize external settings once before using the returned value below.
Stratagem5Settings normalize_stratagem5(Stratagem5Settings settings);
StratagemRecord apply_stratagem5(StratagemRecord record, const Stratagem5Settings& settings);
bool matches_stratagem5(const StratagemRecord& record, const Stratagem5Settings& settings);

} // namespace s8::domain
