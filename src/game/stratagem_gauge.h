#pragma once

#include "native_patch.h"

namespace s8::game {

struct StratagemGauge {
    NativePatchGroup patches{};
    std::uintptr_t module_base = 0;
    std::uintptr_t cave = 0;
    bool attack = false;
    bool defense = false;
};

bool set_stratagem_gauge(StratagemGauge& state, std::uintptr_t module_base,
    std::span<const std::byte> image, bool attack, bool defense);

} // namespace s8::game
