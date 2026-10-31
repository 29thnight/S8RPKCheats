#pragma once

#include "service_wake.h"
#include "../domain/ownership.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <span>
#include <string_view>
#include <utility>

namespace s8::services {

struct FileLogEntry {
    static constexpr std::size_t TEXT_CAPACITY = 1024;

    std::array<char, TEXT_CAPACITY> text{};
    std::size_t size = 0;
    std::time_t time = 0;
};

// A bounded MPSC channel. Producers never allocate, wait for disk, or wait for
// a contended queue lock; rejected entries remain visible in the drop counter.
class FileLogChannel {
public:
    static constexpr std::size_t CAPACITY = 4096;

    explicit FileLogChannel(ServiceWake& wake);
    void set_enabled(bool value);
    void push(std::string_view text);
    std::size_t take(std::span<FileLogEntry> batch);
    bool pending() const noexcept;
    bool failed() const noexcept;
    std::uint64_t dropped() const noexcept;
    void record_failure(std::size_t entries) noexcept;
    void close() noexcept;

private:
    struct Storage {
        std::array<FileLogEntry, CAPACITY> entries{};
        std::size_t head = 0;
        std::size_t count = 0;
    };

    ServiceWake& wake;
    own::unique_owner<Storage> storage;
    std::mutex mutex;
    std::atomic<std::size_t> queued{0};
    std::atomic<std::uint64_t> discarded{0};
    std::atomic<bool> enabled{false};
    std::atomic<bool> io_failed{false};
    std::atomic<bool> closed{false};
};

// Owned exclusively by the configuration service. The fixed batch is reused
// across drains; all encoding, timestamp formatting, and disk I/O stay here.
class FileLogWriter {
public:
    explicit FileLogWriter(std::filesystem::path path) : path(std::move(path)) {}
    void poll(FileLogChannel& channel);
    void close(FileLogChannel& channel);

private:
    static constexpr std::size_t BATCH_CAPACITY = 64;
    std::filesystem::path path;
    std::ofstream file;
    std::array<FileLogEntry, BATCH_CAPACITY> batch{};
};

} // namespace s8::services
