#include "city_management.h"

#include "../platform/windows/process_memory.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <span>
#include <utility>

namespace s8::game {
namespace {

constexpr std::uintptr_t CITY_STRIDE = 0x2a0;
constexpr std::array<std::uintptr_t, domain::CITY_FIELD_COUNT> FIELD_OFFSETS{
    0xbc, 0xc0, 0xc4, 0xd0, 0xd4, 0xd8, 0xdc, 0x100, 0x105};
constexpr unsigned PENDING_REFRESH = 1;
constexpr unsigned PENDING_REVOLT = 2;
constexpr unsigned PENDING_EXCHANGE = 4;
constexpr unsigned PENDING_ROUTES = 8;
constexpr unsigned PENDING_TECHNOLOGY = 16;
constexpr std::uintptr_t FACTION_STRIDE = 0x998;

template<class T> bool read_value(std::uintptr_t address, T& value) {
    return platform::windows::read_memory(address, std::as_writable_bytes(std::span(&value, 1)));
}

template<class T> T from_record(const std::array<std::byte, CITY_STRIDE>& record, std::size_t offset) {
    T value{};

    std::memcpy(&value, record.data() + offset, sizeof(value));
    return value;
}

std::size_t field_width(domain::CityField field) {
    switch (field) {
    case domain::CityField::DevelopmentLimit: case domain::CityField::CommerceLimit:
    case domain::CityField::DefenseLimit: case domain::CityField::TechnologyLimit:
        return 2;
    case domain::CityField::Revolt:
        return 1;
    default:
        return 4;
    }
}

std::uint16_t officer_id(std::uintptr_t officer) {
    std::uint16_t id = 0;

    return officer > 0x10000 && read_value(officer + 8, id) && id >= 1 && id <= 5102
        ? id : domain::NO_CITY_OFFICER;
}

std::uint16_t force_lord(std::uintptr_t force) {
    std::uintptr_t lord = 0;

    return force > 0x10000 && read_value(force + 0xc0, lord) ? officer_id(lord) : domain::NO_CITY_OFFICER;
}

std::uintptr_t player_force(std::uintptr_t player) {
    std::uintptr_t city = 0;

    return player > 0x10000 && read_value(player + 0x20, city) ? read_city_force(city) : 0;
}

bool scenario_ready(std::uintptr_t root, std::uintptr_t player) {
    std::uintptr_t current_player = 0;

    return root > 0x10000 && platform::windows::is_committed_memory(root, 0x6000) &&
        player > 0x10000 && platform::windows::is_committed_memory(player, 0x200) &&
        read_value(root + 0xe0, current_player) && current_player == player &&
        officer_id(player) != domain::NO_CITY_OFFICER;
}

bool basic_city_action(domain::CityAction action) {
    switch (action) {
    case domain::CityAction::Refresh: case domain::CityAction::Write:
    case domain::CityAction::Maximize: case domain::CityAction::ResetRevolt:
    case domain::CityAction::Cancel: case domain::CityAction::ConvertSelected:
        return true;
    default:
        return false;
    }
}

std::uintptr_t resolve_faction_table(std::uintptr_t module_base) {
    if (!module_base || module_base > std::numeric_limits<std::uintptr_t>::max() - 0x34c8630) {
        return 0;
    }
    constexpr std::array<std::uintptr_t, 5> CHAIN{0, 0, 8, 0x10, 0};
    std::uintptr_t address = module_base + 0x34c8630;

    for (const std::uintptr_t offset : CHAIN) {
        if (!read_value(address + offset, address) || address <= 0x10000) {
            return 0;
        }
    }
    if (address > std::numeric_limits<std::uintptr_t>::max() - 0xc6848) {
        return 0;
    }
    return address + 0xc6848;
}

bool read_faction_technology(CityManagement& state, std::size_t index) {
    const std::uintptr_t address = state.faction_table + index * FACTION_STRIDE;
    std::uint8_t exists = 0;
    domain::CityFactionTechnology row;

    if (!read_value(address + 0xc5, exists)) {
        state.snapshot.factions[index] = row;
        return false;
    }
    if (exists != 0) {
        row.lord = force_lord(address);
        row.valid = read_value(address + 0x205, row.levels);
    }
    state.snapshot.factions[index] = row;
    return exists == 0 || row.valid;
}

struct CityRefreshLinks {
    struct Corps {
        std::uintptr_t address = 0;
        std::array<std::byte, 2 * sizeof(std::uintptr_t)> bytes{};
        bool readable = false;
    };

