#pragma once

#include "ids.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace s8::domain {

inline constexpr std::size_t DEBUG_VIEW_BYTES = 4096;
inline constexpr std::size_t DEBUG_RESULT_PAGE = 128;
inline constexpr std::size_t DEBUG_RESULT_LIMIT = 100001;

enum class MemoryValueType { Byte, Word, Dword, Qword, Float, Double };
enum class DebugAction { Read, Capture, Write, Search, Refine, Cancel, ResultPage, ClearCapture };
enum class DebugStatus { Ready, Scanning, Complete, Cancelled, SessionChanged, InvalidRequest, ReadFailed, WriteFailed, Conflict };

struct MemoryValue {
    std::array<std::byte, 8> bytes{};
    MemoryValueType type = MemoryValueType::Dword;
};

struct DebugRequest {
    std::uint64_t id = 0;
    GameEpoch epoch{};
    DebugAction action = DebugAction::Read;
    std::uintptr_t address = 0;
    std::size_t count = 16;
    MemoryValue value;
    std::int32_t offset = 0;
    std::uint16_t radius = 0;
    std::byte expected{};
    std::byte replacement{};
};

struct MemoryMatch {
    std::uintptr_t address = 0;
    std::array<std::byte, 8> value{};
};

struct DebugSnapshot {
    GameEpoch epoch{};
    std::uint64_t revision = 0;
    std::uint64_t completed_request = 0;
    DebugStatus status = DebugStatus::Ready;
    DebugStatus request_status = DebugStatus::Ready;
    std::uintptr_t module = 0;
    std::uintptr_t game_base = 0;
    std::uintptr_t player = 0;
    std::uintptr_t view_address = 0;
    std::size_t view_size = 0;
    std::array<std::byte, DEBUG_VIEW_BYTES> bytes{};
    std::array<bool, DEBUG_VIEW_BYTES> readable{};
    std::uintptr_t capture_address = 0;
    std::size_t capture_size = 0;
    std::array<std::byte, DEBUG_VIEW_BYTES> capture{};
    std::array<bool, DEBUG_VIEW_BYTES> capture_readable{};
    MemoryValueType scan_type = MemoryValueType::Dword;
    bool scanning = false;
    bool refining = false;
    bool result_limit_reached = false;
    std::uint64_t scanned_bytes = 0;
    std::size_t matches = 0;
    std::size_t page = 0;
    std::size_t page_count = 0;
    std::array<MemoryMatch, DEBUG_RESULT_PAGE> results{};
};

constexpr std::size_t memory_value_size(MemoryValueType type) noexcept {
    switch (type) {
    case MemoryValueType::Byte: return 1;
    case MemoryValueType::Word: return 2;
    case MemoryValueType::Dword: case MemoryValueType::Float: return 4;
    case MemoryValueType::Qword: case MemoryValueType::Double: return 8;
    }
    return 0;
}

bool parse_memory_value(MemoryValueType type, std::string_view text, MemoryValue& output);

} // namespace s8::domain
