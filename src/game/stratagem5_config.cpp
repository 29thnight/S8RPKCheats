#include "stratagem5_config.h"
#include "legacy_config.h"

#include <array>

namespace s8::game {

Stratagem5Config read_stratagem5_config(std::string_view text, domain::Stratagem5Settings current) {
    Stratagem5Config config;
    config.enabled = read_legacy_bool(text, "\"bStratagemFiveEnabled\"").value_or(false);

    struct Field { std::string_view key; int domain::Stratagem5Settings::* member; };
    constexpr std::array FIELDS{
        Field{"\"iStratagem5Target\"", &domain::Stratagem5Settings::target},
        Field{"\"iStratagem5Effect1\"", &domain::Stratagem5Settings::effect1},
        Field{"\"iStratagem5Power1\"", &domain::Stratagem5Settings::power1},
        Field{"\"iStratagem5Duration1\"", &domain::Stratagem5Settings::duration1},
        Field{"\"iStratagem5Effect2\"", &domain::Stratagem5Settings::effect2},
        Field{"\"iStratagem5Power2\"", &domain::Stratagem5Settings::power2},
        Field{"\"iStratagem5Duration2\"", &domain::Stratagem5Settings::duration2},
        Field{"\"iStratagem5Range\"", &domain::Stratagem5Settings::range},
        Field{"\"iStratagem5HealAmount\"", &domain::Stratagem5Settings::heal_amount},
    };
    for (const auto& field : FIELDS)
        if (const auto value = read_legacy_int(text, field.key))
            current.*(field.member) = *value;
    config.settings = current;
    return config;
}

} // namespace s8::game
