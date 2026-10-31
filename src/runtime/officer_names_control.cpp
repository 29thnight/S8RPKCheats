#include "officer_names_control.h"

#include "runtime.h"
#include "../game/game_base.h"
#include "../game/stratagem5_native_code.h"
#include "../services/ui_bridge.h"

namespace s8::runtime {

void update_officer_names(game::NativeOfficerNames& state, Runtime& runtime,
    services::UiBridge& bridge, std::uintptr_t module, std::uint64_t now_ms,
    const domain::WorkBudget& work_budget) {
    if (runtime.stopping || work_budget.exhausted()) {
        return;
    }
    if (state.layout_module != module) {
        state.layout_module = module;
        state.layout_supported = game::stratagem5_native_detail::supported_image(runtime.module_image);
        if (!state.layout_supported) {
            bridge.publish_notice("이 게임 빌드의 실제 이름 조회 함수를 확인하지 못해 파일의 무장 이름을 사용합니다.");
        }
    }
    const game::NativeOfficerNameContext context{
        module, game::read_game_base(runtime.game_base_pointer), runtime.game_base_pointer, runtime.epoch, now_ms};

    game::update_native_officer_names(state, context, work_budget);
    if (state.pending && bridge.native_officer_names.push(state.output)) {
        state.pending = false;
    }
}

} // namespace s8::runtime
