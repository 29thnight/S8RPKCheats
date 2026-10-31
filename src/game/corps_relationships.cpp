#include "corps_relationships.h"

#include "corps_plan.h"
#include "../platform/windows/process_memory.h"

#include <array>
#include <cstring>
#include <limits>
#include <span>

namespace s8::game {
namespace {

constexpr std::uintptr_t OFFICER_STRIDE = 0x3d0;
constexpr std::array<std::uintptr_t, 2> RELATION_OFFSETS{0x57ae38, 0x462ba8};
constexpr std::array<std::uintptr_t, 2> SYNERGY_OFFSETS{0x547330, 0x433210};
constexpr std::array<std::uintptr_t, 2> RELATION_BIASES{0x80, 0xc0};
constexpr std::size_t PAIR_PROBE_LIMIT = 1024;
constexpr std::uint32_t NO_EDGE = 0xffffffff;

template<class T> bool read_value(std::uintptr_t address, T& value) {
    return platform::windows::read_memory(address, std::as_writable_bytes(std::span(&value, 1)));
}

template<class T, std::size_t N> T from_bytes(const std::array<std::byte, N>& bytes, std::size_t offset) {
    T result{};

    std::memcpy(&result, bytes.data() + offset, sizeof(result));
    return result;
}

std::uint16_t roster_row(const CorpsDeployment& state, std::uintptr_t pointer) {
    if (pointer < state.roster) { return NO_CORPS_ROW; }
    const std::uintptr_t delta = pointer - state.roster;

    return delta % OFFICER_STRIDE == 0 && delta / OFFICER_STRIDE < domain::CORPS_OFFICER_CAPACITY
        ? static_cast<std::uint16_t>(delta / OFFICER_STRIDE) : NO_CORPS_ROW;
}

std::uint32_t pair_key(std::uint16_t first, std::uint16_t second) {
    return first < second ? (std::uint32_t(first) << 16) | second : (std::uint32_t(second) << 16) | first;
}

bool remember_pair(CorpsDeployment& state, std::uint16_t first, std::uint16_t second) {
    const std::uint32_t key = pair_key(first, second);
    std::size_t index = (key * 2654435761u) >> 16;

    for (std::size_t probe = 0; probe < PAIR_PROBE_LIMIT; ++probe) {
        std::uint32_t& current = state.direct_pairs[index];

        if (current == key) { return true; }
        if (current == 0) { current = key; return true; }
        index = (index + 1) & (state.direct_pairs.size() - 1);
    }
    return false;
}

bool has_direct_pair(const CorpsDeployment& state, std::uint16_t first, std::uint16_t second) {
    const std::uint32_t key = pair_key(first, second);
    std::size_t index = (key * 2654435761u) >> 16;

    for (std::size_t probe = 0; probe < PAIR_PROBE_LIMIT; ++probe) {
        const std::uint32_t current = state.direct_pairs[index];

        if (current == key) { return true; }
        if (current == 0) { return false; }
        index = (index + 1) & (state.direct_pairs.size() - 1);
    }
    return true;
}

bool append_edge(CorpsDeployment& state, std::uint16_t first, std::uint16_t second, std::size_t category) {
    if (state.relationship_count == state.relationship_edges.size()) { return false; }
    const std::uint32_t edge = static_cast<std::uint32_t>(state.relationship_count++);
    std::uint32_t& head = state.relationship_heads[category][first];
    std::uint32_t& tail = state.relationship_tails[category][first];

    state.relationship_edges[edge] = {NO_EDGE, second};
    if (tail == NO_EDGE) { head = edge; }
    else { state.relationship_edges[tail].next = edge; }
    tail = edge;
    return true;
}

bool append_pair(CorpsDeployment& state, std::uint16_t first, std::uint16_t second, std::size_t category) {
    if (append_edge(state, first, second, category) && append_edge(state, second, first, category)) { return true; }
    state.failure = domain::CorpsStatus::CapacityExceeded;
    state.stage = CorpsStage::Idle;
    return false;
}

void begin_merge(CorpsDeployment& state) {
    state.cursor = 0;
    state.secondary = 0;
    state.relationship_cursor = state.count == 0 ? NO_EDGE : state.relationship_heads[0][state.order[0]];
    state.stage = CorpsStage::RelationMerge;
}

void reset_candidate(CorpsDeployment& state, CorpsStage stage, std::uintptr_t candidate) {
    state.candidate = candidate;
    state.cursor = 0;
    state.candidate_valid = 0;
    state.candidate_invalid = 0;
    state.stage = stage;
}

void begin_synergy(CorpsDeployment& state) {
    state.candidate_index = 0;
    state.synergy_scan_cursor = 0;
    state.stage = CorpsStage::SynergyCandidate;
}

bool requested_relationships_complete(const CorpsDeployment& state) {
    return (!(state.snapshot.options.group_sworn || state.snapshot.options.group_spouses) || state.relation_table != 0) &&
        (!state.snapshot.options.group_synergetic || (state.relation_table != 0 && state.synergy_table != 0));
}

} // namespace

void begin_corps_relationships(CorpsDeployment& state) {
    state.relation_table = 0;
    state.synergy_table = 0;
    state.candidate_index = 0;
    state.direct_pairs.fill(0);
    state.relationship_count = 0;
    for (std::array<std::uint32_t, domain::CORPS_OFFICER_CAPACITY>& heads : state.relationship_heads) { heads.fill(NO_EDGE); }
    for (std::array<std::uint32_t, domain::CORPS_OFFICER_CAPACITY>& tails : state.relationship_tails) { tails.fill(NO_EDGE); }
    state.stage = CorpsStage::RelationCandidate;
    state.snapshot.relationships_complete = false;
}

bool step_corps_relationships(CorpsDeployment& state) {
    if (!state.snapshot.options.group_sworn && !state.snapshot.options.group_spouses && !state.snapshot.options.group_synergetic) {
        state.snapshot.relationships_complete = true;
        return true;
    }
    if (state.stage == CorpsStage::RelationCandidate) {
        if (state.candidate_index == RELATION_OFFSETS.size() * RELATION_BIASES.size()) { begin_synergy(state); return false; }
        const std::size_t slot = state.candidate_index++;
        std::uintptr_t pointer = 0;

        if (read_value(state.game_root + RELATION_OFFSETS[slot / 2], pointer) && pointer > 0x10000 + RELATION_BIASES[slot % 2]) {
            reset_candidate(state, CorpsStage::RelationValidate, pointer - RELATION_BIASES[slot % 2]);
        }
    } else if (state.stage == CorpsStage::RelationValidate) {
        std::array<std::byte, 0x40> record{};

        if (state.cursor == 128) {
            if (state.candidate_valid != 0) { state.relation_table = state.candidate; state.cursor = 0; state.stage = CorpsStage::RelationScan; }
            else { state.stage = CorpsStage::RelationCandidate; }
            return false;
        }
        if (!read_value(state.candidate + state.cursor++ * record.size(), record)) { state.stage = CorpsStage::RelationCandidate; return false; }
        const std::uint8_t kind = from_bytes<std::uint8_t>(record, 8);

        if (kind == 0) { return false; }
        if (kind > 4 || roster_row(state, from_bytes<std::uintptr_t>(record, 0x10)) == NO_CORPS_ROW ||
            roster_row(state, from_bytes<std::uintptr_t>(record, 0x18)) == NO_CORPS_ROW) {
            if (++state.candidate_invalid > 3) { state.stage = CorpsStage::RelationCandidate; }
        } else { ++state.candidate_valid; }
    } else if (state.stage == CorpsStage::RelationScan) {
        if (state.cursor == 3000) { begin_synergy(state); return false; }
        std::array<std::byte, 0x40> record{};
        std::array<std::uint16_t, 5> members{};

        if (!read_value(state.relation_table + state.cursor++ * record.size(), record)) {
            state.relation_table = 0;
            begin_synergy(state);
            return false;
        }
        const std::uint8_t kind = from_bytes<std::uint8_t>(record, 8);

        if (kind < 1 || kind > 4) { return false; }
        for (std::size_t slot = 0; slot < members.size(); ++slot) {
            members[slot] = roster_row(state, from_bytes<std::uintptr_t>(record, 0x10 + slot * sizeof(std::uintptr_t)));
        }
        for (std::size_t first = 0; first < members.size(); ++first) {
            if (members[first] == NO_CORPS_ROW) { continue; }
            const CorpsRosterEntry& left = state.rows[members[first]];

            if (left.id == 0 || left.id > domain::CORPS_OFFICER_CAPACITY) { continue; }
            for (std::size_t second = first + 1; second < members.size(); ++second) {
                if (members[second] == NO_CORPS_ROW || members[first] == members[second]) { continue; }
                const CorpsRosterEntry& right = state.rows[members[second]];

                if (right.id == 0 || right.id > domain::CORPS_OFFICER_CAPACITY) { continue; }
                if (!remember_pair(state, left.id, right.id)) {
                    state.failure = domain::CorpsStatus::CapacityExceeded;
                    state.stage = CorpsStage::Idle;
                    return false;
                }
                if (left.selected && right.selected &&
                    ((kind == 1 && state.snapshot.options.group_sworn) || (kind == 2 && state.snapshot.options.group_spouses))) {
                    if (!append_pair(state, members[first], members[second], kind == 1 ? 0 : 2)) { return false; }
                }
            }
        }
    } else if (state.stage == CorpsStage::SynergyCandidate) {
        if (!state.snapshot.options.group_synergetic) {
            state.snapshot.relationships_complete = requested_relationships_complete(state);
            begin_merge(state);
            return false;
        }
        std::uintptr_t pointer = 0;

        if (state.candidate_index < SYNERGY_OFFSETS.size()) {
            read_value(state.game_root + SYNERGY_OFFSETS[state.candidate_index++], pointer);
        } else {
            if (state.synergy_scan_cursor == 0) { state.synergy_scan_cursor = state.game_root; }
            if (state.synergy_scan_cursor - state.game_root >= 0x800000) {
                state.snapshot.relationships_complete = false;
                begin_merge(state);
                return false;
            }
            read_value(state.synergy_scan_cursor, pointer);
            state.synergy_scan_cursor += sizeof(std::uintptr_t);
        }
        if (pointer > 0x10000 && pointer <= std::numeric_limits<std::uintptr_t>::max() - 0xa0 - 5000 * 0x20) {
            reset_candidate(state, CorpsStage::SynergyValidate, pointer + 0xa0);
        }
    } else if (state.stage == CorpsStage::SynergyValidate) {
        if (state.cursor == 600) {
            if (state.candidate_valid != 0) { state.synergy_table = state.candidate; state.cursor = 0; state.stage = CorpsStage::SynergyScan; }
            else { state.stage = CorpsStage::SynergyCandidate; }
            return false;
        }
        std::array<std::byte, 0x20> record{};

        if (!read_value(state.candidate + state.cursor++ * record.size(), record)) { state.stage = CorpsStage::SynergyCandidate; return false; }
        const std::uint8_t kind = from_bytes<std::uint8_t>(record, 0x18);

        if (kind == 0) { return false; }
        if (kind > 2 || roster_row(state, from_bytes<std::uintptr_t>(record, 8)) == NO_CORPS_ROW ||
            roster_row(state, from_bytes<std::uintptr_t>(record, 0x10)) == NO_CORPS_ROW) {
            if (++state.candidate_invalid > 3) { state.stage = CorpsStage::SynergyCandidate; }
        } else { ++state.candidate_valid; }
    } else if (state.stage == CorpsStage::SynergyScan) {
        if (state.cursor == 5000) {
            state.snapshot.relationships_complete = requested_relationships_complete(state);
            begin_merge(state);
            return false;
        }
        std::array<std::byte, 0x20> record{};

        if (!read_value(state.synergy_table + state.cursor++ * record.size(), record)) {
            state.synergy_table = 0;
            state.snapshot.relationships_complete = false;
            begin_merge(state);
            return false;
        }
        if (from_bytes<std::uint8_t>(record, 0x18) != 2 || from_bytes<std::uint8_t>(record, 0x19) != 0) { return false; }
        const std::uint16_t first = roster_row(state, from_bytes<std::uintptr_t>(record, 8));
        const std::uint16_t second = roster_row(state, from_bytes<std::uintptr_t>(record, 0x10));

        if (first != NO_CORPS_ROW && second != NO_CORPS_ROW && first != second && state.rows[first].selected && state.rows[second].selected &&
            !has_direct_pair(state, state.rows[first].id, state.rows[second].id)) {
            if (!append_pair(state, first, second, 1)) { return false; }
        }
    } else if (state.stage == CorpsStage::RelationMerge) {
        if (state.cursor == state.count) { return true; }
        if (state.relationship_cursor != NO_EDGE) {
            const CorpsRelationshipEdge& edge = state.relationship_edges[state.relationship_cursor];

            unite_corps_group(state, state.order[state.cursor], edge.target);
            state.relationship_cursor = edge.next;
        } else {
            if (++state.secondary == state.relationship_heads.size()) { state.secondary = 0; ++state.cursor; }
            if (state.cursor != state.count) {
                state.relationship_cursor = state.relationship_heads[state.secondary][state.order[state.cursor]];
            }
        }
    }
    return false;
}

} // namespace s8::game
