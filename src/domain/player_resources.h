#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace s8::domain {

enum class PlayerResource : std::uint8_t { Gold, FriendshipTokens, Courage, Privileges, Count };
inline constexpr std::size_t PLAYER_RESOURCE_COUNT = static_cast<std::size_t>(PlayerResource::Count);
inline constexpr std::array<const char*, PLAYER_RESOURCE_COUNT> PLAYER_RESOURCE_LABELS{
    "주인공 금", "우호의 증표", "담력", "특권"};

constexpr std::uint32_t player_resource_limit(PlayerResource resource) {
    switch (resource) {
    case PlayerResource::FriendshipTokens: return (std::numeric_limits<std::uint16_t>::max)();
    case PlayerResource::Privileges: return (std::numeric_limits<std::uint8_t>::max)();
    default: return (std::numeric_limits<std::uint32_t>::max)();
    }
}

struct PlayerResources {
    std::uint16_t officer_id = 0;
    std::array<std::uint32_t, PLAYER_RESOURCE_COUNT> values{};
};

struct SetPlayerResource {
    PlayerResource resource = PlayerResource::Gold;
    std::uint16_t officer_id = 0;
    std::uint32_t expected = 0;
    std::uint32_t value = 0;
};

} // namespace s8::domain
