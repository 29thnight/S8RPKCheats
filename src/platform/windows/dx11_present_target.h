#pragma once

#include <expected>

namespace s8::platform::windows {

struct Dx11PresentTargets {
    void* present = nullptr;
    void* resize_buffers = nullptr;
};

std::expected<Dx11PresentTargets, int> find_dx11_present_targets();
std::expected<void*, int> find_dx11_present_target();

} // namespace s8::platform::windows
