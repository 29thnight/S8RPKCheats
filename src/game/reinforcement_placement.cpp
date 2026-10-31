#include "reinforcement_placement.h"
#include "../platform/windows/code_memory.h"
#include "../platform/windows/process_memory.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <algorithm>
#include <atomic>
#include <limits>

namespace s8::game {
namespace {

// WOOSEOK99/S8RPKCheats@068ca1d881edf870f7cf06f935cf50e286004af8,
// War/ReinforcementDefenderPlacement.cpp. Preserve its native selector ABI,
// defense-commander filter, axial rings 1..5 and ascending map-index priority.
constexpr std::uint64_t PLACEMENT_FAILURE = 0x8000000080000000ULL;
constexpr std::size_t TILE_CAPACITY = 0x4e20;
constexpr std::array CALL_ORIGINAL = native_hex("E8E79B0000");
constexpr std::array THUNK_TEMPLATE = native_hex("48B80000000000000000FFE0");
std::atomic<std::uintptr_t> callback_base{0};
std::atomic<std::uint64_t> callback_epoch{0};
std::atomic<std::uint64_t> cache_generation{1};

using OriginalSelector = std::uint64_t (*)(void*);
using Passability = std::uint8_t (*)(void*, void*);

struct TileEntry {
    std::int32_t q = 0;
    std::int32_t r = 0;
    std::uint32_t index = 0;
};

struct GeometryCache {
    std::array<TileEntry, TILE_CAPACITY> tiles{};
    std::array<std::uint32_t, TILE_CAPACITY> ring{};
    std::uintptr_t battle = 0;
    std::uintptr_t tile_base = 0;
    std::uint32_t tile_count = 0;
    std::size_t ring_count = 0;
    std::uint64_t epoch = 0;
    std::uint64_t generation = 0;
    bool valid = false;
    bool active = false;
};

// Each callback thread owns fixed scratch buffers. No lock is held across a
// native game call; a recursive selector invocation falls back to the game.
thread_local GeometryCache geometry{};

struct GeometryAdmission {
    GeometryCache& cache;

