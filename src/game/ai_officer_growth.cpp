#include "ai_officer_growth.h"

#include "officer_roster.h"
#include "../platform/windows/process_memory.h"

#include <algorithm>
#include <cstring>
#include <span>

namespace s8::game {
namespace {

constexpr std::uintptr_t GROWTH_SETTING_RVA = 0x3ba9888;
constexpr std::size_t OFFICERS_PER_TICK = 8;
constexpr std::size_t OFFICER_STRIDE = 0x3d0;

template <typename T>
bool read_value(std::uintptr_t address, T& value) {

    return platform::windows::read_memory(address, std::as_writable_bytes(std::span(&value, 1)));
}

template <typename T>
T record_value(const std::array<std::byte, OFFICER_STRIDE>& record, std::size_t offset) {

    T value{};

    std::memcpy(&value, record.data() + offset, sizeof(T));
    return value;
}

bool write_game_growth(std::uintptr_t module_base, std::uint8_t player, std::uint8_t others) {

    std::uint8_t packed = 0;

    if (!module_base || !read_value(module_base + GROWTH_SETTING_RVA, packed)) {
        return false;
    }

    const std::uint8_t desired = static_cast<std::uint8_t>((packed & 0xf0u) | player | (others << 2));

    if (packed == desired) {
        return true;
    }

    std::uint8_t actual = 0;

    return platform::windows::write_data_memory(
               module_base + GROWTH_SETTING_RVA, std::as_bytes(std::span(&desired, 1))) &&
        read_value(module_base + GROWTH_SETTING_RVA, actual) && actual == desired;
}

domain::AiGrowthProfile decode_profile(const std::array<std::byte, OFFICER_STRIDE>& record,
                                       std::uintptr_t officer, std::uintptr_t player,
                                       std::uint16_t player_id) {

    domain::AiGrowthProfile profile{};

    profile.officer_id = record_value<std::uint16_t>(record, 8);
    profile.eligible = profile.officer_id > 0 && profile.officer_id <= domain::AI_GROWTH_OFFICER_LIMIT &&
        officer != player && profile.officer_id != player_id &&
        record_value<std::uintptr_t>(record, 0x18) > 0x10000;

    if (!profile.eligible) {
        return profile;
    }

    std::memcpy(profile.stats.data(), record.data() + 0xaa, profile.stats.size());
    std::memcpy(profile.skills.data(), record.data() + 0x1d0, profile.skills.size());
    std::memcpy(profile.current_exp.data(), record.data() + 0xb0, sizeof(profile.current_exp));
    std::memcpy(profile.cumulative_exp.data(), record.data() + 0xba, sizeof(profile.cumulative_exp));
    std::memcpy(profile.aptitudes.data(), record.data() + 0xc4, profile.aptitudes.size());

    for (std::size_t category = 0; category < domain::AI_GROWTH_CATEGORY_COUNT; ++category) {
        std::memcpy(profile.tactics[category].data(), record.data() + 0x139 + category * 5, 5);
    }

    return profile;
}

struct GrowthWrite {
    std::uintptr_t address;
    std::span<const std::byte> before;
    std::span<const std::byte> after;
};

bool same_bytes(std::uintptr_t address, std::span<const std::byte> expected) {

    std::array<std::byte, 35> current{};

    return platform::windows::read_memory(address, std::span(current).first(expected.size())) &&
        std::memcmp(current.data(), expected.data(), expected.size()) == 0;
}

bool apply_growth(std::uintptr_t officer, const std::array<std::byte, OFFICER_STRIDE>& record,
                   const domain::AiGrowthPreview& preview) {

    std::array<std::uint16_t, 10> experience{};
    std::array<std::uint8_t, 35> tactics{};

    for (std::size_t stat = 0; stat < 5; ++stat) {
        experience[stat] = preview.current_exp_after[stat];
    }

    for (std::size_t category = 0; category < domain::AI_GROWTH_CATEGORY_COUNT; ++category) {
        std::copy(preview.tactics_after[category].begin(), preview.tactics_after[category].end(),
                  tactics.begin() + static_cast<std::ptrdiff_t>(category * 5));
    }

    const std::span<const std::byte> before(record);
    const std::array<GrowthWrite, 3> writes{{
        {officer + 0xb0, before.subspan(0xb0, 20), std::as_bytes(std::span(experience))},
        {officer + 0xc4, before.subspan(0xc4, 7), std::as_bytes(std::span(preview.aptitude_after))},
        {officer + 0x139, before.subspan(0x139, 35), std::as_bytes(std::span(tactics))}}};
    std::uint16_t current_id = 0;

    if (!read_value(officer + 8, current_id) || current_id != preview.officer_id) {
        return false;
    }

    for (const GrowthWrite& write : writes) {

        if (!same_bytes(write.address, write.before)) {
            return false;
        }
    }

    for (std::size_t index = 0; index < writes.size(); ++index) {

        const GrowthWrite& write = writes[index];

        if (platform::windows::write_data_memory(write.address, write.after) &&
            same_bytes(write.address, write.after)) {
            continue;
        }

        for (std::size_t rollback = index + 1; rollback > 0; --rollback) {

            const GrowthWrite& saved = writes[rollback - 1];

            if (same_bytes(saved.address, saved.after)) {
                platform::windows::write_data_memory(saved.address, saved.before);
            }
        }

        return false;
    }

    return true;
}

bool read_calendar(std::uintptr_t module_base, std::uintptr_t& manager,
                    std::uint16_t& year, std::uint8_t& month) {

    std::array<std::uint8_t, 3> date{};

    if (!module_base || !read_value(module_base + 0x2e98bc8, manager) || manager <= 0x10000 ||
        !platform::windows::read_memory(manager + 0x72d0, std::as_writable_bytes(std::span(date)))) {
        return false;
    }

    year = static_cast<std::uint16_t>(date[0] | (static_cast<std::uint16_t>(date[1]) << 8));
    month = date[2];
    return year != 0 && month >= 1 && month <= 12;
}

void finish_job(AiGrowthState& state, AiGrowthOutcome outcome) {

    state.running = false;
    state.totals.outcome = outcome;
    state.report = state.totals;

    if (outcome == AiGrowthOutcome::Completed) {
        state.completed_year = state.active_year;
    }
}

void clear_session(AiGrowthState& state) {

    if (state.running) {
        finish_job(state, AiGrowthOutcome::SessionChanged);
    }

    state.game_base = 0;
    state.player = 0;
    state.manager = 0;
    state.roster = 0;
    state.pending_year = 0;
    state.completed_year = 0;
    state.last_poll_ms = 0;
    state.date_initialized = false;
}

void apply_next_officers(AiGrowthState& state) {

    const std::size_t end = std::min<std::size_t>(domain::AI_GROWTH_OFFICER_LIMIT,
                                                  state.cursor + OFFICERS_PER_TICK);

    while (state.cursor < end) {

        const std::uintptr_t officer = state.roster + state.cursor * OFFICER_STRIDE;

        ++state.cursor;

        if (!platform::windows::read_memory(officer, state.record)) {
            continue;
        }

        const domain::AiGrowthProfile profile = decode_profile(state.record, officer, state.player, state.player_id);
        const std::uint16_t id = profile.officer_id;

        if (id == 0 || id > domain::AI_GROWTH_OFFICER_LIMIT || state.seen[id]) {
            continue;
        }

        state.seen[id] = true;

        if (!profile.eligible) {
            continue;
        }

        ++state.totals.eligible;

        bool cumulative = false;

        for (const std::uint16_t value : profile.cumulative_exp) {
            cumulative = cumulative || value != 0;
        }

        if (!cumulative) {
            continue;
        }

        const domain::AiGrowthPreview preview = domain::calculate_ai_growth(
            profile, state.active_speed, domain::make_ai_growth_annual_seed(state.active_year, id));

        if (!apply_growth(officer, state.record, preview)) {
            ++state.totals.failed;
            continue;
        }

        ++state.totals.consumed;

        if (preview.current_exp_after != preview.current_exp_before ||
            preview.aptitude_after != preview.aptitude_before || preview.tactics_after != preview.tactics_before) {
            ++state.totals.changed;
        }

        for (std::size_t category = 0; category < domain::AI_GROWTH_CATEGORY_COUNT; ++category) {
            state.totals.aptitude_gain += static_cast<std::uint32_t>(preview.aptitude_gain[category]);
            state.totals.tactic_level_ups += static_cast<std::uint32_t>(preview.tactic_level_ups[category]);
        }
    }

    if (state.cursor == domain::AI_GROWTH_OFFICER_LIMIT) {
        finish_job(state, AiGrowthOutcome::Completed);
    }
}

} // namespace

bool set_ai_growth_enabled(std::uintptr_t module_base, domain::AiGrowthSettings& settings, bool enabled) {

    if (enabled == settings.enabled) {
        return true;
    }

    domain::AiGrowthSettings requested = settings;

    if (enabled) {

        std::uint8_t packed = 0;

        if (!module_base || !read_value(module_base + GROWTH_SETTING_RVA, packed)) {
            return false;
        }

        const int player = packed & 3u;
        const int others = (packed >> 2) & 3u;

        requested.restore_none = player == 0 && others == 0;
        requested.speed = others != 0 ? others : player != 0 ? player : 2;

        if (!set_ai_growth_speed(module_base, requested.speed)) {
            return false;
        }
    } else {

        if (requested.restore_none && !write_game_growth(module_base, 0, 0)) {
            return false;
        }

        requested.restore_none = false;
        requested.annual_special_abilities = false;
    }

    requested.enabled = enabled;
    settings = requested;
    return true;
}

bool set_ai_growth_speed(std::uintptr_t module_base, int speed) {

    const std::uint8_t normalized = static_cast<std::uint8_t>(domain::normalize_ai_growth_speed(speed));

    return write_game_growth(module_base, normalized, normalized);
}

bool read_ai_growth_preview(std::uintptr_t officer, std::uintptr_t player, int speed,
                            std::uint32_t seed, domain::AiGrowthPreview& preview) {

    std::array<std::byte, OFFICER_STRIDE> record{};
    std::uint16_t player_id = 0;

    preview = {};

    if (officer <= 0x10000 || !platform::windows::read_memory(officer, record)) {
        return false;
    }

    if (player > 0x10000) {
        read_value(player + 8, player_id);
    }

    preview = domain::calculate_ai_growth(decode_profile(record, officer, player, player_id), speed, seed);
    return true;
}

bool read_current_ai_growth_preview(std::uintptr_t module_base, std::uintptr_t officer,
                                    std::uintptr_t player, int speed, domain::AiGrowthPreview& preview) {

    std::uintptr_t manager = 0;
    std::uint16_t year = 0;
    std::uint8_t month = 0;
    std::uint16_t id = 0;
    std::uint32_t seed = 0;

    if (read_calendar(module_base, manager, year, month) && read_value(officer + 8, id)) {

        const std::uint16_t apply_year = static_cast<std::uint16_t>(year + (month == 12 ? 1 : 0));

        seed = domain::make_ai_growth_annual_seed(apply_year, id);
    }

    return read_ai_growth_preview(officer, player, speed, seed, preview);
}

void update_ai_officer_growth(AiGrowthState& state, std::uintptr_t module_base,
                              std::uintptr_t game_base, std::uintptr_t player,
                              domain::GameEpoch epoch, std::uint64_t now_ms,
                              const domain::AiGrowthSettings& settings) {

    if (state.module_base != module_base || state.epoch != epoch ||
        state.game_base != game_base || state.player != player) {
        clear_session(state);
        state.module_base = module_base;
        state.epoch = epoch;
        state.game_base = game_base;
        state.player = player;
        state.applied_game_speed = 0;
    }

    if (!module_base || game_base <= 0x10000 || player <= 0x10000) {
        return;
    }

    const int speed = domain::normalize_ai_growth_speed(settings.speed);

    if (settings.enabled && state.applied_game_speed != speed &&
        (state.last_setting_attempt_ms == 0 || now_ms - state.last_setting_attempt_ms >= 500)) {
        state.last_setting_attempt_ms = now_ms;
        state.game_setting_failed = !set_ai_growth_speed(module_base, speed);

        if (!state.game_setting_failed) {
            state.applied_game_speed = speed;
        }
    }

    if (!state.running && state.last_poll_ms != 0 && now_ms - state.last_poll_ms < 200) {
        return;
    }

    state.last_poll_ms = now_ms;

    std::uintptr_t manager = 0;
    std::uint16_t year = 0;
    std::uint8_t month = 0;

    if (!read_calendar(module_base, manager, year, month)) {
        clear_session(state);
        return;
    }

    if (state.manager != 0 && state.manager != manager) {
        clear_session(state);
        state.game_base = game_base;
        state.player = player;
    }

    state.manager = manager;

    if (!state.date_initialized) {
        state.date_initialized = true;
        state.last_year = year;
        state.last_month = month;
        return;
    }

    if (!settings.enabled) {

        if (state.running) {
            finish_job(state, AiGrowthOutcome::Cancelled);
        }

        state.pending_year = 0;
        state.last_year = year;
        state.last_month = month;
        state.applied_game_speed = 0;
        return;
    }

    if (!state.running && state.last_month == 12 && month == 1 &&
        year >= state.last_year && year != state.completed_year) {
        state.pending_year = year;
    }

    state.last_year = year;
    state.last_month = month;

    std::uint8_t phase = 0;

    if (!read_value(game_base + 0xd0, phase)) {
        return;
    }

    if (state.running) {

        if (year != state.active_year || month != 1 ||
            resolve_officer_roster_base(module_base) != state.roster) {
            finish_job(state, AiGrowthOutcome::SessionChanged);
            return;
        }

        if (phase == 5 || phase == 7) {
            apply_next_officers(state);
        }

        return;
    }

    if (!state.pending_year || phase != 5 || year != state.pending_year || month != 1) {
        return;
    }

    const std::uintptr_t roster = resolve_officer_roster_base(module_base);
    std::uint16_t player_id = 0;

    if (!roster || !read_value(player + 8, player_id) || player_id == 0) {
        return;
    }

    state.roster = roster;
    state.player_id = player_id;
    state.active_year = state.pending_year;
    state.pending_year = 0;
    state.active_speed = speed;
    state.cursor = 0;
    state.seen.fill(false);
    state.totals = {};
    state.totals.year = state.active_year;
    state.report.reset();
    state.running = true;
    apply_next_officers(state);
}

} // namespace s8::game