    std::array<Corps, domain::CITY_COUNT> corps{};
    std::size_t count = 0;
};

bool read_city_row(CityManagement& state, std::size_t index,
    const std::array<std::byte, CITY_STRIDE>* captured = nullptr, CityRefreshLinks* links = nullptr) {
    std::array<std::byte, CITY_STRIDE> local_bytes{};
    const std::array<std::byte, CITY_STRIDE>& bytes = captured ? *captured : local_bytes;
    domain::CityRow row;
    const std::uintptr_t city = state.city_table + index * CITY_STRIDE;

    if (!captured && !read_value(city, local_bytes)) {
        state.snapshot.cities[index] = row;
        state.forces[index] = 0;
        return false;
    }
    for (std::size_t field = 0; field < FIELD_OFFSETS.size(); ++field) {
        const std::size_t width = field_width(static_cast<domain::CityField>(field));

        std::memcpy(&row.values[field], bytes.data() + FIELD_OFFSETS[field], width);
    }
    const std::uintptr_t governor = from_record<std::uintptr_t>(bytes, 0x98);
    const std::uintptr_t corps = from_record<std::uintptr_t>(bytes, 0x90);
    std::uintptr_t force = 0;

    if (governor > 0x10000) {
        // ID (+8) and force (+0x18) belong to the same officer record.
        std::array<std::byte, 0x20 - 8> governor_bytes{};

        if (read_value(governor + 8, governor_bytes)) {
            std::uint16_t id = 0;

            std::memcpy(&id, governor_bytes.data(), sizeof(id));
            if (id >= 1 && id <= 5102) { row.governor = id; }
            std::memcpy(&force, governor_bytes.data() + 0x18 - 8, sizeof(force));
            if (force <= 0x10000) { force = 0; }
        }
    }
    if (corps > 0x10000) {
        // Force (+0x10) and district number (+0x18) are adjacent.
        std::array<std::byte, 2 * sizeof(std::uintptr_t)> corps_bytes{};

        bool readable = false;

        if (links) {
            std::size_t slot = 0;

            while (slot < links->count && links->corps[slot].address != corps) {
                ++slot;
            }
            CityRefreshLinks::Corps& entry = links->corps[slot];

            if (slot == links->count) {
                entry.address = corps;
                entry.readable = read_value(corps + 0x10, entry.bytes);
                ++links->count;
            }
            corps_bytes = entry.bytes;
            readable = entry.readable;
        } else {
            readable = read_value(corps + 0x10, corps_bytes);
        }
        if (readable) {
            if (force == 0) {
                std::memcpy(&force, corps_bytes.data(), sizeof(force));
                if (force <= 0x10000) { force = 0; }
            }
            std::memcpy(&row.corps, corps_bytes.data() + 8, sizeof(row.corps));
        }
    }
    bool lord_loaded = false;

    if (state.work == CityWork::Refresh && force != 0) {
        for (std::size_t previous = 0; previous < index; ++previous) {
            if (state.snapshot.cities[previous].valid && state.forces[previous] == force) {
                row.force_lord = state.snapshot.cities[previous].force_lord;
                lord_loaded = true;
                break;
            }
        }
    }
    if (!lord_loaded) { row.force_lord = force_lord(force); }
    row.occupied = force != 0;
    for (std::size_t slot = 0; slot < row.neighbors.size(); ++slot) {
        const std::uintptr_t neighbor = from_record<std::uintptr_t>(bytes, 0x20 + slot * sizeof(std::uintptr_t));

        if (neighbor == 0) {
            continue;
        }
        row.neighbors[slot] = city_index_from_pointer(state.city_table, neighbor);
        if (row.neighbors[slot] == domain::NO_CITY) {
            ++row.unknown_connections;
        }
    }
    row.valid = true;
    state.snapshot.cities[index] = row;
    state.forces[index] = force;
    return true;
}

void update_frontiers(CityManagement& state) {
    for (std::size_t index = 0; index < state.snapshot.cities.size(); ++index) {
        domain::CityRow& row = state.snapshot.cities[index];

        row.frontline = false;
        row.foreign_neighbors = 0;
        if (!row.valid || state.forces[index] == 0) {
            continue;
        }
        for (const std::uint8_t neighbor : row.neighbors) {
            if (neighbor != domain::NO_CITY && state.forces[neighbor] != state.forces[index]) {
                row.frontline = true;
                ++row.foreign_neighbors;
            }
        }
    }
    state.snapshot.player_force_lord = force_lord(player_force(state.player));
}

bool restore_data(CityManagement& state) {
    bool restored = true;

    for (std::size_t index = state.recovery_count; index != 0; --index) {
        const CityDataWrite& write = state.recovery[index - 1];
        std::array<std::byte, 4> current{};
        const std::span<const std::byte> before = std::as_bytes(std::span(&write.before, 1)).first(write.width);
        const std::span<const std::byte> after = std::as_bytes(std::span(&write.after, 1)).first(write.width);

        if (!platform::windows::read_memory(write.address, std::span(current).first(write.width))) {
            restored = false;
            continue;
        }
        if (std::equal(before.begin(), before.end(), current.begin())) {
            continue;
        }
        bool owned = true;

        for (std::size_t byte = 0; byte < write.width; ++byte) {
            owned = owned && (current[byte] == before[byte] || current[byte] == after[byte]);
        }
        if (!owned || !platform::windows::write_data_memory(write.address, before)) {
            restored = false;
        }
    }
    if (restored) {
        state.recovery_count = 0;
    }
    return restored;
}

bool restore_bound_data(CityManagement& state) {
    const std::uintptr_t table = state.recovery_faction ? resolve_faction_table(state.module_base)
        : resolve_city_data(state.module_base);

    if (table == 0) {
        return false;
    }
    if (table != state.recovery_table) {
        state.recovery_count = 0;
        return true;
    }
    return restore_data(state);
}

domain::CityWorkStatus write_transaction(CityManagement& state, std::span<const CityDataWrite> writes, bool faction = false) {
    const std::uintptr_t table = faction ? resolve_faction_table(state.module_base) : resolve_city_data(state.module_base);
    if (table == 0) {
        return domain::CityWorkStatus::ReadFailed;
    }
    if (table != (faction ? state.faction_table : state.city_table)) {
        return domain::CityWorkStatus::SessionChanged;
    }
    for (const CityDataWrite& write : writes) {
        std::uint32_t current = 0;

        if (!platform::windows::read_memory(write.address,
                std::as_writable_bytes(std::span(&current, 1)).first(write.width)) || current != write.before) {
            return domain::CityWorkStatus::OwnershipChanged;
        }
    }
    state.recovery_count = 0;
    state.recovery_faction = faction;
    state.recovery_table = faction ? state.faction_table : state.city_table;
    for (const CityDataWrite& write : writes) {
        state.recovery[state.recovery_count++] = write;
        if (!platform::windows::write_data_memory(write.address,
                std::as_bytes(std::span(&write.after, 1)).first(write.width))) {
            return restore_data(state) ? domain::CityWorkStatus::WriteFailed : domain::CityWorkStatus::RollbackFailed;
        }
    }
    state.recovery_count = 0;
    return domain::CityWorkStatus::Complete;
}

void end_work(CityManagement& state, domain::CityWorkStatus status) {
    state.work = CityWork::Idle;
    state.active_routes = {};
    state.snapshot.busy = false;
    state.snapshot.locks_commands = false;
    state.snapshot.status = status;
    state.snapshot.last_request = status;
    ++state.snapshot.revision;
}

void start_work(CityManagement& state, CityWork work) {
    state.work = work;
    state.work_result = domain::CityWorkStatus::Complete;
    state.cursor = 0;
    state.snapshot.busy = true;
    state.snapshot.locks_commands = work == CityWork::Maximize || work == CityWork::Support ||
        work == CityWork::Routes || work == CityWork::ConvertSelected;
    state.snapshot.status = domain::CityWorkStatus::Working;
    state.snapshot.completed = 0;
    state.snapshot.succeeded = 0;
    state.snapshot.moved = {};
    state.snapshot.troop_limit_reached = false;
    state.snapshot.total = domain::CITY_COUNT;
    ++state.snapshot.revision;
}

bool bind_city_session(CityManagement& state, std::uintptr_t module_base, std::uintptr_t root,
    std::uintptr_t player, domain::GameEpoch epoch, bool refresh = true) {
    // The city owner table does not require a protagonist. Scenario operations
    // keep their separate live-player gate.
    const bool same_session = state.module_base == module_base && state.game_root == root && state.player == player &&
        state.epoch == epoch;

    if (!refresh && same_session && state.snapshot.available) {
        return true;
    }
    const std::uintptr_t table = resolve_city_data(module_base);

    if (same_session && state.city_table == table) {
        if (!state.snapshot.available && table != 0) {
            state.snapshot.available = true;
            ++state.snapshot.revision;
        }
        return table != 0;
    }
    if (same_session && table == 0 && state.city_table != 0) {
        if (state.snapshot.available) {
            state.snapshot.available = false;
            ++state.snapshot.revision;
        }
        return false;
    }
    state.module_base = module_base;
    state.game_root = root;
    state.player = player;
    state.city_table = table;
    state.epoch = epoch;
    state.recovery_count = 0;
    state.observed_year = 0;
    state.observed_month = 0;
    state.exchanged_month = 0xff;
    state.next_maintenance_ms = 0;
    state.next_revolt_ms = 0;
    state.pending = table ? PENDING_REFRESH : 0;
    state.snapshot.epoch = epoch;
    state.snapshot.cities = {};
    state.snapshot.factions = {};
    state.snapshot.year = 0;
    state.snapshot.month = 0;
    state.snapshot.player_force_lord = domain::NO_CITY_OFFICER;
    state.snapshot.technologies_loaded = false;
    state.faction_table = 0;
    state.snapshot.available = table != 0;
    state.forces = {};
    end_work(state, domain::CityWorkStatus::SessionChanged);
    return table != 0;
}

bool city_frontline(std::uintptr_t table, std::uint8_t index, std::uintptr_t force, bool& frontline) {
    std::array<std::uintptr_t, 6> neighbors{};

    frontline = false;
    if (!read_value(table + index * CITY_STRIDE + 0x20, neighbors)) {
        return false;
    }
    for (const std::uintptr_t neighbor : neighbors) {
        if (city_index_from_pointer(table, neighbor) != domain::NO_CITY && read_city_force(neighbor) != force) {
            frontline = true;
        }
    }
    return true;
}

std::uint32_t support_amount(std::uint32_t available, std::uint32_t requested, domain::CitySupportMode mode) {
    const std::uint64_t amount = mode == domain::CitySupportMode::Percent
        ? static_cast<std::uint64_t>(available) * requested / 100 : requested;

    return static_cast<std::uint32_t>(std::min<std::uint64_t>(available, amount));
}

domain::CityWorkStatus transfer_support(CityManagement& state, const domain::CitySupportRoute& route,
    std::uintptr_t force, bool require_frontier) {
    if (!domain::valid_city_route(route) || force <= 0x10000) {
        return domain::CityWorkStatus::InvalidRequest;
    }
    const std::uintptr_t source = state.city_table + route.source * CITY_STRIDE;
    const std::uintptr_t target = state.city_table + route.target * CITY_STRIDE;

    if (read_city_force(source) != force || read_city_force(target) != force) {
        return domain::CityWorkStatus::OwnershipChanged;
    }
    if (require_frontier) {
        bool source_front = false;
        bool target_front = false;

        if (!city_frontline(state.city_table, route.source, force, source_front) ||
            !city_frontline(state.city_table, route.target, force, target_front)) {
            return domain::CityWorkStatus::ReadFailed;
        }
        if (source_front || !target_front) {
            return domain::CityWorkStatus::OwnershipChanged;
        }
    }
    std::array<std::uint32_t, 3> source_values{};
    std::array<std::uint32_t, 3> target_values{};
    std::uint32_t troop_limit = 0;

    if (!read_value(source + FIELD_OFFSETS[0], source_values) ||
        !read_value(target + FIELD_OFFSETS[0], target_values) || !read_value(target + 0x100, troop_limit)) {
        return domain::CityWorkStatus::ReadFailed;
    }
    const std::array<std::uint32_t, 3> requested{route.gold, route.grain, route.troops};
    std::array<CityDataWrite, 6> writes{};
    std::array<std::uint32_t, 3> moved{};
    std::size_t count = 0;

    for (std::size_t index = 0; index < requested.size(); ++index) {
        const std::uint32_t amount = support_amount(source_values[index], requested[index], route.mode);
        const std::uint32_t room = index == 2 ? (troop_limit > target_values[index] ? troop_limit - target_values[index] : 0)
            : std::numeric_limits<std::uint32_t>::max() - target_values[index];

        moved[index] = std::min(amount, room);
        if (index == 2 && amount > room) {
            state.snapshot.troop_limit_reached = true;
        }
        if (moved[index] != 0) {
            writes[count++] = {source + FIELD_OFFSETS[index], source_values[index], source_values[index] - moved[index]};
            writes[count++] = {target + FIELD_OFFSETS[index], target_values[index], target_values[index] + moved[index]};
        }
    }
    if (count == 0) {
        return domain::CityWorkStatus::NoResources;
    }
    const domain::CityWorkStatus status = write_transaction(state, std::span(writes).first(count));

    if (status == domain::CityWorkStatus::Complete) {
        state.snapshot.moved = moved;
        state.pending |= PENDING_REFRESH;
    }
    return status;
}

domain::CityWorkStatus write_city_field(CityManagement& state, std::uint8_t city,
    domain::CityField field, std::uint32_t desired) {
    const std::size_t index = static_cast<std::size_t>(field);
    const std::uintptr_t address = state.city_table + city * CITY_STRIDE + FIELD_OFFSETS[index];
    const std::size_t width = field_width(field);
    std::uint32_t current = 0;

    if (!platform::windows::read_memory(address, std::as_writable_bytes(std::span(&current, 1)).first(width))) {
        return domain::CityWorkStatus::ReadFailed;
    }
    if (width == 2) {
        desired = std::min<std::uint32_t>(desired, 30000);
    } else if (width == 1) {
        desired = std::min<std::uint32_t>(desired, 255);
    }
    if (field == domain::CityField::Troops) {
        std::uint32_t limit = 0;

        if (!read_value(state.city_table + city * CITY_STRIDE + 0x100, limit)) {
            return domain::CityWorkStatus::ReadFailed;
        }
        desired = std::min(desired, limit);
    }
    const CityDataWrite write{address, current, desired, width};
    const domain::CityWorkStatus result = current == desired ? domain::CityWorkStatus::Complete
        : write_transaction(state, std::span(&write, 1));

    if (result == domain::CityWorkStatus::Complete && state.snapshot.cities[city].valid) {
        state.snapshot.cities[city].values[index] = desired;
    }
    return result;
}

domain::CityWorkStatus write_faction_technology(CityManagement& state, const domain::CityRequest& request) {
    if (request.city >= domain::CITY_FACTION_COUNT || request.technology >= domain::CITY_TECHNOLOGY_LIMITS.size()) {
        return domain::CityWorkStatus::InvalidRequest;
    }
    const std::uintptr_t table = resolve_faction_table(state.module_base);

    if (!table || table != state.faction_table) {
        state.pending |= PENDING_TECHNOLOGY;
        return domain::CityWorkStatus::SessionChanged;
    }
    const std::uintptr_t faction = table + request.city * FACTION_STRIDE;
    std::uint8_t exists = 0;
    std::array<std::uint8_t, 5> levels{};

    if (!read_value(faction + 0xc5, exists) || !read_value(faction + 0x205, levels)) {
        return domain::CityWorkStatus::ReadFailed;
    }
    if (!exists || request.force_lord == domain::NO_CITY_OFFICER || force_lord(faction) != request.force_lord) {
        return domain::CityWorkStatus::OwnershipChanged;
    }
    std::array<CityDataWrite, 5> writes{};
    std::size_t count = 0;

    for (std::size_t index = 0; index < levels.size(); ++index) {
        if (request.action == domain::CityAction::WriteTechnology && index != request.technology) {
            continue;
        }
        const std::uint32_t desired = request.action == domain::CityAction::MaximizeTechnology
            ? domain::CITY_TECHNOLOGY_LIMITS[index]
            : std::min<std::uint32_t>(request.value, domain::CITY_TECHNOLOGY_LIMITS[index]);

        writes[count++] = {faction + 0x205 + index, levels[index], desired, 1};
    }
    const domain::CityWorkStatus result = write_transaction(state, std::span(writes).first(count), true);

    if (result == domain::CityWorkStatus::Complete) {
        read_faction_technology(state, request.city);
    }
    return result;
}

domain::CityWorkStatus exchange_city(CityManagement& state, std::uint8_t city) {
    std::array<std::uint32_t, 2> resources{};
    const std::uintptr_t address = state.city_table + city * CITY_STRIDE + 0xbc;
    const domain::CitySettings& settings = state.snapshot.settings;

    if (!read_value(address, resources)) {
        return domain::CityWorkStatus::ReadFailed;
    }
    if (resources[1] <= settings.grain_limit || resources[1] <= settings.retained_grain) {
        return domain::CityWorkStatus::NoResources;
    }
    const std::uint32_t excess = resources[1] - settings.retained_grain;
    const std::uint32_t requested = excess / settings.exchange_rate;
    const std::uint32_t room = std::numeric_limits<std::uint32_t>::max() - resources[0];
    const std::uint32_t gain = std::min(requested, room);
    const std::uint32_t retained = requested <= room ? settings.retained_grain
        : resources[1] - gain * settings.exchange_rate;
    const std::array<CityDataWrite, 2> writes{{
        {address + 4, resources[1], retained}, {address, resources[0], resources[0] + gain}}};
    const domain::CityWorkStatus result = write_transaction(state, writes);

    if (result == domain::CityWorkStatus::Complete && state.snapshot.cities[city].valid) {
        state.snapshot.cities[city].values[0] = resources[0] + gain;
        state.snapshot.cities[city].values[1] = retained;
    }
    return result;
}

void observe_calendar(CityManagement& state, std::uint64_t now) {
    if (now < state.next_maintenance_ms) {
        return;
    }
    state.next_maintenance_ms = now + 500;
    std::uintptr_t manager = 0;
    std::array<std::uint8_t, 3> date{};
    std::uint8_t phase = 0;

    if (!read_value(state.module_base + 0x2e98bc8, manager) || manager <= 0x10000 ||
        !read_value(manager + 0x72d0, date) || date[2] < 1 || date[2] > 12) {
        return;
    }
    const std::uint16_t year = static_cast<std::uint16_t>(date[0] | (static_cast<std::uint16_t>(date[1]) << 8));
    const std::uint8_t month = date[2];

    if (state.observed_month == 12 && month == 1 && year >= state.observed_year &&
        state.snapshot.routes && state.snapshot.routes->yearly_auto) {
        state.pending |= PENDING_ROUTES;
    }
    state.observed_month = month;
    state.observed_year = year;
    if (state.snapshot.year != year || state.snapshot.month != month) {
        ++state.snapshot.revision;
    }
    state.snapshot.year = year;
    state.snapshot.month = month;
    if (read_value(state.game_root + 0xd0, phase)) {
        if (phase == 7) {
            state.exchanged_month = 0xff;
        } else if (phase == 5 && month != state.exchanged_month) {
            if (state.snapshot.settings.auto_exchange) {
                state.pending |= PENDING_EXCHANGE;
            }
            state.exchanged_month = month;
        }
    }
}

void begin_pending(CityManagement& state) {
    if ((state.pending & PENDING_ROUTES) != 0) {
        state.pending &= ~PENDING_ROUTES;
        state.active_routes = state.snapshot.routes;
        state.route_force = player_force(state.player);
        state.annual_routes = true;
        start_work(state, CityWork::Routes);
        state.snapshot.total = state.active_routes ? state.active_routes->routes.size() : 0;
    } else if ((state.pending & PENDING_EXCHANGE) != 0) {
        state.pending &= ~PENDING_EXCHANGE;
        start_work(state, CityWork::Exchange);
    } else if ((state.pending & PENDING_REVOLT) != 0) {
        state.pending &= ~PENDING_REVOLT;
        // Maintenance must not inherit an earlier manual ResetRevolt request.
        state.request = {};
        start_work(state, CityWork::Revolt);
    } else if ((state.pending & PENDING_REFRESH) != 0) {
        state.pending &= ~PENDING_REFRESH;
        start_work(state, CityWork::Refresh);
    } else if ((state.pending & PENDING_TECHNOLOGY) != 0) {
        state.pending &= ~PENDING_TECHNOLOGY;
        state.faction_table = resolve_faction_table(state.module_base);
        if (state.faction_table == 0) {
            end_work(state, domain::CityWorkStatus::ReadFailed);
            return;
        }
        start_work(state, CityWork::Technology);
        state.snapshot.total = domain::CITY_FACTION_COUNT;
    }
}

} // namespace

std::uintptr_t resolve_city_data(std::uintptr_t module_base) {
    if (!module_base || module_base > std::numeric_limits<std::uintptr_t>::max() - 0x2e98bc8) {
        return 0;
    }
    std::uintptr_t owner = 0;
    std::array<std::uintptr_t, domain::CITY_COUNT + 1> cities{};

    if (!read_value(module_base + 0x2e98bc8, owner) || owner <= 0x10000 ||
        owner > std::numeric_limits<std::uintptr_t>::max() - 0x5469a8 ||
        !read_value(owner + 0x546808, cities)) {
        return 0;
    }
    const std::uintptr_t table = cities[1];

    if (table <= 0x10000 ||
        table > std::numeric_limits<std::uintptr_t>::max() - domain::CITY_COUNT * CITY_STRIDE) {
        return 0;
    }
    // Validate the external pointer array once, before using contiguous records.
    for (std::size_t index = 0; index < domain::CITY_COUNT; ++index) {
        if (cities[index + 1] != table + index * CITY_STRIDE) {
            return 0;
        }
    }
    return table;
}

std::uintptr_t read_city_force(std::uintptr_t raw_city) {
    std::uintptr_t link = 0;
    std::uintptr_t force = 0;

    if (raw_city <= 0x10000) {
        return 0;
    }
    if (read_value(raw_city + 0x98, link) && link > 0x10000 && read_value(link + 0x18, force) && force > 0x10000) {
        return force;
    }
    if (read_value(raw_city + 0x90, link) && link > 0x10000 && read_value(link + 0x10, force) && force > 0x10000) {
        return force;
    }
    return 0;
}

std::uint8_t city_index_from_pointer(std::uintptr_t city_table, std::uintptr_t raw_city) {
    if (city_table <= 0x10000 || raw_city < city_table) {
        return domain::NO_CITY;
    }
    const std::uintptr_t delta = raw_city - city_table;

    return delta % CITY_STRIDE == 0 && delta / CITY_STRIDE < domain::CITY_COUNT
        ? static_cast<std::uint8_t>(delta / CITY_STRIDE) : domain::NO_CITY;
}

bool set_city_settings(CityManagement& state, const domain::CitySettings& settings) {
    if (state.stopping || !domain::valid_city_settings(settings)) {
        return false;
    }
    if (!settings.revolt_always_zero) {
        state.pending &= ~PENDING_REVOLT;
        if (state.work == CityWork::Revolt) {
            end_work(state, domain::CityWorkStatus::Cancelled);
        }
    } else if (!state.snapshot.settings.revolt_always_zero) {
        state.pending |= PENDING_REVOLT;
    }
    if (!settings.auto_exchange) {
        state.pending &= ~PENDING_EXCHANGE;
        if (state.work == CityWork::Exchange) {
            end_work(state, domain::CityWorkStatus::Cancelled);
        }
    }
    state.snapshot.settings = settings;
    ++state.snapshot.revision;
    return true;
}

bool set_city_support_routes(CityManagement& state, own::shared_owner<const domain::CitySupportRoutes> routes) {
    if (state.stopping || !routes) {
        return false;
    }
    // The service validates file content; each route is checked again at its
    // commit boundary. Publishing an owner does not walk an unbounded list.
    if (!routes->yearly_auto) {
        state.pending &= ~PENDING_ROUTES;
        if (state.annual_routes && state.work == CityWork::Routes) {
            end_work(state, domain::CityWorkStatus::Cancelled);
        }
    }
    state.snapshot.routes = std::move(routes);
    ++state.snapshot.revision;
    return true;
}

domain::CityWorkStatus request_city_management(CityManagement& state, std::uintptr_t module_base,
    std::uintptr_t game_root, std::uintptr_t player, domain::GameEpoch epoch, const domain::CityRequest& request) {
    if (state.stopping) {
        return domain::CityWorkStatus::Cancelled;
    }
    if (request.action == domain::CityAction::Cancel) {
        state.pending = 0;
        end_work(state, domain::CityWorkStatus::Cancelled);
        return domain::CityWorkStatus::Cancelled;
    }
    const bool previously_bound = state.epoch == epoch && state.module_base == module_base &&
        state.game_root == game_root && state.player == player && state.city_table != 0;
    const std::uintptr_t previous_table = state.city_table;

    if (!bind_city_session(state, module_base, game_root, player, epoch)) {
        return domain::CityWorkStatus::ReadFailed;
    }
    if (!basic_city_action(request.action) && !scenario_ready(game_root, player)) {
        return domain::CityWorkStatus::ReadFailed;
    }
    if (request.action != domain::CityAction::Refresh && (!previously_bound || previous_table != state.city_table)) {
        return domain::CityWorkStatus::SessionChanged;
    }
    if (state.recovery_count != 0) {
        return domain::CityWorkStatus::RollbackFailed;
    }
    if (request.action == domain::CityAction::Refresh || request.action == domain::CityAction::RefreshTechnology) {
        state.pending |= request.action == domain::CityAction::Refresh ? PENDING_REFRESH : PENDING_TECHNOLOGY;
        return domain::CityWorkStatus::Working;
    }
    if (state.work != CityWork::Idle) {
        if (state.snapshot.locks_commands) {
            return domain::CityWorkStatus::Busy;
        }
        state.pending |= state.work == CityWork::Revolt ? PENDING_REVOLT
            : state.work == CityWork::Exchange ? PENDING_EXCHANGE
            : state.work == CityWork::Technology ? PENDING_TECHNOLOGY : PENDING_REFRESH;
        end_work(state, domain::CityWorkStatus::Cancelled);
    }
    state.request = request;
    switch (request.action) {
    case domain::CityAction::ConvertSelected:
        if (request.value > 3 || request.selected_cities == 0 ||
            (request.selected_cities >> domain::CITY_COUNT) != 0) {
            return domain::CityWorkStatus::InvalidRequest;
        }
        start_work(state, CityWork::ConvertSelected);
        break;
    case domain::CityAction::WriteTechnology: case domain::CityAction::MaximizeTechnology: {
        const domain::CityWorkStatus result = write_faction_technology(state, request);

        state.snapshot.last_request = result;
        ++state.snapshot.revision;
        return result;
    }
    case domain::CityAction::Write: {
        if (request.city >= domain::CITY_COUNT || request.field >= domain::CityField::Count) {
            return domain::CityWorkStatus::InvalidRequest;
        }
        const domain::CityWorkStatus result = write_city_field(state, request.city, request.field, request.value);

        state.snapshot.last_request = result;
        ++state.snapshot.revision;
        return result;
    }
    case domain::CityAction::Maximize:
        if (request.field >= domain::CityField::Count) {
            return domain::CityWorkStatus::InvalidRequest;
        }
        start_work(state, CityWork::Maximize);
        break;
    case domain::CityAction::ResetRevolt:
        start_work(state, CityWork::Revolt);
        break;
    case domain::CityAction::Support:
        if (!domain::valid_city_route(request.support)) {
            return domain::CityWorkStatus::InvalidRequest;
        }
        state.route_force = read_city_force(state.city_table + request.support.source * CITY_STRIDE);
        if (request.force_lord == domain::NO_CITY_OFFICER || force_lord(state.route_force) != request.force_lord) {
            return domain::CityWorkStatus::OwnershipChanged;
        }
        start_work(state, CityWork::Support);
        state.snapshot.total = 1;
        break;
    case domain::CityAction::RunRoutes:
        state.route_force = player_force(player);
        if (request.force_lord != domain::NO_CITY_OFFICER) {
            state.route_force = 0;
            for (std::size_t city = 0; city < domain::CITY_COUNT; ++city) {
                const std::uintptr_t force = read_city_force(state.city_table + city * CITY_STRIDE);

                if (force_lord(force) == request.force_lord) {
                    state.route_force = force;
                    break;
                }
            }
            if (state.route_force == 0) {
                return domain::CityWorkStatus::OwnershipChanged;
            }
        }
        state.active_routes = state.snapshot.routes;
        state.annual_routes = false;
        start_work(state, CityWork::Routes);
        state.snapshot.total = state.active_routes ? state.active_routes->routes.size() : 0;
        break;
    default:
        return domain::CityWorkStatus::InvalidRequest;
    }
    return domain::CityWorkStatus::Working;
}

void tick_city_management(CityManagement& state, std::uintptr_t module_base, std::uintptr_t game_root,
    std::uintptr_t player, domain::GameEpoch epoch, std::uint64_t real_milliseconds, std::size_t max_items,
    const domain::WorkBudget& work_budget) {
    if (state.stopping || !bind_city_session(state, module_base, game_root, player, epoch, false)) {
        return;
    }
    if (state.recovery_count != 0 && !restore_bound_data(state)) {
        state.snapshot.status = domain::CityWorkStatus::RollbackFailed;
        return;
    }
    const bool live_scenario = scenario_ready(game_root, player);
    if (live_scenario) {
        observe_calendar(state, real_milliseconds);
        if (state.work != CityWork::Revolt && state.snapshot.settings.revolt_always_zero &&
            real_milliseconds >= state.next_revolt_ms) {
            state.pending |= PENDING_REVOLT;
            state.next_revolt_ms = real_milliseconds + 200;
        }
    } else {
        state.pending &= PENDING_REFRESH;
        // Only explicit edits of the static city table may run before a
        // scenario. Automatic maintenance, faction edits and support wait.
        const bool manual_revolt = state.work == CityWork::Revolt &&
            state.request.action == domain::CityAction::ResetRevolt;
        if (state.work != CityWork::Idle && state.work != CityWork::Refresh &&
            state.work != CityWork::Maximize && state.work != CityWork::ConvertSelected && !manual_revolt) {
            end_work(state, domain::CityWorkStatus::SessionChanged);
        }
    }
    if (state.work == CityWork::Idle) {
        begin_pending(state);
    }
    if (state.work == CityWork::Refresh && max_items != 0) {
        // A refresh is one bounded snapshot, not 51 separately scheduled reads.
        const std::uintptr_t table = resolve_city_data(module_base);
        std::array<std::array<std::byte, CITY_STRIDE>, domain::CITY_COUNT> records{};

        if (table == 0 || table != state.city_table) {
            end_work(state, table == 0 ? domain::CityWorkStatus::ReadFailed : domain::CityWorkStatus::SessionChanged);
            return;
        }
        if (!read_value(table, records)) {
            end_work(state, domain::CityWorkStatus::ReadFailed);
            return;
        }
        for (std::size_t index = 0; index < records.size(); ++index) {
            if (from_record<std::uint8_t>(records[index], 8) != index + 1) {
                end_work(state, domain::CityWorkStatus::ReadFailed);
                return;
            }
        }
        CityRefreshLinks links;

        for (std::size_t index = 0; index < records.size(); ++index) {
            read_city_row(state, index, &records[index], &links);
        }
        update_frontiers(state);
        state.cursor = domain::CITY_COUNT;
        state.snapshot.completed = domain::CITY_COUNT;
        state.snapshot.succeeded = domain::CITY_COUNT;
        end_work(state, domain::CityWorkStatus::Complete);
        return;
    }
    if (state.work == CityWork::Technology && resolve_faction_table(module_base) != state.faction_table) {
        state.snapshot.technologies_loaded = false;
        end_work(state, domain::CityWorkStatus::SessionChanged);
        return;
    }
    // The runtime service admits this work before resolving its context. Include
    // one item in that admitted unit so binding cannot consume every slice.
    for (std::size_t step = 0; step < max_items && state.work != CityWork::Idle && (step == 0 || !work_budget.exhausted()); ++step) {
        domain::CityWorkStatus result = domain::CityWorkStatus::Complete;

        if (state.work == CityWork::Routes || state.work == CityWork::Support) {
            if (state.work == CityWork::Routes && state.annual_routes &&
                (state.snapshot.month != 1 || player_force(state.player) != state.route_force)) {
                end_work(state, domain::CityWorkStatus::OwnershipChanged);
                break;
            }
            if (state.work == CityWork::Support) {
                result = transfer_support(state, state.request.support, state.route_force, false);
                state.cursor = 1;
            } else if (!state.active_routes || state.cursor >= state.active_routes->routes.size()) {
                end_work(state, domain::CityWorkStatus::Complete);
                break;
            } else {
                const domain::CitySupportRoute& route = state.active_routes->routes[state.cursor++];

                result = route.enabled ? transfer_support(state, route, state.route_force, true)
                    : domain::CityWorkStatus::NoResources;
            }
        } else {
            const std::uint8_t city = static_cast<std::uint8_t>(state.cursor++);

            switch (state.work) {
            case CityWork::Technology:
                result = read_faction_technology(state, city) ? domain::CityWorkStatus::Complete : domain::CityWorkStatus::ReadFailed;
                break;
            case CityWork::Maximize:
                result = write_city_field(state, city, state.request.field, domain::city_field_maximum(state.request.field));
                break;
            case CityWork::ConvertSelected: {
                if ((state.request.selected_cities & (std::uint64_t{1} << city)) == 0) {
                    result = domain::CityWorkStatus::NoResources;
                    break;
                }
                // Native city type byte and the same limit profiles used by city_conversion.cpp.
                constexpr std::array<std::uint8_t, 4> TYPES{128, 208, 48, 88};
                constexpr std::array<std::array<std::uint16_t, 4>, 4> LIMITS{{
                    {9000, 12000, 9000, 4000}, {1500, 3000, 4500, 1500},
                    {6500, 5000, 0, 0}, {5000, 6500, 0, 0}}};
                const std::uintptr_t address = state.city_table + city * CITY_STRIDE;
                std::array<CityDataWrite, 5> writes{};
                const std::size_t count = state.request.value < 2 ? 5 : 3;
                writes[0] = {address + 0x10, 0, TYPES[state.request.value], 1};
                for (std::size_t index = 1; index < count; ++index) {
                    writes[index] = {address + 0xd0 + (index - 1) * 4, 0,
                        LIMITS[state.request.value][index - 1], 2};
                }
                bool read = true;
                for (CityDataWrite& write : std::span(writes).first(count)) {
                    if (!platform::windows::read_memory(write.address,
                            std::as_writable_bytes(std::span(&write.before, 1)).first(write.width))) {
                        read = false;
                        break;
                    }
                }
                result = read ? write_transaction(state, std::span(writes).first(count)) : domain::CityWorkStatus::ReadFailed;
                if (result == domain::CityWorkStatus::Complete) read_city_row(state, city);
                break;
            }
            case CityWork::Revolt:
                result = write_city_field(state, city, domain::CityField::Revolt, 0);
                break;
            case CityWork::Exchange:
                result = exchange_city(state, city);
                break;
            default:
                break;
            }
        }
        state.snapshot.completed = state.cursor;
        if (result == domain::CityWorkStatus::Complete) {
            ++state.snapshot.succeeded;
        } else if (result != domain::CityWorkStatus::NoResources) {
            state.snapshot.last_request = result;
            state.work_result = result;
        }
        if (result == domain::CityWorkStatus::RollbackFailed) {
            end_work(state, result);
            break;
        }
        if (state.cursor >= state.snapshot.total) {
            if (state.work == CityWork::Technology) {
                state.snapshot.technologies_loaded = true;
            }
            if (state.work == CityWork::Revolt) {
                state.next_revolt_ms = real_milliseconds + 200;
            }
            end_work(state, state.work == CityWork::Support ? result : state.work_result);
            break;
        }
        ++state.snapshot.revision;
        if (state.work == CityWork::Routes) {
            break;
        }
    }
}

bool stop_city_management(CityManagement& state, std::uintptr_t module_base, domain::GameEpoch epoch) {
    state.stopping = true;
    state.pending = 0;
    if (state.recovery_count != 0) {
        const std::uintptr_t table = resolve_city_data(module_base);

        if (epoch != state.epoch || module_base != state.module_base || (table != 0 && table != state.city_table)) {
            state.recovery_count = 0;
        } else if (table == 0 || !restore_bound_data(state)) {
            return false;
        }
    }
    end_work(state, domain::CityWorkStatus::Cancelled);
    return true;
}

} // namespace s8::game
