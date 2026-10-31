#pragma once

#include <cstdint>
#include <expected>

namespace s8::domain {

enum class ErrorCode {
    InvalidData,
    StaleEpoch,
    UnsupportedVersion,
    QueueFull,
    Cancelled,
    PlatformFailure,
};

struct Error {
    ErrorCode code;
    std::uint32_t native_code{};
};

template <class T>
using Result = std::expected<T, Error>;

} // namespace s8::domain
