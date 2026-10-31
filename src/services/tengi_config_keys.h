#pragma once

#include "../domain/tengi.h"

#include <array>
#include <string_view>

namespace s8::services {

struct TengiConfigKeys {
    std::array<char, 32> allowed{};
    std::array<char, 32> duration{};
    std::array<char, 32> cooldown{};
};

consteval std::array<char, 32> indexed_tengi_key(std::string_view prefix, std::size_t index) {
    std::array<char, 32> text{};
    std::size_t cursor = 0;

    for (const char value : prefix) {
        text[cursor++] = value;
    }
    if (index >= 10) {
        text[cursor++] = static_cast<char>('0' + index / 10);
    }
    text[cursor] = static_cast<char>('0' + index % 10);

    return text;
}

consteval std::array<TengiConfigKeys, domain::TENGI_EVENT_COUNT> make_tengi_config_keys() {
    std::array<TengiConfigKeys, domain::TENGI_EVENT_COUNT> keys{};

    for (std::size_t index = 0; index < keys.size(); ++index) {
        keys[index].allowed = indexed_tengi_key("tengiListAllow", index);
        keys[index].duration = indexed_tengi_key("tengiListMonths", index);
        keys[index].cooldown = indexed_tengi_key("tengiListCooldownYears", index);
    }

    return keys;
}

inline constexpr std::array<TengiConfigKeys, domain::TENGI_EVENT_COUNT> TENGI_CONFIG_KEYS = make_tengi_config_keys();

} // namespace s8::services
