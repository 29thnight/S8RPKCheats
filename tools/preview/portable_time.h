#pragma once

// FileLogWriter is linked with its channel, but never started by the preview.
// Adapt its Windows C-runtime spelling only in this non-Windows target.
#include <cerrno>
#include <ctime>

inline int localtime_s(std::tm* output, const std::time_t* input) {
    return localtime_r(input, output) ? 0 : errno;
}