    ~GeometryAdmission() {
        cache.active = false;
    }
};

template<typename T>
bool read_value(std::uintptr_t address, T& value) {
    return platform::windows::read_memory(address,
        std::as_writable_bytes(std::span<T>(&value, 1)));
}

template<typename T>
bool read_fast(std::uintptr_t address, T& value) {
    if (!address) {
        return false;
    }
    __try {
        value = *reinterpret_cast<const T*>(address);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool tile_less(const TileEntry& left, const TileEntry& right) {
    if (left.q != right.q) {
        return left.q < right.q;
    }
    if (left.r != right.r) {
        return left.r < right.r;
    }
    return left.index < right.index;
}

bool ensure_geometry(GeometryCache& cache, std::uintptr_t battle,
    std::uintptr_t tile_base, std::uint32_t tile_count) {
    const std::uint64_t epoch = callback_epoch.load(std::memory_order_acquire);
    const std::uint64_t generation = cache_generation.load(std::memory_order_acquire);

    if (cache.valid && cache.battle == battle && cache.tile_base == tile_base &&
        cache.tile_count == tile_count && cache.epoch == epoch && cache.generation == generation) {
        return true;
    }
    cache.valid = false;
    if (tile_count == 0 || tile_count > cache.tiles.size()) {
        return false;
    }
    for (std::uint32_t index = 0; index < tile_count; ++index) {
        const std::uintptr_t tile = tile_base + static_cast<std::uintptr_t>(index) * 0x40;
        TileEntry& entry = cache.tiles[index];

        if (!read_fast(tile + 8, entry.q) || !read_fast(tile + 0xc, entry.r)) {
            return false;
        }
        entry.index = index;
    }
    std::sort(cache.tiles.begin(), cache.tiles.begin() + tile_count, tile_less);
    cache.battle = battle;
    cache.tile_base = tile_base;
    cache.tile_count = tile_count;
    cache.epoch = epoch;
    cache.generation = generation;
    cache.valid = true;
    return true;
}

bool append_tiles_at(GeometryCache& cache, std::int32_t q, std::int32_t r) {
    std::size_t low = 0;
    std::size_t high = cache.tile_count;

    while (low < high) {
        const std::size_t middle = low + (high - low) / 2;
        const TileEntry& entry = cache.tiles[middle];

        if (entry.q < q || (entry.q == q && entry.r < r)) {
            low = middle + 1;
        } else {
            high = middle;
        }
    }
    while (low < cache.tile_count) {
        const TileEntry& entry = cache.tiles[low];

        if (entry.q != q || entry.r != r) {
            break;
        }
        if (cache.ring_count == cache.ring.size()) {
            return false;
        }
        cache.ring[cache.ring_count++] = entry.index;
        ++low;
    }
    return true;
}

std::uint64_t call_original(void* reinforcement) {
    const std::uintptr_t base = callback_base.load(std::memory_order_acquire);

    return base ? reinterpret_cast<OriginalSelector>(base + 0x01e41810)(reinforcement) : PLACEMENT_FAILURE;
}

std::uint64_t reinforcement_selector(void* reinforcement) {

  GeometryCache& cache = geometry;

  if (cache.active) {
    return call_original(reinforcement);
  }
  cache.active = true;

  const GeometryAdmission admission{cache};
  const std::uintptr_t base = callback_base.load(std::memory_order_acquire);
  const Passability passability = base ? reinterpret_cast<Passability>(base + 0x01d76440) : nullptr;

  const std::uintptr_t reinforcementAddr =
      reinterpret_cast<std::uintptr_t>(reinforcement);
  if (!reinforcementAddr) {
    return call_original(reinforcement);
  }

  std::uintptr_t unitState = 0;
  if (!read_value(reinforcementAddr + 0x08, unitState) || !unitState) {
    return call_original(reinforcement);
  }

  std::uintptr_t ownerBackref = 0;
  if (!read_value(unitState, ownerBackref) ||
      ownerBackref != reinforcementAddr) {
    return call_original(reinforcement);
  }

  std::uint8_t state5D = 0;
  if (!read_value(unitState + 0x5D, state5D) || state5D != 3) {
    return call_original(reinforcement);
  }

  std::uint8_t state5C = 0;
  if (!read_value(unitState + 0x5C, state5C) ||
      (state5C != 1 && state5C != 2)) {
    return call_original(reinforcement);
  }

  std::uintptr_t sideHolder = 0;
  std::uintptr_t sideObject = 0;
  std::uint8_t side = 0;
  if (!read_value(unitState + 0x30, sideHolder) || !sideHolder ||
      !read_value(sideHolder, sideObject) || !sideObject ||
      !read_value(sideObject + 0x18, side) || side != 1) {
    return call_original(reinforcement);
  }

  std::uintptr_t battleHolder = 0;
  std::uintptr_t battleObject = 0;
  std::uintptr_t battleContainer = 0;
  if (!read_value(unitState + 0x10, battleHolder) || !battleHolder ||
      !read_value(battleHolder, battleObject) || !battleObject ||
      !read_value(battleObject + 0x50, battleContainer) || !battleContainer) {
    return call_original(reinforcement);
  }

  std::uintptr_t unitList = 0;
  std::uint32_t unitCount = 0;
  std::uintptr_t unitEntries = 0;
  if (!read_value(battleContainer + 0x108, unitList) || !unitList ||
      !read_value(unitList + 0x18, unitCount) ||
      unitCount == 0 || unitCount > 0x1000 ||
      !read_value(unitList + 0x10, unitEntries) || !unitEntries) {
    return call_original(reinforcement);
  }

  const std::size_t unitEntryBytes = static_cast<std::size_t>(unitCount) * 0x10;
  if (!platform::windows::is_committed_memory(unitEntries, unitEntryBytes)) {
    return call_original(reinforcement);
  }

  std::uintptr_t commanderTile = 0;
  for (std::uint32_t i = 0; i < unitCount; ++i) {
    const std::uintptr_t entry = unitEntries + static_cast<std::uintptr_t>(i) * 0x10;

    std::uintptr_t unit = 0;
    if (!read_fast(entry + 0x08, unit) || !unit ||
        !platform::windows::is_committed_memory(unit, 0x60)) {
      continue;
    }

    std::uintptr_t unitBackref = 0;
    std::uint8_t unitState5D = 0;
    std::uint16_t commanderMarker = 0;
    std::uintptr_t unitSideHolder = 0;
    std::uintptr_t tile = 0;
    if (!read_fast(unit, unitBackref) || unitBackref != entry ||
        !read_fast(unit + 0x5D, unitState5D) || unitState5D != 2 ||
        !read_fast(unit + 0x38, commanderMarker) || commanderMarker == 0 ||
        !read_fast(unit + 0x30, unitSideHolder) || !unitSideHolder ||
        !read_fast(unit + 0x40, tile) || !tile) {
      continue;
    }

    std::uintptr_t unitSideObject = 0;
    if (!platform::windows::is_committed_memory(unitSideHolder, sizeof(std::uintptr_t)) ||
        !read_fast(unitSideHolder, unitSideObject) || !unitSideObject ||
        !platform::windows::is_committed_memory(unitSideObject + 0x18, 1)) {
      continue;
    }

    std::uint8_t unitSide = 0;
    if (!read_fast(unitSideObject + 0x18, unitSide) || unitSide != 1) {
      continue;
    }

    if (commanderTile) {
      return call_original(reinforcement);
    }
    commanderTile = tile;
  }

  if (!commanderTile) {
    return call_original(reinforcement);
  }

  std::uintptr_t mapObject = 0;
  std::uint32_t tileCount = 0;
  std::uintptr_t tileBase = 0;
  if (!read_value(battleContainer + 0x18, mapObject) || !mapObject ||
      !read_value(mapObject + 0x198, tileCount) ||
      tileCount == 0 || tileCount > 0x4E20 ||
      !read_value(mapObject + 0x1A0, tileBase) || !tileBase) {
    return call_original(reinforcement);
  }

  const std::size_t tileBytes = static_cast<std::size_t>(tileCount) * 0x40;
  const std::uintptr_t tileEnd = tileBase + tileBytes;
  if (!platform::windows::is_committed_memory(tileBase, tileBytes)) {
    return call_original(reinforcement);
  }

  if (commanderTile < tileBase || commanderTile >= tileEnd ||
      ((commanderTile - tileBase) & 0x3F) != 0) {
    return call_original(reinforcement);
  }

  std::int32_t commanderQ = 0;
  std::int32_t commanderR = 0;
  if (!read_fast(commanderTile + 0x08, commanderQ) ||
      !read_fast(commanderTile + 0x0C, commanderR)) {
    return call_original(reinforcement);
  }

  if (!ensure_geometry(cache, battleContainer, tileBase, tileCount)) {
    return call_original(reinforcement);
  }

  if (!passability) {
    return call_original(reinforcement);
  }

  static constexpr std::int32_t kDirections[6][2] = {
      {1, 0}, {1, -1}, {0, -1},
      {-1, 0}, {-1, 1}, {0, 1},
  };

  constexpr std::int64_t kMaxPlacementRadius = 5;

  for (std::int64_t radius = 1; radius <= kMaxPlacementRadius; ++radius) {
    cache.ring_count = 0;

    std::int64_t q = static_cast<std::int64_t>(commanderQ) - radius;
    std::int64_t r = static_cast<std::int64_t>(commanderR) + radius;

    for (int sideIndex = 0; sideIndex < 6; ++sideIndex) {
      for (std::int64_t step = 0; step < radius; ++step) {
        if (q >= INT32_MIN && q <= INT32_MAX &&
            r >= INT32_MIN && r <= INT32_MAX) {
          if (!append_tiles_at(cache, static_cast<std::int32_t>(q), static_cast<std::int32_t>(r))) {
            return call_original(reinforcement);
          }
        }
        q += kDirections[sideIndex][0];
        r += kDirections[sideIndex][1];
      }
    }

    if (cache.ring_count == 0) {
      continue;
    }

    std::sort(cache.ring.begin(),
              cache.ring.begin() + cache.ring_count);

    for (const std::uint32_t index : std::span<const std::uint32_t>(cache.ring.data(), cache.ring_count)) {
      const std::uintptr_t tile =
          tileBase + static_cast<std::uintptr_t>(index) * 0x40;

      std::uintptr_t occupantA = 0;
      std::uintptr_t occupantB = 0;
      std::uintptr_t tileInfo = 0;
      std::uint8_t tileType = 0;
      if (!read_fast(tile + 0x18, occupantA) || occupantA != 0 ||
          !read_fast(tile + 0x20, occupantB) || occupantB != 0 ||
          !read_fast(tile, tileInfo) || !tileInfo ||
          !read_fast(tileInfo + 0x09, tileType) || tileType == 0x0A) {
        continue;
      }

      const std::uint8_t passable =
          passability(reinterpret_cast<void *>(tile), reinforcement);
      if (!passable) {
        continue;
      }

      std::uint64_t result = 0;
      if (!read_fast(tileInfo, result)) {
        return call_original(reinforcement);
      }

      return result;
    }
  }

  return call_original(reinforcement);
}


bool valid_reinforcement_context(std::uintptr_t base) {
    constexpr std::array DISPATCH_ORIGINAL = native_hex("136CD8015B6CD801");
    constexpr std::array DISPATCH_ARRIVAL_FIRST = native_hex("5B6CD801136CD801");
    constexpr std::array TURN_ORIGINAL = native_hex("FFC883F80E");
    constexpr std::array ARRIVAL_ORIGINAL = native_hex("E85AAA0B00");
    constexpr std::array ORDER_ORIGINAL = native_hex("E86B660B00");
    constexpr std::array SPAWN_ORIGINAL = native_hex("E856DEFFFF");
    constexpr std::array SELECTOR_SIGNATURE = native_hex("488B0344386009");

    return (native_bytes_match(base + 0x01d86d58, DISPATCH_ORIGINAL) ||
            native_bytes_match(base + 0x01d86d58, DISPATCH_ARRIVAL_FIRST)) &&
        native_bytes_match(base + 0x01d86585, TURN_ORIGINAL) &&
        native_bytes_match(base + 0x01d86c51, ARRIVAL_ORIGINAL) &&
        native_bytes_match(base + 0x01d86c60, ORDER_ORIGINAL) &&
        native_bytes_match(base + 0x01e38305, SPAWN_ORIGINAL) &&
        native_bytes_match(base + 0x01e41891, SELECTOR_SIGNATURE) &&
        platform::windows::is_committed_memory(base + 0x01d76440, 1) &&
        platform::windows::is_committed_memory(base + 0x01e41810, 1);
}

bool prepare_reinforcement(ReinforcementPlacement& state, std::uintptr_t base) {
    state.patches.last_result = platform::windows::CodePatchResult::ExpectedBytesMismatch;

    if (!native_bytes_match(base + 0x01e37c24, CALL_ORIGINAL)) {
        return false;
    }

    state.patches.last_result = platform::windows::CodePatchResult::WriteFailed;

    const std::uintptr_t cave = platform::windows::allocate_code_near(base + 0x01e37c24, 0x40);

    if (!cave) {
        return false;
    }

    std::array<std::byte, THUNK_TEMPLATE.size()> code = THUNK_TEMPLATE;
    NativePatchGroup patches{};

    if (!put_native_pointer(code, 2, reinterpret_cast<std::uintptr_t>(&reinforcement_selector)) ||
        !append_native_branch(patches, base + 0x01e37c24, cave, CALL_ORIGINAL, std::byte{0xe8}) ||
        !platform::windows::write_code_memory(cave, code) ||
        !platform::windows::retain_callback_module(reinterpret_cast<std::uintptr_t>(&reinforcement_selector))) {
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

void change_reinforcement_epoch(domain::GameEpoch epoch) {
    callback_epoch.store(epoch.value, std::memory_order_release);
    cache_generation.fetch_add(1, std::memory_order_acq_rel);
}

bool set_reinforcement_placement(ReinforcementPlacement& state, std::uintptr_t module_base, bool enabled) {
    if (!module_base || (state.module_base && state.module_base != module_base)) {
        state.patches.last_result = platform::windows::CodePatchResult::InvalidInput;
        return false;
    }
    if (enabled && !valid_reinforcement_context(module_base)) {
        state.patches.last_result = platform::windows::CodePatchResult::ExpectedBytesMismatch;
        return false;
    }
    if (enabled && !state.cave && !prepare_reinforcement(state, module_base)) {
        return false;
    }
    if (!set_native_patch_group(state.patches, enabled)) {
        return false;
    }
    cache_generation.fetch_add(1, std::memory_order_acq_rel);
    return true;
}

} // namespace s8::game
