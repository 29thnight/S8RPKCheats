#include "file_log.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <utility>

namespace s8::services {

FileLogChannel::FileLogChannel(ServiceWake& wake)
    : wake(wake), storage(own::make_unique<Storage>()) {}

void FileLogChannel::set_enabled(bool value) {
    if (enabled.exchange(value, std::memory_order_acq_rel) != value) {
        if (value) {
            io_failed.store(false, std::memory_order_release);
        }
        wake.signal();
    }
}

void FileLogChannel::push(std::string_view text) {
    if (!enabled.load(std::memory_order_acquire) || closed.load(std::memory_order_acquire)) {
        return;
    }
    if (io_failed.load(std::memory_order_acquire)) {
        discarded.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    const std::time_t time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::size_t size = std::min(text.size(), FileLogEntry::TEXT_CAPACITY - 1);

    // A truncated UTF-8 line must end before a partial code point.
    if (size < text.size()) {
        while (size && (static_cast<unsigned char>(text[size]) & 0xc0) == 0x80) {
            --size;
        }
    }
    {
        std::unique_lock<std::mutex> lock(mutex, std::try_to_lock);

        if (!lock.owns_lock() || storage->count == CAPACITY || closed.load(std::memory_order_acquire)) {
            discarded.fetch_add(1, std::memory_order_relaxed);
            return;
        }

        FileLogEntry& entry = storage->entries[(storage->head + storage->count) % CAPACITY];

        std::memcpy(entry.text.data(), text.data(), size);
        entry.text[size] = '\0';
        entry.size = size;
        entry.time = time;
        ++storage->count;
        queued.store(storage->count, std::memory_order_release);
    }
    wake.signal();
}

std::size_t FileLogChannel::take(std::span<FileLogEntry> batch) {
    std::lock_guard<std::mutex> lock(mutex);
    const std::size_t size = std::min(storage->count, batch.size());

    for (std::size_t index = 0; index < size; ++index) {
        batch[index] = storage->entries[(storage->head + index) % CAPACITY];
    }
    storage->head = (storage->head + size) % CAPACITY;
    storage->count -= size;
    queued.store(storage->count, std::memory_order_release);
    return size;
}

bool FileLogChannel::pending() const noexcept {
    return queued.load(std::memory_order_acquire) != 0;
}

bool FileLogChannel::failed() const noexcept {
    return io_failed.load(std::memory_order_acquire);
}

std::uint64_t FileLogChannel::dropped() const noexcept {
    return discarded.load(std::memory_order_acquire);
}

void FileLogChannel::record_failure(std::size_t entries) noexcept {
    io_failed.store(true, std::memory_order_release);
    discarded.fetch_add(entries, std::memory_order_relaxed);
}

void FileLogChannel::close() noexcept {
    closed.store(true, std::memory_order_release);
}

void FileLogWriter::poll(FileLogChannel& channel) {
    const std::size_t count = channel.take(batch);

    if (!count) {
        return;
    }
    if (channel.failed()) {
        channel.record_failure(count);
        return;
    }
    if (!file.is_open()) {
        file.clear();
        file.open(path, std::ios::app | std::ios::binary | std::ios::ate);
        if (file.is_open() && file.tellp() == std::streampos(0)) {
            constexpr std::array<char, 3> UTF8_BOM{char(0xef), char(0xbb), char(0xbf)};

            file.write(UTF8_BOM.data(), static_cast<std::streamsize>(UTF8_BOM.size()));
        }
    }
    for (std::size_t index = 0; index < count && file; ++index) {
        const FileLogEntry& entry = batch[index];
        std::tm local{};
        std::array<char, 32> prefix{};

        localtime_s(&local, &entry.time);
        const std::size_t size = std::strftime(prefix.data(), prefix.size(), "[%Y-%m-%d %H:%M:%S] ", &local);

        file.write(prefix.data(), static_cast<std::streamsize>(size));
        file.write(entry.text.data(), static_cast<std::streamsize>(entry.size));
        file.write("\r\n", 2);
    }
    file.flush();
    if (!file) {
        channel.record_failure(count);
        if (file.is_open()) {
            file.close();
        }
    }
}

void FileLogWriter::close(FileLogChannel& channel) {
    channel.close();
    while (channel.pending()) {
        poll(channel);
    }
    if (file.is_open()) {
        file.flush();
        if (!file) {
            channel.record_failure(0);
        }
        file.close();
    }
}

} // namespace s8::services
