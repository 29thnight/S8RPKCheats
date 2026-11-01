#include "native_preparation.h"

#include "runtime.h"
#include "../game/find_bytes.h"
#include "../game/love_hate_targets.h"
#include "../game/signature_patterns.h"

#include <limits>
#include <optional>
#include <span>

namespace s8::runtime {
namespace {

constexpr std::size_t SEARCH_32_MIB = 0x2000000;
constexpr std::size_t SEARCH_48_MIB = 0x3000000;

std::uintptr_t find_address(std::uintptr_t module_base, std::span<const std::byte> image,
    std::span<const std::byte> pattern, std::size_t limit) {
    const std::optional<std::size_t> offset = game::find_bytes(image, pattern, limit);

    return offset ? module_base + *offset : 0;
}

void merge_target(std::uintptr_t& address, bool& signature_prepared,
    std::uintptr_t candidate, bool safe) {
    if (safe) {
        if (!address) {
            address = candidate;
        }
        // A missing target is cached for this image too. Retrying a feature
        // must not start another full-image scan on the window thread.
        signature_prepared = true;
    }
}

} // namespace

NativeSignaturePreparation prepare_native_signature_values(std::uintptr_t module_base,
    std::span<const std::byte> image) {
    NativeSignaturePreparation prepared;

    if (!module_base || image.empty() || image.size() > std::numeric_limits<std::uintptr_t>::max() - module_base) {
        return prepared;
    }
    prepared.module_base = module_base;
    prepared.instant_attitude = find_address(module_base, image,
        game::signatures::INSTANT_ATTITUDE, SEARCH_32_MIB);
    prepared.infinite_gift = find_address(module_base, image,
        game::signatures::INFINITE_GIFT, SEARCH_48_MIB);
    if (!prepared.infinite_gift) {
        prepared.infinite_gift = find_address(module_base, image,
            game::signatures::INFINITE_GIFT_PATCHED, SEARCH_48_MIB);
    }
    prepared.infinite_talk = find_address(module_base, image,
        game::signatures::INFINITE_TALK, SEARCH_48_MIB);
    prepared.instant_loyalty = find_address(module_base, image,
        game::signatures::INSTANT_LOYALTY, SEARCH_48_MIB);
    prepared.fast_relationship = find_address(module_base, image,
        game::signatures::FAST_RELATIONSHIP, image.size());
    prepared.domestics = find_address(module_base, image,
        game::signatures::DOMESTICS, image.size());

    const std::optional<std::size_t> marriage = game::find_bytes(image,
        game::signatures::MARRIAGE_CONDITION, SEARCH_48_MIB);

    if (marriage && *marriage >= 2 &&
        (image[*marriage - 2] == std::byte{0x75} || image[*marriage - 2] == std::byte{0xeb})) {
        prepared.marriage_condition = module_base + *marriage - 2;
    }

    const std::optional<std::size_t> duel = game::find_bytes(image,
        game::signatures::INFINITE_DUEL_DEBATE, SEARCH_48_MIB);

    prepared.infinite_duel_debate = duel ? module_base + *duel + 12 : 0;
    prepared.resonance_select = find_address(module_base, image,
        game::signatures::RESONANCE_SELECT, image.size());
    prepared.resonance_getter = find_address(module_base, image,
        game::signatures::RESONANCE_GETTER, image.size());

    const game::LoveHateTargets targets = game::find_love_hate_targets(image);

    prepared.affinity = targets.affinity ? module_base + *targets.affinity : 0;
    prepared.relationship = targets.relationship ? module_base + *targets.relationship : 0;
    prepared.relationship_offset = targets.relationship_offset.value_or(0);
    return prepared;
}

void merge_native_signature_values(Runtime& runtime, const NativeSignaturePreparation& prepared) {
    if (runtime.stopping || !prepared.module_base || runtime.image_module != prepared.module_base ||
        runtime.module_image.empty()) {
        return;
    }

    // A fresh image may contain our installed jumps. Never use a failed search
    // in that image to erase a target needed by a retained patch or its undo.
    merge_target(runtime.instant_attitude.address, runtime.instant_attitude.signature_prepared,
        prepared.instant_attitude, !runtime.instant_attitude_config_applied);
    merge_target(runtime.infinite_gift.code_address, runtime.infinite_gift.signature_prepared,
        prepared.infinite_gift, !runtime.infinite_gift.applied);
    merge_target(runtime.infinite_talk.hook, runtime.infinite_talk.signature_prepared,
        prepared.infinite_talk, !runtime.infinite_talk.applied && !runtime.infinite_talk.cave);
    merge_target(runtime.instant_loyalty.address, runtime.instant_loyalty.signature_prepared,
        prepared.instant_loyalty, !runtime.instant_loyalty.applied && !runtime.instant_loyalty.cave);
    merge_target(runtime.fast_relationship.address, runtime.fast_relationship.signature_prepared,
        prepared.fast_relationship, !runtime.fast_relationship.applied);
    merge_target(runtime.domestics_patch.address, runtime.domestics_patch.signature_prepared,
        prepared.domestics, !runtime.domestics_patch.applied && !runtime.domestics_patch.cave);
    merge_target(runtime.marriage_condition.address, runtime.marriage_condition.signature_prepared,
        prepared.marriage_condition, !runtime.marriage_condition.applied && !runtime.marriage_condition.original_saved);
    merge_target(runtime.infinite_duel_debate.hook, runtime.infinite_duel_debate.signature_prepared,
        prepared.infinite_duel_debate, !runtime.infinite_duel_debate.applied && !runtime.infinite_duel_debate.cave);

    game::InstantResonance& resonance = runtime.instant_resonance;
    if (!resonance.select_applied && !resonance.getter_applied && !resonance.select_cave && !resonance.getter_cave) {
        merge_target(resonance.select_address, resonance.signature_prepared, prepared.resonance_select, true);
        merge_target(resonance.getter_address, resonance.signature_prepared, prepared.resonance_getter, true);
    }

    game::LoveHate& love_hate = runtime.love_hate;
    if (!love_hate.affinity_applied && !love_hate.relationship_applied &&
        !love_hate.affinity_cave && !love_hate.relationship_cave) {
        merge_target(love_hate.affinity_address, love_hate.signature_prepared, prepared.affinity, true);
        // The displacement belongs to the relationship instruction. Preserve
        // an existing displacement rather than joining incompatible results.
        if (!love_hate.relationship_offset || love_hate.relationship_offset == prepared.relationship_offset) {
            merge_target(love_hate.relationship_address, love_hate.signature_prepared, prepared.relationship, true);
            if (love_hate.relationship_address && love_hate.relationship_address == prepared.relationship &&
                !love_hate.relationship_offset) {
                love_hate.relationship_offset = prepared.relationship_offset;
            }
        }
    }
}

void prepare_native_signatures(Runtime& runtime, std::uintptr_t module_base) {
    merge_native_signature_values(runtime, prepare_native_signature_values(module_base, runtime.module_image));
}

} // namespace s8::runtime
