#include "battle_state.h"

#include "../platform/windows/process_memory.h"

#include <array>
#include <span>

namespace s8::game {
namespace {

bool read_pointer(std::uintptr_t address, std::uintptr_t& value) {
    return platform::windows::read_memory(address, std::as_writable_bytes(std::span(&value, 1))) && value >= 0x10000;
}

std::optional<NativeBattleState> native_battle_state(std::uintptr_t unit, std::uintptr_t module_base) {
    std::uintptr_t root = 0, impl = 0, owner = 0, vtable = 0, native = 0;
    if (!read_pointer(unit + 0x10, root) || !read_pointer(root, impl) ||
        !read_pointer(impl, owner) || owner != root) {
        return std::nullopt;
    }
    const auto machine = impl + 0x58;
    if (!read_pointer(machine, vtable) || vtable != module_base + 0x02707a60 ||
        !read_pointer(machine + 0x48, owner) || owner != root ||
        !read_pointer(machine + 0x30, native) ||
        !read_pointer(native + 0x10, owner) || owner != machine ||
        !read_pointer(native + 0x18, owner) || owner != root) {
        return std::nullopt;
    }
    NativeBattleState result{};
    if (!platform::windows::read_memory(native + 8, std::as_writable_bytes(std::span(&result.id, 1))) ||
        !platform::windows::read_memory(impl + 0x18038, std::as_writable_bytes(std::span(&result.meeting, 1))) ||
        result.id > 20 || result.meeting > 1) {
        return std::nullopt;
    }
    return result;
}

} // namespace

BattleMode classify_battle_state(BattleState& state, bool session_ready, int game_state,
                                 std::optional<NativeBattleState> native, std::uint64_t now_ms) {
    state.mode = BattleMode::Unknown;
    state.game_state = game_state;
    state.native_present = native.has_value();
    if (session_ready && native) {
        state.missing_since_ms = 0;
        if (native->meeting == 1 || native->id >= 19) {
            state.mode = BattleMode::NonBattle;
        } else if (native->id >= 3) {
            state.mode = BattleMode::Battle;
        }
    } else if (session_ready && (game_state == 5 || game_state == 7)) {
        if (!state.missing_since_ms) {
            state.missing_since_ms = now_ms;
        }
        if (now_ms - state.missing_since_ms >= 600) {
            state.mode = BattleMode::NonBattle;
        }
    } else {
        state.missing_since_ms = 0;
    }
    return state.mode;
}

BattleMode sample_battle_state(BattleState& state, std::uintptr_t module_base,
                               std::uintptr_t game_base, std::uint64_t now_ms) {
    if (state.last_sample_ms && now_ms - state.last_sample_ms < 200) {
        return state.mode;
    }
    state.last_sample_ms = now_ms;
    std::uint8_t game_state = 0xff;
    std::uintptr_t player = 0;
    const bool session_ready = game_base &&
        read_pointer(game_base + 0xe0, player) && platform::windows::is_committed_memory(player, 0x200);
    if (game_base) {
        platform::windows::read_memory(game_base + 0xd0, std::as_writable_bytes(std::span(&game_state, 1)));
    }
    std::optional<NativeBattleState> native;
    if (game_base && game_state != 0 && module_base) {
        auto address = module_base + 0x02e99460;
        constexpr std::array<std::uintptr_t, 6> OFFSETS{0x28, 0x250, 0x1d8, 0, 0x180, 0};
        for (const auto offset : OFFSETS) {
            std::uintptr_t next = 0;
            if (!read_pointer(address, next)) {
                address = 0;
                break;
            }
            address = next + offset;
        }
        std::uint8_t count = 0;
        if (address && platform::windows::read_memory(address - 8, std::as_writable_bytes(std::span(&count, 1))) &&
            count > 0 && count <= 60) {
            for (std::size_t i = 0; i < count && i < 4; ++i) {
                std::uintptr_t unit = 0;
                if (read_pointer(address + 8 + i * 0x10, unit)) {
                    native = native_battle_state(unit, module_base);
                    if (native) {
                        break;
                    }
                }
            }
        }
    }
    return classify_battle_state(state, session_ready, game_state, native, now_ms);
}

} // namespace s8::game
