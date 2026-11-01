#include "battle_dispatch.h"
#include "runtime.h"
#include "../game/stratagem5_metadata.h"
#include "../game/stratagem5_owner.h"
#include "../platform/windows/process_memory.h"

#include <span>

namespace s8::runtime {
namespace {

bool valid_context(void* context) {
    const BattleDispatch& state = *static_cast<const BattleDispatch*>(context);
    const Runtime* const runtime = state.runtime;
    std::uintptr_t current_root = 0;

    return runtime && !state.stopping && !runtime->stopping && runtime->loop_context_observed &&
        runtime->loop_game_base_ready &&
        state.epoch == runtime->epoch && state.game_root && state.game_root == runtime->loop_game_base &&
        state.root_pointer && platform::windows::read_memory(state.root_pointer,
            std::as_writable_bytes(std::span(&current_root, 1))) && current_root == state.game_root;
}

bool prepare_model(void* context, const game::Stratagem5NativeContext& native,
    std::uint32_t& original_count) {
    BattleDispatch& state = *static_cast<BattleDispatch*>(context);

    if (!valid_context(context)) {
        return false;
    }
    Runtime& runtime = *state.runtime;
    std::uintptr_t holder = 0, owner = 0;

    if (should_skip_stratagem5(runtime.stratagem5_lifecycle) || !native.dialog ||
        !platform::windows::read_memory(native.dialog + 0x20, std::as_writable_bytes(std::span(&holder, 1))) ||
        !holder || !platform::windows::read_memory(holder, std::as_writable_bytes(std::span(&owner, 1))) || !owner) {
        return false;
    }
    // Native Dialog::Initialize may precede the side/gauge fields. The dialog
    // owner's checked five-row model is the primary identity, as in original.
    std::optional<std::uintptr_t> table = game::resolve_stratagem5_metadata_from_owner(owner);

    if (!table && state.metadata_offset) {
        table = game::resolve_stratagem5_metadata(*state.metadata_offset, state.game_root);
    }
    if (!table) {
        return false;
    }
    const std::optional<game::Stratagem5CountCandidate> candidate = game::read_stratagem5_candidate(owner, *table);

    if (!candidate || !valid_context(context)) {
        return false;
    }
    original_count = candidate->plan.original_count;

    // One synchronous, bounded event advances DATA -> COUNT -> CAMP -> MODEL
    // before the game binds native button callbacks. No render-thread write or
    // delayed worker can satisfy this one-shot initialization boundary.
    if (!on_stratagem_metadata(runtime, *table) || !valid_context(context) ||
        !on_stratagem5_context(runtime, *table, native.dialog, native.attack_owner, native.defense_owner)) {
        return false;
    }
    const std::optional<game::Stratagem5CountCandidate> verified = game::read_stratagem5_candidate(owner, *table);

    return verified && verified->plan.already_present && verified->plan.original_count == original_count;
}

bool prepare_bridge(void* context) {
    BattleDispatch& state = *static_cast<BattleDispatch*>(context);

    if (!state.runtime || state.prepare_blocked || state.now_ms < state.next_prepare_ms || !valid_context(context)) {
        return false;
    }
    if (!state.preparation_attempted || !state.native.view.prepared) {
        return false;
    }
    const game::Stratagem5NativeCallbacks callbacks{context, valid_context, prepare_model};
    const bool prepared = game::prepare_stratagem5_native(state.native, state.module,
        state.runtime->module_image, callbacks);

    if (!prepared) {
        state.next_prepare_ms = state.now_ms + 100;
        state.prepare_blocked = state.native.view.patch_result != platform::windows::CodePatchResult::Busy;
    }
    return prepared;
}

bool set_metadata(void* context, bool enabled) {
    BattleDispatch& state = *static_cast<BattleDispatch*>(context);

    return game::set_stratagem5_native_metadata(state.native, enabled);
}

bool ensure_capture(void* context) {
    BattleDispatch& state = *static_cast<BattleDispatch*>(context);

    return game::ensure_stratagem5_native_count_capture(state.native);
}

domain::Error native_error(const BattleDispatch& state) {
    if (state.native.view.patch_result == platform::windows::CodePatchResult::Busy) {
        return {domain::ErrorCode::QueueFull, 0x5100};
    }
    if (state.native.view.issue == game::Stratagem5NativeIssue::UnsupportedBuild) {
        return {domain::ErrorCode::UnsupportedVersion, 0x5101};
    }
    return {domain::ErrorCode::PlatformFailure,
        0x5110 + static_cast<std::uint32_t>(state.native.view.patch_result)};
}

} // namespace

bool prepare_battle_dispatch(BattleDispatch& state, Runtime& runtime, std::uintptr_t module_base) {
    if (state.preparation_attempted || !module_base || runtime.module_image.empty()) {
        return state.native.view.prepared;
    }
    refresh_battle_preparation(state, runtime, module_base,
        game::find_stratagem5_metadata_offset(runtime.module_image));
    return state.native.view.prepared;
}

void refresh_battle_preparation(BattleDispatch& state, Runtime& runtime, std::uintptr_t module_base,
    std::optional<std::uint32_t> metadata_offset) {
    if (state.stopping || state.stopped || runtime.stopping || !module_base ||
        runtime.image_module != module_base || runtime.module_image.empty() ||
        (state.module && state.module != module_base)) {
        return;
    }
    if (!state.metadata_offset) {
        state.metadata_offset = metadata_offset;
    }
    state.metadata_searched = true;
    if (state.native.view.prepared || state.native.view.installed ||
        state.native.view.armed || state.native.view.registered) {
        return;
    }
    state.module = module_base;
    state.preparation_attempted = true;
    // The adapter also rejects a previously published or thread-bound owner.
    // Do not reset its view or storage to force another preparation attempt.
    state.prepare_blocked = !game::prepare_stratagem5_native_code(state.native, module_base, runtime.module_image);
    if (!state.prepare_blocked) {
        state.next_prepare_ms = 0;
    }
}

void update_battle_dispatch(BattleDispatch& state, Runtime& runtime,
    std::uintptr_t module_base, std::uint64_t now_ms, const domain::WorkBudget& work_budget) {
    if (state.stopping || runtime.stopping || !module_base || work_budget.exhausted()) {
        return;
    }
    const bool changed = state.epoch != runtime.epoch || state.game_root != runtime.loop_game_base;

    state.runtime = &runtime;
    state.module = module_base;
    state.game_root = runtime.loop_game_base;
    state.root_pointer = runtime.game_base_pointer;
    state.epoch = runtime.epoch;
    state.now_ms = now_ms;
    if (changed) {
        state.battle_seen = false;
        game::update_stratagem5_native(state.native, false, state.epoch.value, true);
    }
    if (!runtime.stratagem5_lifecycle.desired.load(std::memory_order_acquire) && state.native.view.prepared) {
        state.prepare_blocked = false;
        state.next_prepare_ms = 0;
    }
    const Stratagem5UiActions actions{&state, prepare_bridge, set_metadata, ensure_capture};
    const bool previously_seen = state.battle_seen;

    monitor_battle(runtime, module_base, runtime.loop_game_base, now_ms, actions, work_budget);
    state.battle_seen = runtime.battle_seen;
    game::update_stratagem5_native(state.native,
        !should_skip_stratagem5(runtime.stratagem5_lifecycle), state.epoch.value,
        previously_seen && !state.battle_seen);
}

std::string_view take_battle_dispatch_notice(BattleDispatch& state, std::uint64_t now_ms) {
    const game::Stratagem5NativeIssue issue = state.native.view.issue;

    if (issue != state.observed_issue) {
        state.observed_issue = issue;
        state.issue_since_ms = now_ms;
    }
    if (!state.runtime || !state.runtime->stratagem5_lifecycle.desired.load(std::memory_order_acquire) ||
        state.stopping.load(std::memory_order_acquire)) {
        return {};
    }
    if (issue == game::Stratagem5NativeIssue::None) {
        state.reported_issue = issue;
        return {};
    }
    if (issue == state.reported_issue || state.native.view.patch_result == platform::windows::CodePatchResult::Busy) {
        return {};
    }
    const bool temporary = issue == game::Stratagem5NativeIssue::WaitingModel ||
        issue == game::Stratagem5NativeIssue::InvalidLayout || issue == game::Stratagem5NativeIssue::InvalidSession;

    if (temporary && now_ms - state.issue_since_ms < 2000) {
        return {};
    }
    state.reported_issue = issue;
    switch (issue) {
    case game::Stratagem5NativeIssue::UnsupportedBuild:
        return "책략 5칸: 게임 실행 파일의 빌드 식별자가 원본 지원 버전과 달라 네이티브 UI 확장을 적용하지 않았습니다.";
    case game::Stratagem5NativeIssue::HookFailure:
        return "책략 5칸: 네이티브 UI 명령어 검증 또는 패치 적용에 실패했습니다. 적용 상태와 복구 정보를 보존합니다.";
    case game::Stratagem5NativeIssue::ForeignThread:
        return "책략 5칸: 게임 UI 콜백이 지정된 창 스레드 밖에서 호출되어 해당 콜백은 원본 경로로 통과시켰습니다.";
    case game::Stratagem5NativeIssue::InvalidSession:
        return "책략 5칸: 현재 게임 루트와 세션을 확인할 수 없어 네이티브 UI 변경을 보류했습니다.";
    case game::Stratagem5NativeIssue::WaitingModel:
        return "책략 5칸: 현재 전장의 책략 데이터 또는 진영 모델 준비를 기다리고 있습니다.";
    case game::Stratagem5NativeIssue::InvalidLayout:
        return "책략 5칸: 현재 네이티브 UI 레이아웃/세대가 일치하지 않아 추가 버튼 연결을 보류했습니다.";
    case game::Stratagem5NativeIssue::AllocationFailed:
        return "책략 5칸: 게임 소유 버튼 메모리를 준비하지 못했습니다. 같은 UI 세대에서는 중복 할당하지 않습니다.";
    case game::Stratagem5NativeIssue::AllocationBudgetExhausted:
        return "책략 5칸: 복구 상태를 확인하지 못한 버튼 보관 한도에 도달했습니다. 새 버튼 할당을 보류하고 기존 UI를 유지합니다.";
    case game::Stratagem5NativeIssue::RegistryFailed:
        return "책략 5칸: 추가 버튼의 ID 7 등록 결과를 검증하지 못했습니다.";
    case game::Stratagem5NativeIssue::MemoryFailed:
        return "책략 5칸: 게임 UI 또는 모델 접근에 실패했습니다. 적용 백업을 보존하고 다음 안전한 시점에 복구합니다.";
    case game::Stratagem5NativeIssue::None:
        return {};
    }
    return {};
}

domain::Result<void> quiesce_battle_dispatch(BattleDispatch& state) {
    state.stopping = true;
    if (!game::quiesce_stratagem5_native(state.native)) {
        return std::unexpected(native_error(state));
    }
    return {};
}

void abandon_battle_dispatch(BattleDispatch& state) noexcept {
    state.stopping.store(true, std::memory_order_release);
    game::close_stratagem5_native_admission();
}

domain::Result<void> stop_battle_dispatch(BattleDispatch& state, Runtime& runtime) {
    if (state.stopped) {
        return {};
    }
    if (!runtime.stopped) {
        return std::unexpected(domain::Error{domain::ErrorCode::QueueFull, 0x5102});
    }
    if (!game::stop_stratagem5_native(state.native)) {
        return std::unexpected(native_error(state));
    }
    state.runtime = nullptr;
    state.stopped = true;
    return {};
}

} // namespace s8::runtime
