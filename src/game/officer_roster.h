#pragma once

#include "../domain/officer_roster.h"
#include "../domain/officer_roster_publication.h"
#include "../domain/work_budget.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace s8::game {

struct OfficerRosterScan {
    std::uintptr_t base = 0;
    std::size_t cursor = 0;
    std::array<std::uint16_t, domain::OFFICER_ROSTER_ID_SLOTS> slots{};
    std::array<domain::OfficerRosterEntry, domain::OFFICER_ROSTER_ID_SLOTS> entries{};
    static constexpr std::size_t DIRTY_WORDS = (domain::OFFICER_ROSTER_ID_SLOTS + 63) / 64;
    std::array<std::uint64_t, DIRTY_WORDS> dirty{};
    std::size_t dirty_count = 0;
    std::size_t publication_word = 0;
    std::uint64_t generation = 0;
    std::optional<domain::GameEpoch> publication_epoch;
    domain::OfficerRosterUpdate publication;
    bool publication_pending = false;
    bool reset_pending = false;
    bool has_scan = false;
    bool requested = false;
    bool completed = false;
};

struct TraitObjectSearch {
    std::uintptr_t cursor = 0;
    std::uintptr_t vtable = 0;
    std::uintptr_t result = 0;
    std::uint16_t trait_id = 0;
    bool complete = false;
    static constexpr std::size_t BLOCK_SIZE = 4096;
    std::array<std::byte, BLOCK_SIZE + sizeof(std::uint16_t)> bytes{};
};

void scan_trait_object(TraitObjectSearch& search, std::size_t max_bytes,
    const domain::WorkBudget& budget = {});

void reset_officer_roster(OfficerRosterScan& scan);
void request_officer_roster(OfficerRosterScan& scan);
void update_officer_roster(OfficerRosterScan& scan, std::uintptr_t module_base,
    const domain::WorkBudget& budget = {});
bool update_roster_officer_status(OfficerRosterScan& scan, std::uint16_t id, std::uint8_t status);
// The returned member is borrowed until the producer next changes the scan.
// A full channel keeps it pending; acknowledge only after a successful push.
const domain::OfficerRosterUpdate* peek_officer_roster_update(OfficerRosterScan& scan, domain::GameEpoch epoch);
void acknowledge_officer_roster_update(OfficerRosterScan& scan);
domain::OfficerTraits read_officer_traits(std::uintptr_t officer);
std::uintptr_t resolve_officer_roster_base(std::uintptr_t module_base);
std::uintptr_t resolve_trait_target(const OfficerRosterScan& scan, std::uintptr_t module_base,
                                    std::uint16_t id, std::uintptr_t player);
std::optional<domain::OfficerDetail> read_roster_officer(const OfficerRosterScan& scan,
                                                         std::uintptr_t module_base, std::uint16_t id);
bool roster_traits_changed(const OfficerRosterScan& scan, std::uintptr_t module_base,
                           const domain::OfficerDetail& detail);
bool write_roster_officer_basic(const OfficerRosterScan& scan, std::uintptr_t module_base,
                               std::uint16_t id, std::uint8_t index, std::uint32_t value);
bool write_roster_officer_gender(const OfficerRosterScan& scan, std::uintptr_t module_base,
                                std::uint16_t id, std::uint8_t gender);
bool write_roster_officer_voice(const OfficerRosterScan& scan, std::uintptr_t module_base,
                               std::uint16_t id, std::uint8_t voice);
bool set_roster_officer_ronin(const OfficerRosterScan& scan, std::uintptr_t module_base,
                               std::uint16_t id, bool undiscovered_only);
bool move_roster_officer(const OfficerRosterScan& scan, std::uintptr_t module_base,
                         std::uint16_t id, std::uint8_t city);
bool revive_roster_officer(const OfficerRosterScan& scan, std::uintptr_t module_base,
                           std::uintptr_t player, std::uint16_t id, std::optional<std::uint8_t> city);
bool set_officer_trait_direct(std::uintptr_t officer, std::uintptr_t game_base,
                              std::uint8_t slot, std::uint16_t trait_id);
std::uintptr_t resolve_officer_trait_vtable(std::uintptr_t officer, std::uintptr_t game_base);
bool set_officer_trait_object(std::uintptr_t officer, std::uint8_t slot, std::uint16_t trait_id,
                              std::uintptr_t trait, std::uintptr_t expected_vtable);
bool write_roster_officer_training(const OfficerRosterScan& scan, std::uintptr_t module_base,
                                  std::uint16_t id, domain::OfficerTrainingKind kind,
                                  std::uint8_t index, std::uint8_t value);

} // namespace s8::game
