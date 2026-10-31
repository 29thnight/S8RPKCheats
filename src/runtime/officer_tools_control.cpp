#include "officer_tools_control.h"

#include "runtime.h"
#include "../game/game_base.h"
#include "../platform/windows/process_memory.h"
#include "../services/ui_bridge.h"

#include <array>
#include <cstdio>
#include <span>
#include <utility>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

namespace s8::runtime {
namespace {

game::OfficerToolContext context_for(const Runtime& runtime, std::uintptr_t module) {
    const std::uintptr_t base = game::read_game_base(runtime.game_base_pointer);

    return {module, base, game::read_player(base), runtime.epoch, GetTickCount64(), runtime.game_base_pointer};
}

void log_notice_counts(services::UiBridge& ui, const domain::OfficerNotificationView& before,
    const domain::OfficerNotificationView& current) {
    std::array<char, 256> text{};

    if (before.ronin_sequence != current.ronin_sequence && current.total_ronins != 0) {
        const int length = std::snprintf(text.data(), text.size(), "[재야 감지] %zu명 발견", current.total_ronins);

        if (length > 0) {
            ui.file_log.push(text.data());
        }
    }
    if (before.changes_sequence != current.changes_sequence &&
        (current.total_recruits != 0 || current.total_deaths != 0)) {
        const int length = std::snprintf(text.data(), text.size(), "[장수변동] 등용 %zu명 / 사망 %zu명",
            current.total_recruits, current.total_deaths);

        if (length > 0) {
            ui.file_log.push(text.data());
        }
    }
}

} // namespace

domain::Result<void> submit_officer_tools_request(Runtime& runtime, services::UiBridge* ui,
    std::uintptr_t module, const domain::OfficerToolsRequest& request) {
    const game::OfficerToolContext context = context_for(runtime, module);
    own::shared_owner<const domain::CustomTraits> traits;

    if (ui != nullptr) {
        traits = ui->custom_traits_view();
    }
    const bool accepted = game::submit_officer_tool(*runtime.officer_tools, context, request, std::move(traits));

    runtime.officer_tools_publish_pending = true;
    if (accepted) {
        return {};
    }
    const domain::OfficerToolStatus status = runtime.officer_tools->view.request_status;
    const domain::ErrorCode error = status == domain::OfficerToolStatus::SessionChanged
        ? domain::ErrorCode::StaleEpoch : status == domain::OfficerToolStatus::InvalidRequest
        ? domain::ErrorCode::InvalidData : domain::ErrorCode::PlatformFailure;

    return std::unexpected(domain::Error{error, static_cast<std::uint32_t>(status)});
}

void service_officer_tools(Runtime& runtime, services::UiBridge& ui, std::uintptr_t module,
    const domain::WorkBudget& work_budget) {
    if (runtime.stopping || work_budget.exhausted()) {
        return;
    }
    const game::OfficerToolContext context = context_for(runtime, module);
    const bool was_running = runtime.officer_tools->running;

    if (game::tick_officer_tools(*runtime.officer_tools, context, work_budget)) {
        runtime.officer_tools_publish_pending = true;
    }
    if (was_running && !runtime.officer_tools->running && runtime.selected_officer) {
        runtime.selected_officer = game::read_roster_officer(runtime.officer_roster, module, runtime.selected_officer->id);
    }
    if (runtime.officer_tools_publish_pending && ui.officer_tools.publish(runtime.officer_tools->view)) {
        runtime.officer_tools_publish_pending = false;
    }
}

void service_officer_notices(Runtime& runtime, services::UiBridge& ui, std::uintptr_t module,
    const domain::WorkBudget& work_budget) {
    if (runtime.stopping || work_budget.exhausted()) {
        return;
    }
    const game::OfficerToolContext context = context_for(runtime, module);
    std::uint8_t phase = 0xff;
    const bool stable_player = runtime.config_loaded && runtime.loop_player_ready &&
        runtime.config_first_player_ms != 0 && context.now_ms - runtime.config_first_player_ms >= 3000;

    if (context.game_base != 0) {
        platform::windows::read_memory(context.game_base + 0xd0, std::as_writable_bytes(std::span(&phase, 1)));
    }
    game::tick_officer_notifications(*runtime.officer_notices, module, runtime.epoch, context.player, phase,
        runtime.battle_state.mode == game::BattleMode::Battle,
        stable_player && runtime.features.get(domain::FeatureId::MonitorRonin),
        stable_player && runtime.features.get(domain::FeatureId::OfficerChangeNotify), 128, work_budget);
    const domain::OfficerNotificationView& current = runtime.officer_notices->view;
    domain::OfficerNoticesSnapshot& snapshot = runtime.officer_notices_snapshot;

    if (snapshot.view.epoch != current.epoch || snapshot.view.ronin_sequence != current.ronin_sequence ||
        snapshot.view.changes_sequence != current.changes_sequence) {
        log_notice_counts(ui, snapshot.view, current);
        snapshot.view = current;
        snapshot.history_count = game::copy_officer_notice_history(*runtime.officer_notices, snapshot.history);
        snapshot.history_sequence = runtime.officer_notices->history.sequence;
        runtime.officer_notices_publish_pending = true;
    }
    if (runtime.officer_notices_publish_pending && ui.officer_notices.publish(snapshot)) {
        runtime.officer_notices_publish_pending = false;
    }
}

} // namespace s8::runtime
