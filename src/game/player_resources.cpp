#include "player_resources.h"

#include "../platform/windows/process_memory.h"

#include <array>
#include <span>

namespace s8::game {
namespace {

constexpr std::array<std::uintptr_t, domain::PLAYER_RESOURCE_COUNT> OFFSETS{0x300, 0xf8, 0x5bb8, 0xea};
constexpr std::array<std::size_t, domain::PLAYER_RESOURCE_COUNT> WIDTHS{4, 2, 4, 1};

template<class T> bool read_value(std::uintptr_t address, T& value) {
    return platform::windows::read_memory(address, std::as_writable_bytes(std::span(&value, 1)));
}

bool same_player(std::uintptr_t root_pointer, std::uintptr_t root, std::uintptr_t player, std::uint16_t id) {
    std::uintptr_t observed_root = 0;
    std::uintptr_t observed_player = 0;
    std::uint16_t observed_id = 0;

    return root > 0x10000 && player > 0x10000 && root_pointer &&
        read_value(root_pointer, observed_root) && observed_root == root &&
        read_value(root + 0xe0, observed_player) && observed_player == player &&
        read_value(player + 8, observed_id) && observed_id == id;
}

bool contains_only_attempted_bytes(std::span<const std::byte> current,
    std::span<const std::byte> previous, std::span<const std::byte> replacement) {
    for (std::size_t index = 0; index < current.size(); ++index) {
        if (current[index] != previous[index] && current[index] != replacement[index]) {
            return false;
        }
    }
    return true;
}

} // namespace

std::optional<domain::PlayerResources> read_player_resources(std::uintptr_t root_pointer,
    std::uintptr_t game_base, std::uintptr_t player) {
    domain::PlayerResources resources;

    if (player <= 0x10000 || !read_value(player + 8, resources.officer_id) ||
        resources.officer_id < 1 || resources.officer_id > 5102 ||
        !same_player(root_pointer, game_base, player, resources.officer_id)) {
        return std::nullopt;
    }
    for (std::size_t index = 0; index < domain::PLAYER_RESOURCE_COUNT; ++index) {
        const std::uintptr_t address = (index == 0 ? player : game_base) + OFFSETS[index];
        const std::span<std::byte> bytes(reinterpret_cast<std::byte*>(&resources.values[index]), WIDTHS[index]);

        if (!platform::windows::read_memory(address, bytes)) {
            return std::nullopt;
        }
    }
    return same_player(root_pointer, game_base, player, resources.officer_id)
        ? std::optional<domain::PlayerResources>{resources} : std::nullopt;
}

domain::Result<void> write_player_resource(std::uintptr_t root_pointer,
    std::uintptr_t game_base, std::uintptr_t player, const domain::SetPlayerResource& request) {
    const std::size_t index = static_cast<std::size_t>(request.resource);

    if (index >= domain::PLAYER_RESOURCE_COUNT || request.officer_id < 1 || request.officer_id > 5102 ||
        request.expected > domain::player_resource_limit(request.resource) ||
        request.value > domain::player_resource_limit(request.resource)) {
        return std::unexpected(domain::Error{domain::ErrorCode::InvalidData});
    }
    if (!same_player(root_pointer, game_base, player, request.officer_id)) {
        return std::unexpected(domain::Error{domain::ErrorCode::StaleEpoch});
    }
    const std::uintptr_t address = (index == 0 ? player : game_base) + OFFSETS[index];
    const std::size_t width = WIDTHS[index];
    std::uint32_t current = 0;
    const std::span<std::byte> current_bytes(reinterpret_cast<std::byte*>(&current), width);
    const std::span<const std::byte> previous(reinterpret_cast<const std::byte*>(&request.expected), width);
    const std::span<const std::byte> replacement(reinterpret_cast<const std::byte*>(&request.value), width);

    if (!platform::windows::read_memory(address, current_bytes) || current != request.expected) {
        return std::unexpected(domain::Error{domain::ErrorCode::StaleEpoch});
    }
    if (!same_player(root_pointer, game_base, player, request.officer_id)) {
        return std::unexpected(domain::Error{domain::ErrorCode::StaleEpoch});
    }
    if (current == request.value) {
        return {};
    }
    const bool written = platform::windows::write_data_memory(address, replacement);
    const bool observed = platform::windows::read_memory(address, current_bytes);

    if (!same_player(root_pointer, game_base, player, request.officer_id)) {
        return std::unexpected(domain::Error{domain::ErrorCode::StaleEpoch, 0x5202});
    }
    if (written && observed && current == request.value) {
        return {};
    }
    if (!observed) {
        return std::unexpected(domain::Error{domain::ErrorCode::PlatformFailure, 0x5202});
    }
    if (current == request.expected) {
        return std::unexpected(domain::Error{domain::ErrorCode::PlatformFailure, 0x5201});
    }
    // A failed write may have changed only part of the scalar. Restore a
    // same-session value only while every byte is old or from our attempted write.
    if (!contains_only_attempted_bytes(current_bytes, previous, replacement)) {
        return std::unexpected(domain::Error{domain::ErrorCode::PlatformFailure, 0x5202});
    }
    if (!same_player(root_pointer, game_base, player, request.officer_id)) {
        return std::unexpected(domain::Error{domain::ErrorCode::StaleEpoch, 0x5202});
    }
    const bool restored = platform::windows::write_data_memory(address, previous);
    const bool restoration_observed = platform::windows::read_memory(address, current_bytes);

    if (!same_player(root_pointer, game_base, player, request.officer_id)) {
        return std::unexpected(domain::Error{domain::ErrorCode::StaleEpoch, 0x5202});
    }
    if (!restored || !restoration_observed || current != request.expected) {
        return std::unexpected(domain::Error{domain::ErrorCode::PlatformFailure, 0x5202});
    }
    return std::unexpected(domain::Error{domain::ErrorCode::PlatformFailure, 0x5201});
}

} // namespace s8::game
