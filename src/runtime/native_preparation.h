#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace s8::runtime {

struct Runtime;

struct NativeSignaturePreparation {
    std::uintptr_t module_base = 0;
    std::uintptr_t instant_attitude = 0;
    std::uintptr_t infinite_gift = 0;
    std::uintptr_t infinite_talk = 0;
    std::uintptr_t instant_loyalty = 0;
    std::uintptr_t fast_relationship = 0;
    std::uintptr_t domestics = 0;
    std::uintptr_t marriage_condition = 0;
    std::uintptr_t infinite_duel_debate = 0;
    std::uintptr_t resonance_select = 0;
    std::uintptr_t resonance_getter = 0;
    std::uintptr_t affinity = 0;
    std::uintptr_t relationship = 0;
    std::uint32_t relationship_offset = 0;
};

// Pure scan of a worker-owned image. No Runtime or live game memory access.
NativeSignaturePreparation prepare_native_signature_values(std::uintptr_t module_base,
    std::span<const std::byte> image);
// Serialized owner only, after adopting the same module's image. Fill safe
// missing targets without replacing live hooks, caves, originals or journals.
void merge_native_signature_values(Runtime& runtime, const NativeSignaturePreparation& prepared);
// Startup coordinator only, before publishing Runtime to any callback.
// The recovery path scans on its worker and invokes only merge on the owner.
void prepare_native_signatures(Runtime& runtime, std::uintptr_t module_base);

} // namespace s8::runtime
