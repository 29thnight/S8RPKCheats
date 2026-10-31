#include "specialty_distribution.h"

#include "../platform/windows/process_memory.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <span>
#include <vector>

namespace s8::game {
namespace {

template <typename T>
bool read_value(std::uintptr_t address, T& value) {
    return platform::windows::read_memory(address, std::as_writable_bytes(std::span(&value, 1)));
}

std::uintptr_t pointer48(std::uintptr_t value) {
    return value & 0x0000ffffffffffffULL;
}

std::uintptr_t specialty_base(std::uintptr_t module_base) {
    constexpr std::array<std::uintptr_t, 3> ANCHORS{0x034c8630, 0x037b0000, 0x037ff430};
    constexpr std::array<std::uintptr_t, 3> OFFSETS{0, 0x20, 0};
    for (const auto anchor : ANCHORS) {
        std::uintptr_t address = 0;
        if (!read_value(module_base + anchor, address) || address < 0x10000) {
            continue;
        }
        for (const auto offset : OFFSETS) {
            std::uintptr_t next = 0;
            if (!read_value(address + offset, next) || next < 0x10000) {
                address = 0;
                break;
            }
            address = next;
        }
        if (address && platform::windows::is_committed_memory(address, 0x38)) {
            return address;
        }
    }
    return 0;
}

bool unowned(std::uintptr_t object, std::uint16_t expected_id = 0) {
    object = pointer48(object);
    if (object <= 0x10000 || !platform::windows::is_committed_memory(object, 0x40)) {
        return false;
    }
    std::uint16_t id = 0;
    std::uintptr_t owner = 0;
    std::uint32_t owner_type = 0;
    std::uint8_t type = 0;
    return read_value(object + 8, id) && (!expected_id || id == expected_id) &&
        read_value(object + 0x30, owner) && read_value(object + 0x38, owner_type) &&
        pointer48(owner) <= 0x10000 && read_value(object + 0x0e, type) && type >= 1 && type <= 26;
}

bool assign_slot(std::uintptr_t slot, std::uintptr_t object) {
    object = pointer48(object);
    if (slot <= 0x10008 || object <= 0x10000 ||
        !platform::windows::is_committed_memory(slot - 8, 0x14) ||
        !platform::windows::is_committed_memory(object, 0x40)) {
        return false;
    }
    return platform::windows::write_specialty_slot(slot, object);
}

} // namespace

int assign_specialties(std::uintptr_t module_base, const domain::NamedSpecialties& named, std::mt19937& random) {
    std::uintptr_t city_base = 0;
    if (!module_base || !read_value(module_base + 0x034c8630, city_base) || city_base <= 0x10000 ||
        !read_value(city_base, city_base) || city_base <= 0x10000 ||
        !read_value(city_base, city_base) || city_base <= 0x10000) {
        return 0;
    }
    auto base = specialty_base(module_base);
    std::uint16_t check = 0;
    if (base > 0x10000 && read_value(base + 8, check) && check != 1) {
        base += 0x19af60;
    }
    struct Sample { std::uintptr_t address; std::uint16_t id; };
    std::vector<Sample> samples;
    std::vector<std::uintptr_t> assigned;
    std::vector<std::uintptr_t> empty_slots;
    for (int city = 0; city < 51; ++city) {
        for (int slot = 0; slot < 3; ++slot) {
            const auto address = city_base + city * 0x2a0 + 0x248 + slot * 0x18;
            std::uintptr_t object = 0;
            std::uint32_t enabled = 0, bought = 0;
            if (!read_value(address, object)) {
                continue;
            }
            object = pointer48(object);
            if (object > 0x10000) {
                assigned.push_back(object);
                std::uint16_t id = 0;
                if (samples.size() < 2 && read_value(object + 8, id) && id > 0 && id <= 1000) {
                    samples.push_back({object, id});
                }
            }
            if (read_value(address - 8, enabled) && read_value(address + 8, bought) &&
                (object <= 0x10000 || !enabled || bought)) {
                empty_slots.push_back(address);
            }
        }
    }
    std::uintptr_t stride = 0x40;
    if (samples.size() == 2 && samples[0].id != samples[1].id) {
        const auto address_diff = static_cast<std::int64_t>(samples[1].address) - static_cast<std::int64_t>(samples[0].address);
        const auto id_diff = static_cast<int>(samples[1].id) - static_cast<int>(samples[0].id);
        const auto candidate = static_cast<std::uintptr_t>(std::abs(address_diff) / std::abs(id_diff));
        if (candidate >= 0x20 && candidate <= 0x100 && candidate % 8 == 0) {
            stride = candidate;
        }
    }
    if ((base <= 0x10000 || !read_value(base + 8, check) || check != 1) && !samples.empty()) {
        base = samples[0].address - static_cast<std::uintptr_t>(samples[0].id - 1) * stride;
    }
    if (base <= 0x10000 || !read_value(base + 8, check) || check != 1) {
        return 0;
    }
    std::vector<std::uintptr_t> pool;
    for (std::uint16_t id = 1; id <= 300; ++id) {
        const auto object = base + static_cast<std::uintptr_t>(id - 1) * stride;
        if (named[id] && std::find(assigned.begin(), assigned.end(), object) == assigned.end() && unowned(object, id)) {
            pool.push_back(object);
        }
    }
    std::shuffle(pool.begin(), pool.end(), random);
    std::shuffle(empty_slots.begin(), empty_slots.end(), random);
    int assigned_count = 0;
    const auto count = std::min(pool.size(), empty_slots.size());
    for (std::size_t i = 0; i < count; ++i) {
        if (unowned(pool[i]) && assign_slot(empty_slots[i], pool[i])) {
            ++assigned_count;
        }
    }
    return assigned_count;
}

} // namespace s8::game
