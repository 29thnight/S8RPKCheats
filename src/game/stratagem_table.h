#pragma once

#include "../domain/stratagem5.h"

#include <array>
#include <cstddef>
#include <span>

namespace s8::game {

inline constexpr std::size_t STRATAGEM_PAYLOAD_SIZE = 0x18;
inline constexpr std::size_t STRATAGEM_RECORD_STRIDE = 0x20;
inline constexpr std::size_t STRATAGEM_TABLE_SNAPSHOT_SIZE = 4 * STRATAGEM_RECORD_STRIDE + STRATAGEM_PAYLOAD_SIZE;
using StratagemPayload = std::array<std::byte, STRATAGEM_PAYLOAD_SIZE>;

domain::StratagemRecord decode_stratagem(std::span<const std::byte, STRATAGEM_PAYLOAD_SIZE> payload);
std::array<domain::StratagemRecord, 5> decode_stratagem_table(
    std::span<const std::byte, STRATAGEM_TABLE_SNAPSHOT_SIZE> payloads);
StratagemPayload encode_stratagem(const domain::StratagemRecord& record);
bool is_expected_stratagem_table(const std::array<domain::StratagemRecord, 5>& records);

} // namespace s8::game
