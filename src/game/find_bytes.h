#pragma once

#include <cstddef>
#include <optional>
#include <span>

namespace s8::game {

std::optional<std::size_t> find_bytes(std::span<const std::byte> image,
                                      std::span<const std::byte> pattern, std::size_t limit);

} // namespace s8::game
