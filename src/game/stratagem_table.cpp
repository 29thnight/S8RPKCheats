#include "stratagem_table.h"

#include <bit>
#include <cstdint>

namespace s8::game {
namespace {

std::int16_t read_int16(std::span<const std::byte, STRATAGEM_PAYLOAD_SIZE> bytes, std::size_t offset) {
    const auto bits = static_cast<std::uint16_t>(
        std::to_integer<std::uint16_t>(bytes[offset]) |
        (std::to_integer<std::uint16_t>(bytes[offset + 1]) << 8));
    return std::bit_cast<std::int16_t>(bits);
}

void write_int16(StratagemPayload& bytes, std::size_t offset, std::int16_t value) {
    const auto bits = std::bit_cast<std::uint16_t>(value);
    bytes[offset] = static_cast<std::byte>(bits & 0xff);
    bytes[offset + 1] = static_cast<std::byte>(bits >> 8);
}

} // namespace

domain::StratagemRecord decode_stratagem(std::span<const std::byte, STRATAGEM_PAYLOAD_SIZE> payload) {
    return {
        read_int16(payload, 0x00), read_int16(payload, 0x02), read_int16(payload, 0x04),
        read_int16(payload, 0x06), read_int16(payload, 0x08), read_int16(payload, 0x0a),
        read_int16(payload, 0x0c), read_int16(payload, 0x0e), read_int16(payload, 0x10),
        read_int16(payload, 0x12), read_int16(payload, 0x14), read_int16(payload, 0x16),
    };
}

std::array<domain::StratagemRecord, 5> decode_stratagem_table(
    std::span<const std::byte, STRATAGEM_TABLE_SNAPSHOT_SIZE> payloads) {
    std::array<domain::StratagemRecord, 5> records{};
    for (std::size_t i = 0; i < records.size(); ++i) {
        records[i] = decode_stratagem(
            std::span<const std::byte, STRATAGEM_PAYLOAD_SIZE>(
                payloads.data() + i * STRATAGEM_RECORD_STRIDE, STRATAGEM_PAYLOAD_SIZE));
    }
    return records;
}

StratagemPayload encode_stratagem(const domain::StratagemRecord& record) {
    StratagemPayload payload{};
    write_int16(payload, 0x00, record.code1);
    write_int16(payload, 0x02, record.code2);
    write_int16(payload, 0x04, record.code3);
    write_int16(payload, 0x06, record.target);
    write_int16(payload, 0x08, record.effect1);
    write_int16(payload, 0x0a, record.power1);
    write_int16(payload, 0x0c, record.duration1);
    write_int16(payload, 0x0e, record.effect2);
    write_int16(payload, 0x10, record.power2);
    write_int16(payload, 0x12, record.duration2);
    write_int16(payload, 0x14, record.range);
    write_int16(payload, 0x16, record.unknown16);
    return payload;
}

bool is_expected_stratagem_table(const std::array<domain::StratagemRecord, 5>& records) {
    for (std::size_t i = 0; i < records.size(); ++i) {
        const auto expected = static_cast<std::int16_t>(i + 1);
        if (records[i].code1 != expected || records[i].code2 != expected || records[i].code3 != expected)
            return false;
    }

    const auto& s1 = records[0];
    const auto& s2 = records[1];
    const auto& s3 = records[2];
    const auto& s4 = records[3];
    return s1.target == 2 && s1.effect1 == 10 && s1.power1 == -40 && s1.range == 8 &&
           s2.target == 2 && s2.effect1 == 11 && s2.power1 == 200 &&
           s2.effect2 == 12 && s2.power2 == 100 && s2.range == 5 &&
           s3.target == 2 && s3.effect1 == 3 && s3.power1 == 3 &&
           s3.duration1 == 1 && s3.range == 5 &&
           s4.target == 2 && s4.effect1 == 3 && s4.power1 == 2 &&
           s4.duration1 == 3 && s4.range == 6;
}

} // namespace s8::game
