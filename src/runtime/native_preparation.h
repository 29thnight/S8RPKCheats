#pragma once

#include <cstdint>

namespace s8::runtime {

struct Runtime;

// Call once on the coordinator, before publishing Runtime to any callback.
// Only scans the retained immutable image; does not patch live game memory.
void prepare_native_signatures(Runtime& runtime, std::uintptr_t module_base);

} // namespace s8::runtime
