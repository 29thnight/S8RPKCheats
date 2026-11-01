#pragma once

#include "ids.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <variant>

namespace s8::domain {

inline constexpr std::size_t OFFICER_TOOL_LIMIT = 5102;
inline constexpr std::size_t FAMILY_CHILD_PAGE = 64;
inline constexpr std::size_t FAMILY_SPOUSE_PAGE = 32;

enum class OfficerToolStatus {
    Idle, Preparing, Applying, Completed, Cancelled, SessionChanged,
    InvalidRequest, Unavailable, ReadFailed, WriteFailed, RollbackFailed, Conflict
};

enum class OfficerToolJob { None, Training, RandomTraits };

struct TrainingChoice {
    bool enabled = false;
    std::uint8_t level = 0;
};

struct BatchTrainingRequest {
    std::array<TrainingChoice, 35> tactics{};
    std::array<TrainingChoice, 24> skills{};
};

struct RandomTraitsRequest {
    std::array<bool, 3> grades{true, true, false};
    // Zero selects the full roster; a nonzero ID selects one officer.
    std::uint16_t officer_id = 0;
};

struct CancelOfficerToolJob {};
struct RefreshFamilyTools {};
struct SetChildSchedule {
    std::uint16_t officer_id = 0;
    std::uint8_t years_later = 3;
    bool enabled = false;
};
struct SetPregnancySpouse {
    std::uint8_t slot = 0;
    std::uint16_t expected_spouse = 0;
    std::uint16_t new_spouse = 0;
};
struct SwapSpouseOrder {
    std::uint16_t first_spouse = 0;
    std::uint16_t second_spouse = 0;
    std::uint16_t first_order = 0;
    std::uint16_t second_order = 0;
    std::uint64_t generation = 0;
};
struct SetFamilyPage {
    std::size_t child_page = 0;
    std::size_t spouse_page = 0;
    std::uint16_t child_spouse = 0;
};
struct SetScenarioDate {
    std::uint16_t year = 200;
    std::uint8_t month = 1;
    bool write_year = true;
    bool write_month = true;
};
struct StopDemoPlay {
    // Must match the previously observed live protagonist; never substitutes
    // the roster master object for that live game object.
    std::uint16_t officer_id = 0;
};

using OfficerToolAction = std::variant<BatchTrainingRequest, RandomTraitsRequest,
    CancelOfficerToolJob, RefreshFamilyTools, SetChildSchedule, SetPregnancySpouse, SwapSpouseOrder,
    SetFamilyPage, SetScenarioDate, StopDemoPlay>;

struct OfficerToolsRequest {
    GameEpoch epoch{};
    OfficerToolAction action;
};

struct OfficerToolProgress {
    OfficerToolJob job = OfficerToolJob::None;
    OfficerToolStatus status = OfficerToolStatus::Idle;
    std::uint32_t scanned = 0;
    std::uint32_t eligible = 0;
    std::uint32_t processed = 0;
    std::uint32_t changed = 0;
    std::uint32_t filled_slots = 0;
    std::uint32_t already_full = 0;
    std::uint32_t failed = 0;
    std::uint32_t trait_candidates = 0;
    std::uint32_t trait_objects = 0;
};

struct ChildToolRow {
    std::uint16_t id = 0;
    std::uint16_t other_parent = 0;
    std::uint16_t appearance = 0;
    std::uint16_t birth = 0;
    std::uint16_t death = 0;
    std::uint16_t target_year = 0;
    std::uint8_t years_later = 3;
    bool selected = false;
};

struct SpouseToolRow {
    std::uint16_t id = 0;
    std::uint16_t birth = 0;
    std::uint16_t child_count = 0;
    // One-based position in the observed relationship table/member order.
    std::uint16_t order = 0;
    bool order_swappable = false;
};

struct PregnancyToolSlot {
    std::uint16_t spouse = 0;
    std::uint16_t child = 0;
    std::uint8_t cooldown = 0;
    std::uint8_t pregnant = 0;
    std::uint8_t remaining_months = 0;
    std::array<std::uint8_t, 5> child_caps{};
    bool safe_to_swap = false;
};

struct FamilyToolsView {
    bool loading = false;
    bool relationships_ready = false;
    bool pregnancy_ready = false;
    bool spouse_order_recovery_pending = false;
    std::uint64_t relationship_generation = 0;
    std::uint16_t hero = 0;
    std::size_t child_page = 0;
    std::size_t spouse_page = 0;
    std::uint16_t child_spouse = 0;
    std::size_t child_total = 0;
    std::size_t spouse_total = 0;
    std::size_t child_count = 0;
    std::size_t spouse_count = 0;
    std::array<ChildToolRow, FAMILY_CHILD_PAGE> children{};
    std::array<SpouseToolRow, FAMILY_SPOUSE_PAGE> spouses{};
    std::array<PregnancyToolSlot, 3> pregnancy{};
};

struct OfficerToolsView {
    GameEpoch epoch{};
    std::uint64_t revision = 0;
    OfficerToolStatus request_status = OfficerToolStatus::Idle;
    OfficerToolProgress progress;
    bool date_ready = false;
    std::uint16_t year = 0;
    std::uint8_t month = 0;
    std::uint16_t hero = 0;
    bool demo_ready = false;
    std::uint16_t demo_officer = 0;
    FamilyToolsView family;
};

} // namespace s8::domain
