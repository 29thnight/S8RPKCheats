#include "corps_control.h"

#include "runtime.h"
#include "../game/game_base.h"
#include "../services/ui_bridge.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

namespace s8::runtime {

domain::Result<void> submit_corps_request(Runtime& runtime, std::uintptr_t module, const domain::CorpsRequest& request) {
    const std::uintptr_t root = game::read_game_base(runtime.game_base_pointer);
    const domain::CorpsStatus status = game::request_corps_deployment(*runtime.corps_deployment, module,
        root, game::read_player(root), runtime.epoch, request);

    runtime.corps_deployment->snapshot.last_request = status;
    ++runtime.corps_deployment->snapshot.revision;
    if (status == domain::CorpsStatus::Complete || status == domain::CorpsStatus::Cancelled ||
        status == domain::CorpsStatus::Reading || status == domain::CorpsStatus::Planning ||
        status == domain::CorpsStatus::ApplyingMoves || status == domain::CorpsStatus::ApplyingGovernors ||
        status == domain::CorpsStatus::Restoring) {
        return {};
    }
    const domain::ErrorCode code = status == domain::CorpsStatus::InvalidRequest ? domain::ErrorCode::InvalidData :
        status == domain::CorpsStatus::SessionChanged ? domain::ErrorCode::StaleEpoch :
        status == domain::CorpsStatus::Busy ? domain::ErrorCode::QueueFull : domain::ErrorCode::PlatformFailure;

    return std::unexpected(domain::Error{code, static_cast<std::uint32_t>(status)});
}

void service_corps_deployment(Runtime& runtime, services::UiBridge& ui, std::uintptr_t module) {
    if (runtime.stopping) { return; }
    const std::uintptr_t root = game::read_game_base(runtime.game_base_pointer);

    game::tick_corps_deployment(*runtime.corps_deployment, module, root, game::read_player(root), runtime.epoch, GetTickCount64(), 64);
    if (runtime.corps_published_revision != runtime.corps_deployment->snapshot.revision) {
        ui.corps_deployment.publish(runtime.corps_deployment->snapshot);
        runtime.corps_published_revision = runtime.corps_deployment->snapshot.revision;
    }
}

} // namespace s8::runtime
