#include "native_preparation.h"

#include "runtime.h"
#include "../game/find_bytes.h"
#include "../game/love_hate_targets.h"
#include "../game/signature_patterns.h"

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

} // namespace

void prepare_native_signatures(Runtime& runtime, std::uintptr_t module_base) {
    if (!module_base || runtime.module_image.empty()) {
        return;
    }
    const std::span<const std::byte> image = runtime.module_image;

    // A missing signature is also a completed result. Repeated feature retries
    // must not rescan tens of megabytes on the game window thread.
    runtime.instant_attitude.address = find_address(module_base, image,
        game::signatures::INSTANT_ATTITUDE, SEARCH_32_MIB);
    runtime.instant_attitude.signature_prepared = true;
    runtime.infinite_gift.code_address = find_address(module_base, image,
        game::signatures::INFINITE_GIFT, SEARCH_48_MIB);
    if (!runtime.infinite_gift.code_address) {
        runtime.infinite_gift.code_address = find_address(module_base, image,
            game::signatures::INFINITE_GIFT_PATCHED, SEARCH_48_MIB);
    }
    runtime.infinite_gift.signature_prepared = true;
    runtime.infinite_talk.hook = find_address(module_base, image,
        game::signatures::INFINITE_TALK, SEARCH_48_MIB);
    runtime.infinite_talk.signature_prepared = true;
    runtime.instant_loyalty.address = find_address(module_base, image,
        game::signatures::INSTANT_LOYALTY, SEARCH_48_MIB);
    runtime.instant_loyalty.signature_prepared = true;
    runtime.fast_relationship.address = find_address(module_base, image,
        game::signatures::FAST_RELATIONSHIP, image.size());
    runtime.fast_relationship.signature_prepared = true;
    runtime.domestics_patch.address = find_address(module_base, image,
        game::signatures::DOMESTICS, image.size());
    runtime.domestics_patch.signature_prepared = true;

    const std::optional<std::size_t> marriage = game::find_bytes(image,
        game::signatures::MARRIAGE_CONDITION, SEARCH_48_MIB);

    if (marriage && *marriage >= 2 &&
        (image[*marriage - 2] == std::byte{0x75} || image[*marriage - 2] == std::byte{0xeb})) {
        runtime.marriage_condition.address = module_base + *marriage - 2;
    }
    runtime.marriage_condition.signature_prepared = true;

    const std::optional<std::size_t> duel = game::find_bytes(image,
        game::signatures::INFINITE_DUEL_DEBATE, SEARCH_48_MIB);

    runtime.infinite_duel_debate.hook = duel ? module_base + *duel + 12 : 0;
    runtime.infinite_duel_debate.signature_prepared = true;
    runtime.instant_resonance.select_address = find_address(module_base, image,
        game::signatures::RESONANCE_SELECT, image.size());
    runtime.instant_resonance.getter_address = find_address(module_base, image,
        game::signatures::RESONANCE_GETTER, image.size());
    runtime.instant_resonance.signature_prepared = true;

    const game::LoveHateTargets targets = game::find_love_hate_targets(image);

    runtime.love_hate.affinity_address = targets.affinity ? module_base + *targets.affinity : 0;
    runtime.love_hate.relationship_address = targets.relationship ? module_base + *targets.relationship : 0;
    runtime.love_hate.relationship_offset = targets.relationship_offset.value_or(0);
    runtime.love_hate.signature_prepared = true;
}

} // namespace s8::runtime
