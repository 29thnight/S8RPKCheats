#include "specialty_tools.h"

#include "runtime.h"
#include "../services/ui_bridge.h"

namespace s8::runtime {

void update_specialty_tools(game::SpecialtyTools& tools, Runtime& runtime,
    services::UiBridge& bridge, std::uintptr_t module, const domain::WorkBudget& work_budget) {
    if (runtime.stopping || work_budget.exhausted()) {
        return;
    }
    constexpr domain::NamedSpecialties EMPTY{};
    const game::SpecialtyContext context{
        module, runtime.loop_game_base, runtime.loop_player, runtime.game_base_pointer, runtime.epoch};
    const domain::NamedSpecialties& named = runtime.named_specialties ? *runtime.named_specialties : EMPTY;
    const std::uint64_t revision = tools.snapshot.revision;

    if (tools.snapshot.epoch != runtime.epoch) {
        game::update_specialty_tools(tools, context, runtime.specialty_random, work_budget);
    }
    const std::optional<domain::SpecialtyRequest> request = bridge.specialties.pop();

    if (request && !game::request_specialty_tools(tools, context, *request, named)) {
        bridge.publish_notice("명품 요청을 적용하지 못했습니다. 진행 중인 작업과 대상의 현재 상태를 확인하세요.");
    }
    if (runtime.specialty_assignment_requested && !tools.snapshot.busy &&
        runtime.loop_game_base_ready && runtime.loop_player_ready && runtime.named_specialties) {
        domain::SpecialtyRequest automatic;

        automatic.epoch = runtime.epoch;
        automatic.action = domain::SpecialtyAction::Distribute;
        if (game::request_specialty_tools(tools, context, automatic, named)) {
            runtime.specialty_assignment_requested = false;
        }
    }
    game::update_specialty_tools(tools, context, runtime.specialty_random, work_budget);
    if (tools.snapshot.revision != revision) {
        bridge.specialties.publish(tools.snapshot);
    }
}

void stop_specialty_tools(game::SpecialtyTools& tools, services::UiBridge& bridge) {
    bridge.specialties.close();
    game::stop_specialty_tools(tools);
    bridge.specialties.publish(tools.snapshot);
}

} // namespace s8::runtime
