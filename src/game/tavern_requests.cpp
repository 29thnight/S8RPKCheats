#include "tavern_requests.h"

#include "../platform/windows/process_memory.h"

#include <array>
#include <span>

namespace s8::game {
namespace {

template <typename T>
bool read_value(std::uintptr_t address, T& value) {
    return platform::windows::read_memory(address, std::as_writable_bytes(std::span(&value, 1)));
}

std::uintptr_t slot_address(std::uintptr_t base, int city, int slot) {
    return base + static_cast<std::uintptr_t>(city) * 0x2a0 + 0x178 + static_cast<std::uintptr_t>(slot) * 0x28;
}

bool usable(const TavernRequest& request) {
    return request.name > 0x10000 && request.data > 0x10000 &&
        platform::windows::is_committed_memory(request.name, 1) &&
        platform::windows::is_committed_memory(request.data, 1);
}

bool read_pair(std::uintptr_t address, TavernRequest& request) {
    return read_value(address, request.name) && read_value(address + 8, request.data);
}

bool write_pair(std::uintptr_t address, const TavernRequest& request) {
    if (!usable(request)) {
        return false;
    }
    TavernRequest previous{};
    if (!read_pair(address, previous)) {
        return false;
    }
    const std::array<std::uintptr_t, 2> desired{request.name, request.data};
    TavernRequest actual{};
    if (platform::windows::write_data_memory(address, std::as_bytes(std::span(desired))) &&
        read_pair(address, actual) && actual == request) {
        return true;
    }
    const std::array<std::uintptr_t, 2> original{previous.name, previous.data};
    platform::windows::write_data_memory(address, std::as_bytes(std::span(original)));
    return false;
}

bool source_live(const TavernRequests& state, int city, int slot, const TavernRequest& expected) {
    TavernRequest actual{};
    return read_pair(slot_address(state.city_base, city, slot), actual) && actual == expected && usable(actual) &&
           !(state.injected[city][slot] == actual);
}

void maintain(TavernRequests& state, bool prefer_live) {
    struct Source { int city; int slot; TavernRequest request; };
    std::array<Source, 204> sources{};
    std::size_t source_count = 0;
    std::array<std::array<TavernRequest, 4>, 51> current{};
    std::array<std::array<bool, 4>, 51> read{};
    for (int city = 0; city < 51; ++city) {
        for (int slot = 0; slot < 4; ++slot) {
            auto& request = current[city][slot];
            if (!read_pair(slot_address(state.city_base, city, slot), request)) {
                continue;
            }
            read[city][slot] = true;
            if (usable(request)) {
                if (!(state.injected[city][slot] == request)) {
                    state.backups[city][slot] = {request, -1, -1, false};
                    state.injected[city][slot] = {};
                    sources[source_count++] = {city, slot, request};
                }
            } else if (!request.name && !request.data) {
                state.injected[city][slot] = {};
            }
        }
    }
    for (int city = 0; city < 51; ++city) {
        for (int slot = 0; slot < 4; ++slot) {
            if (!read[city][slot]) {
                continue;
            }
            auto& request = current[city][slot];
            const auto address = slot_address(state.city_base, city, slot);
            const auto duplicate = [&](std::uintptr_t data) {
                for (int other = 0; other < 4; ++other) {
                    if (other != slot && current[city][other].data == data) {
                        return true;
                    }
                }
                return false;
            };
            const auto fill_live = [&] {
                for (std::size_t i = 0; i < source_count; ++i) {
                    const auto& source = sources[i];
                    if (source.city == city || duplicate(source.request.data) ||
                        !source_live(state, source.city, source.slot, source.request)) {
                        continue;
                    }
                    if (!write_pair(address, source.request)) {
                        return false;
                    }
                    request = source.request;
                    state.injected[city][slot] = source.request;
                    state.backups[city][slot] = {source.request, source.city, source.slot, true};
                    return true;
                }
                return false;
            };
            if (!request.name && !request.data) {
                if (prefer_live && fill_live()) {
                    continue;
                }
                const auto& backup = state.backups[city][slot];
                if (usable(backup.request) && !duplicate(backup.request.data) &&
                    (!backup.borrowed || source_live(state, backup.source_city, backup.source_slot, backup.request)) &&
                    write_pair(address, backup.request)) {
                    request = backup.request;
                    state.injected[city][slot] = backup.request;
                    continue;
                }
                fill_live();
            } else if (usable(request) && state.injected[city][slot] == request) {
                const auto& backup = state.backups[city][slot];
                if (backup.borrowed && !source_live(state, backup.source_city, backup.source_slot, backup.request)) {
                    fill_live();
                }
            }
        }
    }
}

} // namespace

void update_tavern_requests(TavernRequests& state, std::uintptr_t module_base,
                            std::uintptr_t game_base, std::uint64_t now_ms, bool enabled) {
    if (now_ms - state.last_tick_ms < 500) {
        return;
    }
    state.last_tick_ms = now_ms;
    if (!enabled) {
        if (state.enabled) {
            state = {};
        }
        return;
    }
    if (!state.enabled) {
        state = {};
        state.enabled = true;
        state.last_tick_ms = now_ms;
    }
    std::uintptr_t city_base = 0;
    if (!module_base || !read_value(module_base + 0x034c8630, city_base) || city_base <= 0x10000 ||
        !read_value(city_base, city_base) || city_base <= 0x10000 ||
        !read_value(city_base, city_base) || city_base <= 0x10000) {
        return;
    }
    if (state.city_base && state.city_base != city_base) {
        state = {};
        state.city_base = city_base;
        state.enabled = true;
        state.last_tick_ms = now_ms;
        return;
    }
    state.city_base = city_base;
    std::uint8_t phase = 0;
    if (game_base <= 0x10000 || !read_value(game_base + 0xd0, phase) || (phase != 5 && phase != 7)) {
        return;
    }
    if (!state.last_phase) {
        state.last_phase = phase;
        if (phase == 7) {
            maintain(state, true);
        }
        return;
    }
    if (phase != state.last_phase) {
        const auto previous = state.last_phase;
        state.last_phase = phase;
        if (previous == 5 && phase == 7) {
            state.phase_change_ms = now_ms;
            state.waiting = true;
            return;
        }
        if (phase == 5) {
            state.waiting = false;
            return;
        }
    }
    if (phase != 7) {
        return;
    }
    if (state.waiting) {
        if (now_ms - state.phase_change_ms < 2500) {
            return;
        }
        state.waiting = false;
        maintain(state, true);
        return;
    }
    maintain(state, false);
}

} // namespace s8::game
