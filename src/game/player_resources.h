#pragma once

#include "../domain/error.h"
#include "../domain/player_resources.h"

#include <optional>

namespace s8::game {

std::optional<domain::PlayerResources> read_player_resources(std::uintptr_t root_pointer,
    std::uintptr_t game_base, std::uintptr_t player);
domain::Result<void> write_player_resource(std::uintptr_t root_pointer,
    std::uintptr_t game_base, std::uintptr_t player, const domain::SetPlayerResource& request);

} // namespace s8::game
