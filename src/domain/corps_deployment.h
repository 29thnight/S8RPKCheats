#pragma once

#include "city_management.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace s8::domain {

inline constexpr std::size_t CORPS_OFFICER_CAPACITY = 5102;
inline constexpr std::size_t CORPS_PAGE_SIZE = 64;

struct CorpsOptions {
    bool each_council = false;
    bool group_sworn = true;
    bool group_spouses = true;
    bool group_synergetic = true;
    bool operator==(const CorpsOptions&) const = default;
};

enum class CorpsAction : std::uint8_t {
    Refresh, SelectCity, SetPage, BuildPlan, ResetTargets, ApplyMoves, ApplyGovernors, ApplyAll,
    MoveOfficer, SwapGovernor, SwapGeneral, SetOptions, ToggleAutoTarget, Cancel
};

struct CorpsRequest {
    CorpsAction action = CorpsAction::Refresh;
    std::uint8_t city = NO_CITY;
    std::uint8_t target_city = NO_CITY;
    std::uint16_t officer = NO_CITY_OFFICER;
    std::uint16_t other_officer = NO_CITY_OFFICER;
    std::uint16_t page = 0;
    CorpsOptions options;
};

enum class CorpsStatus : std::uint8_t {
    Ready, Reading, Planning, ApplyingMoves, ApplyingGovernors, Restoring, Complete, Cancelled,
    SessionChanged, Busy, InvalidRequest, ReadFailed, OwnershipChanged, WriteFailed,
    RollbackFailed, MissingGovernor, MissingCycleBuffer, RelationshipUnavailable, CapacityExceeded
};

enum class CorpsReason : std::uint8_t {
    Fixed, Relationship, RelationshipRear, RelationshipAnchor, EmptyGovernor,
    LowLoyaltyRear, AdviserFront, FrontScore, RearScore, GovernorExchange, Retained
};

inline constexpr std::array<std::string_view, 11> CORPS_REASON_NAMES{
    "고정 신분", "관계 묶음", "관계 묶음·낮은 충성", "관계 묶음·고정 도시", "빈 도시 태수",
    "낮은 충성·후방", "군사·전방", "전방 능력", "후방 능력", "태수 후보 교환", "현재 도시 유지"};

struct CorpsCity {
    std::uint64_t number = 0;
    std::uint16_t general = NO_CITY_OFFICER;
    std::uint16_t current_governor = NO_CITY_OFFICER;
    std::uint16_t recommended_governor = NO_CITY_OFFICER;
    std::uint16_t current_count = 0;
    std::uint16_t target_count = 0;
    std::uint16_t assigned_count = 0;
    bool player_owned = false;
    bool selected = false;
    bool frontline = false;
    bool auto_target = false;
    bool deferred = false;
};

struct CorpsOfficer {
    std::uint16_t id = NO_CITY_OFFICER;
    std::uint8_t status = 0;
    std::uint8_t loyalty = 0;
    std::array<std::uint8_t, 5> attributes{};
    std::uint8_t current_city = NO_CITY;
    std::uint8_t recommended_city = NO_CITY;
    CorpsReason reason = CorpsReason::Retained;
    bool relationship_grouped = false;
    bool recommended_governor = false;
};

struct CorpsSnapshot {
    GameEpoch epoch{};
    std::uint64_t revision = 0;
    CorpsOptions options;
    std::array<CorpsCity, CITY_COUNT> cities{};
    std::array<CorpsOfficer, CORPS_PAGE_SIZE> officers{};
    CorpsStatus status = CorpsStatus::Ready;
    CorpsStatus last_request = CorpsStatus::Ready;
    std::uint16_t page = 0;
    std::uint16_t page_count = 0;
    std::uint16_t officer_count = 0;
    std::uint16_t visible_count = 0;
    std::uint8_t selected_city = NO_CITY;
    std::size_t completed = 0;
    std::size_t total = 0;
    std::size_t moved = 0;
    std::size_t swapped = 0;
    std::size_t appointed = 0;
    std::size_t deferred = 0;
    bool available = false;
    bool busy = false;
    bool plan_valid = false;
    bool relationships_complete = false;
    std::string_view work_stage;
};

} // namespace s8::domain
