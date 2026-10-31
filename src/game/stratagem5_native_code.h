#pragma once

#include "native_patch.h"

namespace s8::game::stratagem5_native_detail {

// The generated x64 readers admit only the Open byte. Abandoned is terminal.
enum class Admission : std::uint8_t { Closed = 0, Open = 1, Abandoned = 2 };

struct EntryPoints {
    std::uintptr_t init_layouts = 0;
    std::uintptr_t post_buttons = 0;
    std::uintptr_t before_callbacks = 0;
    std::uintptr_t after_reset = 0;
    std::uintptr_t before_open_button = 0;
    std::uintptr_t admission = 0;
    std::uintptr_t active_layout = 0;
    std::uintptr_t active_button = 0;
    std::uintptr_t attack_owner = 0;
    std::uintptr_t defense_owner = 0;
};

struct Code {
    NativePatchGroup patches;
    // Bounds are a second transaction after every safe load bridge exists.
    // Restore these first; a failed bound rollback must never remove its load.
    NativePatchGroup bounds;
    std::array<std::uintptr_t, 8> caves{};
    std::uintptr_t module = 0;
    std::uintptr_t original_init_layouts = 0;
    bool published = false;
};

bool supported_image(std::span<const std::byte> image);
bool prepare_code(Code& state, std::uintptr_t module_base,
    std::span<const std::byte> image, const EntryPoints& entries);
// Only unpublished allocations can be reclaimed. Published caves remain valid
// for a native return address even after the entry transaction is restored.
void discard_unpublished_code(Code& state);

} // namespace s8::game::stratagem5_native_detail
