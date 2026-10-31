#include "isolated_territory.h"
#include "../platform/windows/code_memory.h"
#include "../platform/windows/process_memory.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <algorithm>
#include <atomic>

namespace s8::game {
namespace {

// WOOSEOK99/S8RPKCheats@068ca1d881edf870f7cf06f935cf50e286004af8,
// War/IsolatedTerritoryMovement.h and IsolatedTerritoryMovementFeature.h.
// The graph/list policies and native signatures below preserve the original.
// The effective wrapper's four bool consumers use TEST AL, not TEST EAX.
std::atomic<std::uintptr_t> callback_base{0};

template<typename T>
bool read_value(std::uintptr_t address, T& value) {
    return platform::windows::read_memory(address,
        std::as_writable_bytes(std::span<T>(&value, 1)));
}

bool valid_game_object(std::uintptr_t object) {
  if (!object) {
    return false;
  }

  std::uintptr_t vtable = 0;
  std::uintptr_t validate = 0;
  if (!read_value(object, vtable) || !vtable ||
      !read_value(vtable + 0x48, validate) || !validate) {
    return false;
  }

  using ValidateFn = std::uint8_t(__fastcall*)(std::uintptr_t);
  __try {
    return reinterpret_cast<ValidateFn>(validate)(object) != 0;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
}

std::uintptr_t owner_root(std::uintptr_t object) {
  std::uintptr_t holder = 0;
  if (!object || !read_value(object + 0x90, holder) ||
      !valid_game_object(holder)) {
    return 0;
  }

  std::uintptr_t root = 0;
  if (!read_value(holder + 0x10, root) || !valid_game_object(root)) {
    return 0;
  }
  return root;
}

bool territories_connected(std::uintptr_t source, std::uintptr_t destination) {
  if (!valid_game_object(source) || !valid_game_object(destination)) {
    return false;
  }

  const std::uintptr_t owner = owner_root(source);
  if (!owner || owner_root(destination) != owner) {
    return false;
  }

  std::array<std::uintptr_t, 256> queue{};
  std::size_t count = 1;
  std::size_t cursor = 0;
  queue[0] = source;

  while (cursor < count) {
    const std::uintptr_t current = queue[cursor++];
    if (current == destination) {
      return true;
    }

    for (std::size_t i = 0; i < 6; ++i) {
      std::uintptr_t neighbor = 0;
      if (!read_value(current + 0x20 + i * sizeof(std::uintptr_t), neighbor) ||
          !valid_game_object(neighbor) || owner_root(neighbor) != owner) {
        continue;
      }

      bool duplicate = false;
      for (std::size_t j = 0; j < count; ++j) {
        if (queue[j] == neighbor) {
          duplicate = true;
          break;
        }
      }
      if (duplicate) {
        continue;
      }

      if (count >= queue.size()) {
        return false;
      }
      queue[count++] = neighbor;
    }
  }
  return false;
}

std::uintptr_t list_node(std::uintptr_t listRef) {
  if (!listRef || !callback_base.load(std::memory_order_acquire)) {
    return 0;
  }

  std::uintptr_t presence = 0;
  if (!read_value(callback_base.load(std::memory_order_acquire) + 0x34C8628, presence) || !presence) {
    return 0;
  }

  std::uintptr_t indexPtr = 0;
  std::uintptr_t table = 0;
  std::uint32_t count = 0;
  if (!read_value(listRef + 0x08, indexPtr) || !indexPtr ||
      !read_value(callback_base.load(std::memory_order_acquire) + 0x34C8630, table) || !table ||
      !read_value(callback_base.load(std::memory_order_acquire) + 0x34C8660, count)) {
    return 0;
  }

  std::uint32_t index = 0;
  if (!read_value(indexPtr, index) || index >= count) {
    return 0;
  }

  std::uintptr_t node = 0;
  if (!read_value(table + static_cast<std::uintptr_t>(index) * sizeof(std::uintptr_t), node)) {
    return 0;
  }
  return node;
}

bool erase_list_object(std::uintptr_t listRef, std::uintptr_t object) {
  if (!callback_base.load(std::memory_order_acquire)) {
    return false;
  }
  using EraseFn = void(__fastcall*)(std::uintptr_t, std::uintptr_t*);
  std::uintptr_t local = object;
  __try {
    reinterpret_cast<EraseFn>(callback_base.load(std::memory_order_acquire) + 0x0001c5a0)(listRef, &local);
    return true;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
}

bool filter_connected_list(std::uintptr_t listRef, std::uintptr_t target) {
  std::uintptr_t node = list_node(listRef);
  std::uintptr_t previous = 0;
  std::uint32_t guard = 0x20000;

  while (node) {
    if (guard-- == 0) {
      return false;
    }

    std::uintptr_t object = 0;
    if (!read_value(node, object)) {
      return false;
    }

    bool keep = valid_game_object(object);
    if (keep) {
      std::uintptr_t city = 0;
      keep = read_value(object + 0x20, city) && territories_connected(city, target);
    }

    if (!keep) {
      if (!erase_list_object(listRef, object)) {
        return false;
      }

      if (previous) {
        if (!read_value(previous + 0x08, node)) {
          return false;
        }
      } else {
        node = list_node(listRef);
      }
      continue;
    }

    previous = node;
    if (!read_value(node + 0x08, node)) {
      return false;
    }
  }

  return list_node(listRef) != 0;
}

bool movement_context_allowed(std::uintptr_t context,
                                   std::uintptr_t destination,
                                   std::uintptr_t owner) {
  if (!valid_game_object(context) || !valid_game_object(destination) || !owner) {
    return true;
  }

  std::uintptr_t contextOwner = 0;
  if (!read_value(context + 0x18, contextOwner) || contextOwner != owner) {
    return true;
  }

  std::uintptr_t source = 0;
  if (!read_value(context + 0x20, source) || !valid_game_object(source)) {
    return true;
  }

  const std::uintptr_t sourceOwner = owner_root(source);
  if (sourceOwner != owner || owner_root(destination) != sourceOwner) {
    return true;
  }

  return territories_connected(source, destination);
}

using MovementCheckFn = std::uint8_t(__fastcall*)(std::uintptr_t, std::uintptr_t,
                                               std::uintptr_t, std::uintptr_t,
                                               std::uint32_t);

std::uint8_t __fastcall movement_check_78(std::uintptr_t arg1,
                                          std::uintptr_t destination,
                                          std::uintptr_t arg3,
                                          std::uintptr_t arg4,
                                          std::uint32_t arg5) {
  const MovementCheckFn native = reinterpret_cast<MovementCheckFn>(callback_base.load(std::memory_order_acquire) + 0x01902120);
  const std::uint8_t result = native(arg1, destination, arg3, arg4, arg5);
  if (!result || arg5 != 0 || !destination) {
    return result;
  }

  std::uintptr_t source = 0;
  if (!read_value(arg1 + 0x20, source)) {
    return result;
  }
  return territories_connected(source, destination) ? 1 : 0;
}

std::uint8_t __fastcall movement_check_910(std::uintptr_t arg1,
                                           std::uintptr_t destination,
                                           std::uintptr_t arg3,
                                           std::uintptr_t arg4,
                                           std::uint32_t arg5) {
  const MovementCheckFn native = reinterpret_cast<MovementCheckFn>(callback_base.load(std::memory_order_acquire) + 0x01902120);
  const std::uint8_t result = native(arg1, destination, arg3, arg4, arg5);
  if (!result) {
    return 0;
  }
  if (!destination) {
    return 0;
  }

  for (std::size_t i = 0; i < 6; ++i) {
    std::uintptr_t neighbor = 0;
    if (!read_value(destination + 0x20 + i * sizeof(std::uintptr_t), neighbor)) {
      continue;
    }
    if (neighbor && neighbor != destination && territories_connected(destination, neighbor)) {
      return result;
    }
  }
  return 0;
}


constexpr std::array HOOK1_ORIGINAL = native_hex(
    "41837f60007515498b47384885c00f84ca000000");

constexpr std::array HOOK2_ORIGINAL = native_hex(
    "4c8b058d1b30014180780c010f8565010000");

constexpr std::array HOOK3_ORIGINAL = native_hex(
    "405556574154415541564157b8d0200000");

constexpr std::array HOOK4_GUARD = native_hex(
    "4533c94c8b4020488d542448498b8c24c0000000e880464f00");

constexpr std::array HOOK5_GUARD = native_hex(
    "4533c94c8b4120488d542438498b8c24c0000000e8252f4f00");

constexpr std::array AI1_ORIGINAL = native_hex(
    "e87a683600");

constexpr std::array AI2_ORIGINAL = native_hex(
    "e8a6623600");

constexpr std::array AI3_ORIGINAL = native_hex(
    "e82e023600");

constexpr std::array HOOK7_ORIGINAL = native_hex(
    "e8a4010000");

constexpr std::array HOOK8_ORIGINAL = native_hex(
    "e8dce3f9ff");

constexpr std::array HOOK9_ORIGINAL = native_hex(
    "e84605faff");

constexpr std::array HOOK10_ORIGINAL = native_hex(
    "e846f8f9ff");

constexpr std::array HOOK4_ORIGINAL = native_hex(
    "e880464f00");

constexpr std::array HOOK5_ORIGINAL = native_hex(
    "e8252f4f00");

constexpr std::array HOOK1_TEMPLATE = native_hex(
    "41837f60000f85eb000000498b47384885c00f84ec0000009c50515241504151415241534881ec80000000f30f7f4424"
    "20f30f7f4c2430f30f7f542440f30f7f5c2450f30f7f642460f30f7f6c24704889c14889fa48b80000000000000000ff"
    "d084c00f8448000000f30f6f442420f30f6f4c2430f30f6f542440f30f6f5c2450f30f6f642460f30f6f6c24704881c4"
    "80000000415b415a415941585a59589d4839c7ff25000000000000000000000000f30f6f442420f30f6f4c2430f30f6f"
    "542440f30f6f5c2450f30f6f642460f30f6f6c24704881c480000000415b415a415941585a59589dff25000000000000"
    "000000000000ff25000000000000000000000000ff25000000000000000000000000");

constexpr std::array HOOK2_TEMPLATE = native_hex(
    "9c50515241504151415241534881ec80000000f30f7f442420f30f7f4c2430f30f7f542440f30f7f5c2450f30f7f6424"
    "60f30f7f6c2470498b4e384889f248b80000000000000000ffd084c00f845d000000f30f6f442420f30f6f4c2430f30f"
    "6f542440f30f6f5c2450f30f6f642460f30f6f6c24704881c480000000415b415a415941585a59589d49b80000000000"
    "0000004d8b004180780c010f859b000000ff25000000000000000000000000f30f6f442420f30f6f4c2430f30f6f5424"
    "40f30f6f5c2450f30f6f642460f30f6f6c24704881c480000000415b415a415941585a59589d49b80000000000000000"
    "4d8b004d85c00f84400000004180780c010f853500000041807814000f852a000000418b5010410fb7400a4869c89d0b"
    "00004c01c18b0c114c01c14801d1ff25000000000000000000000000ff25000000000000000000000000");

constexpr std::array HOOK3_TEMPLATE = native_hex(
    "9c50515241504151415241534881ec88000000f30f7f442420f30f7f4c2430f30f7f542440f30f7f5c2450f30f7f6424"
    "60f30f7f6c2470488b8424c800000049ba00000000000000004c39d00f851400000048b80000000000000000ffd084c0"
    "0f8456000000f30f6f442420f30f6f4c2430f30f6f542440f30f6f5c2450f30f6f642460f30f6f6c24704881c4880000"
    "00415b415a415941585a59589d405556574154415541564157b8d0200000ff25000000000000000000000000f30f6f44"
    "2420f30f6f4c2430f30f6f542440f30f6f5c2450f30f6f642460f30f6f6c24704881c488000000415b415a415941585a"
    "59589d488b4424284885c00f8406000000c7000000000031c0c3");

constexpr std::array HOOK45_TEMPLATE = native_hex(
    "9c50515241504151415241534881ec88000000f30f7f442420f30f7f4c2430f30f7f542440f30f7f5c2450f30f7f6424"
    "60f30f7f6c24704889d14c89c248b80000000000000000ffd084c00f8445000000f30f6f442420f30f6f4c2430f30f6f"
    "542440f30f6f5c2450f30f6f642460f30f6f6c24704881c488000000415b415a415941585a59589dff25000000000000"
    "000000000000f30f6f442420f30f6f4c2430f30f6f542440f30f6f5c2450f30f6f642460f30f6f6c24704881c4880000"
    "00415b415a415941585a59589dc3");

constexpr std::array AI_TEMPLATE = native_hex(
    "9c50515241504151415241534881ec88000000f30f7f442420f30f7f4c2430f30f7f542440f30f7f5c2450f30f7f6424"
    "60f30f7f6c247048b80000000000000000ffd084c00f8445000000f30f6f442420f30f6f4c2430f30f6f542440f30f6f"
    "5c2450f30f6f642460f30f6f6c24704881c488000000415b415a415941585a59589dff25000000000000000000000000"
    "f30f6f442420f30f6f4c2430f30f6f542440f30f6f5c2450f30f6f642460f30f6f6c24704881c488000000415b415a41"
    "5941585a59589dc3");

constexpr std::array THUNK_TEMPLATE = native_hex(
    "48b80000000000000000ffe0");

// These are the four offsets verified by the original Feature wrapper.
static_assert(HOOK1_TEMPLATE[97] == std::byte{0x84});
static_assert(HOOK2_TEMPLATE[74] == std::byte{0x84});
static_assert(HOOK3_TEMPLATE[94] == std::byte{0x84});
static_assert(HOOK45_TEMPLATE[73] == std::byte{0x84});

enum class StubTarget { Game, Connected, Filter, Movement, Check78, Check910 };

struct StubRelocation {
    std::size_t offset = 0;
    StubTarget target = StubTarget::Game;
    std::uintptr_t value = 0;
};

constexpr std::array<StubRelocation, 20> STUB_RELOCATIONS{{
    {0x57, StubTarget::Connected, 0x0},
    {0xa9, StubTarget::Game, 0x1961c18},
    {0xee, StubTarget::Game, 0x1961c6f},
    {0xfc, StubTarget::Game, 0x1961b58},
    {0x10a, StubTarget::Game, 0x1961c1a},
    {0x240, StubTarget::Connected, 0x0},
    {0x28b, StubTarget::Game, 0x2c643a0},
    {0x2a7, StubTarget::Game, 0x196281e},
    {0x2e8, StubTarget::Game, 0x2c643a0},
    {0x334, StubTarget::Game, 0x196298a},
    {0x342, StubTarget::Game, 0x1962983},
    {0x441, StubTarget::Game, 0x1962b1c},
    {0x454, StubTarget::Connected, 0x0},
    {0x4b4, StubTarget::Game, 0x1960f41},
    {0x63f, StubTarget::Filter, 0x0},
    {0x68e, StubTarget::Game, 0x193db10},
    {0x839, StubTarget::Movement, 0x0},
    {0x888, StubTarget::Game, 0x17b0ba0},
    {0xa02, StubTarget::Check78, 0x0},
    {0xa12, StubTarget::Check910, 0x0}
}};


bool prepare_isolated_territory(IsolatedTerritory& state, std::uintptr_t base) {
    state.patches.last_result = platform::windows::CodePatchResult::ExpectedBytesMismatch;

    const std::array<std::span<const std::byte>, 12> originals{{
        HOOK1_ORIGINAL, HOOK2_ORIGINAL, HOOK3_ORIGINAL, HOOK4_ORIGINAL, HOOK5_ORIGINAL,
        AI1_ORIGINAL, AI2_ORIGINAL, AI3_ORIGINAL, HOOK7_ORIGINAL, HOOK8_ORIGINAL,
        HOOK9_ORIGINAL, HOOK10_ORIGINAL
    }};
    constexpr std::array<std::uintptr_t, 12> sites{{
        0x01961b3c, 0x0196280c, 0x01960f30, 0x0144948b, 0x0144abe6,
        0x0144a321, 0x0144a8f5, 0x0145096d, 0x01901f77, 0x01963d3f,
        0x01961bd5, 0x019628d5
    }};
    constexpr std::array<std::size_t, 12> entries{{
        0, 0x200, 0x400, 0x600, 0x600, 0x800, 0x800, 0x800, 0xa00, 0xa00, 0xa10, 0xa10
    }};

    for (std::size_t index = 0; index < sites.size(); ++index) {
        if (!native_bytes_match(base + sites[index], originals[index])) {
            return false;
        }
    }

    state.patches.last_result = platform::windows::CodePatchResult::WriteFailed;

    const std::uintptr_t cave = platform::windows::allocate_code_near(base + sites[3], 0x1000);

    if (!cave) {
        return false;
    }

    std::array<std::byte, 0x1000> code{};

    std::copy(HOOK1_TEMPLATE.begin(), HOOK1_TEMPLATE.end(), code.begin());
    std::copy(HOOK2_TEMPLATE.begin(), HOOK2_TEMPLATE.end(), code.begin() + 0x200);
    std::copy(HOOK3_TEMPLATE.begin(), HOOK3_TEMPLATE.end(), code.begin() + 0x400);
    std::copy(HOOK45_TEMPLATE.begin(), HOOK45_TEMPLATE.end(), code.begin() + 0x600);
    std::copy(AI_TEMPLATE.begin(), AI_TEMPLATE.end(), code.begin() + 0x800);
    std::copy(THUNK_TEMPLATE.begin(), THUNK_TEMPLATE.end(), code.begin() + 0xa00);
    std::copy(THUNK_TEMPLATE.begin(), THUNK_TEMPLATE.end(), code.begin() + 0xa10);

    NativePatchGroup patches{};
    bool valid = true;

    for (const StubRelocation& relocation : STUB_RELOCATIONS) {
        std::uintptr_t target = base + relocation.value;

        switch (relocation.target) {
        case StubTarget::Game: break;
        case StubTarget::Connected: target = reinterpret_cast<std::uintptr_t>(&territories_connected); break;
        case StubTarget::Filter: target = reinterpret_cast<std::uintptr_t>(&filter_connected_list); break;
        case StubTarget::Movement: target = reinterpret_cast<std::uintptr_t>(&movement_context_allowed); break;
        case StubTarget::Check78: target = reinterpret_cast<std::uintptr_t>(&movement_check_78); break;
        case StubTarget::Check910: target = reinterpret_cast<std::uintptr_t>(&movement_check_910); break;
        }
        valid = put_native_pointer(code, relocation.offset, target) && valid;
    }
    for (std::size_t index = 0; index < sites.size(); ++index) {
        if (index < 3) {
            valid = append_native_absolute_branch(patches, base + sites[index],
                cave + entries[index], originals[index]) && valid;
        } else {
            valid = append_native_branch(patches, base + sites[index], cave + entries[index],
                originals[index], std::byte{0xe8}) && valid;
        }
    }
    if (!valid || !platform::windows::write_code_memory(cave, code) ||
        !platform::windows::retain_callback_module(reinterpret_cast<std::uintptr_t>(&territories_connected))) {
        platform::windows::free_code(cave);
        return false;
    }

    std::uintptr_t expected_base = 0;

    if (!callback_base.compare_exchange_strong(expected_base, base, std::memory_order_acq_rel) &&
        expected_base != base) {
        platform::windows::free_code(cave);
        return false;
    }
    state.module_base = base;
    state.cave = cave;
    state.patches = patches;
    return true;
}

} // namespace

bool set_isolated_territory(IsolatedTerritory& state, std::uintptr_t module_base, bool enabled) {
    if (!module_base || (state.module_base && state.module_base != module_base)) {
        state.patches.last_result = platform::windows::CodePatchResult::InvalidInput;
        return false;
    }
    if (enabled &&
        (!native_bytes_match(module_base + 0x01449477, std::span<const std::byte>(HOOK4_GUARD).first(20)) ||
         !native_bytes_match(module_base + 0x0144abd2, std::span<const std::byte>(HOOK5_GUARD).first(20)))) {
        state.patches.last_result = platform::windows::CodePatchResult::ExpectedBytesMismatch;
        return false;
    }
    if (enabled && !state.cave && !prepare_isolated_territory(state, module_base)) {
        return false;
    }
    return set_native_patch_group(state.patches, enabled);
}

} // namespace s8::game
