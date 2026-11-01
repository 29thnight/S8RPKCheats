#include "game_loop.h"

#include "runtime.h"
#include "runtime_services.h"
#include "command_commit.h"
#include "debug_tools.h"
#include "native_preparation.h"
#include "specialty_tools.h"
#include "battle_dispatch.h"
#include "officer_names_control.h"
#include "../domain/ownership.h"
#include "../game/stratagem5_metadata.h"
#include "../overlay/window_subclass.h"
#include "../platform/windows/module_image.h"
#include "../platform/windows/speed_clock.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <winrt/base.h>

#include <algorithm>
#include <atomic>
#include <array>
#include <chrono>
#include <cwchar>
#include <expected>
#include <mutex>
#include <string>
#include <string_view>

namespace s8::runtime {
namespace {

constexpr std::size_t COMMANDS_PER_DISPATCH = 4;
constexpr std::chrono::microseconds DISPATCH_BUDGET{1500};
constexpr std::chrono::microseconds JOB_SLICE{250};
constexpr std::uint64_t STOP_RETRY_INTERVAL_MS = 100;

struct PreparedGameImage {
    std::uintptr_t module = 0;
    std::uint64_t generation = 0;
    std::vector<std::byte> image;
    NativeSignaturePreparation signatures;
    std::optional<std::uint32_t> metadata_offset;
};

struct RuntimeSession {
    Runtime runtime;
    DebugTools debug_tools;
    game::SpecialtyTools specialty_tools;
    BattleDispatch battle_dispatch;
    game::NativeOfficerNames officer_names;
    RuntimeServices services;
    winrt::handle wake;
    winrt::handle instance;
    std::uintptr_t plugin_module = 0;
    std::uintptr_t game_module = 0;
    std::atomic<HWND> window{nullptr};
    HHOOK message_hook = nullptr;
    HWINEVENTHOOK window_events = nullptr;
    UINT attach_message = 0;
    UINT update_message = 0;
    std::atomic<bool> attach_finished{false};
    std::atomic<bool> attaching{true};
    std::atomic<bool> candidate_destroyed{false};
    std::atomic<unsigned> message_callbacks{0};
    std::atomic<UINT_PTR> attach_generation{0};
    std::atomic<bool> running{false};
    std::atomic<bool> update_pending{false};
    std::atomic<bool> stop_requested{false};
    std::atomic<bool> game_stopped{false};
    std::atomic<bool> window_destroyed{false};
    std::atomic<bool> dispatch_active{false};
    UINT_PTR stop_timer = 0;
    bool dispatching = false;
    std::size_t next_job = 0;
    std::uint64_t next_stop_attempt_ms = 0;
    bool stop_failure_reported = false;
    // The coordinator owns image capture and pattern searches; only the window
    // owner adopts their value result. Neither thread mutates the other's image.
    std::atomic<std::uint64_t> image_refresh_requested{0};
    std::atomic<std::uint64_t> image_refresh_failed{0};
    std::mutex prepared_image_mutex;
    own::unique_owner<PreparedGameImage> prepared_image;
    std::uint64_t image_refresh_consumed = 0;
    std::uint64_t next_image_refresh_ms = 0;
    unsigned image_refresh_failures = 0;
    bool image_refresh_retry = false;
};

// This pointer borrows the coordinator's process-lifetime owner. Published
// callbacks cannot outlive the pinned module, event, or RuntimeSession storage.
std::atomic<RuntimeSession*> active_session{nullptr};

void wake_coordinator(RuntimeSession& session) noexcept {
    SetEvent(session.wake.get());
}

bool remove_message_hook(RuntimeSession& session) noexcept {
    if (!session.message_hook) {
        return true;
    }
    if (!UnhookWindowsHookEx(session.message_hook)) {
        return false;
    }
    session.message_hook = nullptr;
    return true;
}

void post_update(RuntimeSession& session) noexcept {
    if (!session.services.overlay.overlay.window_attached.load(std::memory_order_acquire) ||
        session.update_pending.exchange(true, std::memory_order_acq_rel)) {
        return;
    }
    if (!PostMessageW(session.window.load(std::memory_order_acquire), session.update_message, 0, 0)) {
        session.update_pending.store(false, std::memory_order_release);
        session.stop_requested.store(true, std::memory_order_release);
        wake_coordinator(session);
    }
}

void frame_ready(void* context) noexcept {
    RuntimeSession& session = *static_cast<RuntimeSession*>(context);

    if (session.services.overlay.overlay.render_stopped.load(std::memory_order_acquire)) {
        wake_coordinator(session);
    } else if (session.running.load(std::memory_order_acquire)) {
        // One pending message regardless of Present rate. No game scan, memory
        // mutation, or configuration I/O runs in this callback.
        post_update(session);
    }
}

void schedule_stop_retry(RuntimeSession& session) noexcept {
    if (!session.stop_timer && !session.window_destroyed.load(std::memory_order_acquire)) {
        session.stop_timer = SetTimer(session.window.load(std::memory_order_acquire),
            reinterpret_cast<UINT_PTR>(&session), static_cast<UINT>(STOP_RETRY_INTERVAL_MS), nullptr);
    }
}

bool stop_on_window_thread(RuntimeSession& session, std::uint64_t now_ms) {
    if (session.game_stopped.load(std::memory_order_acquire)) {
        return true;
    }
    if (now_ms < session.next_stop_attempt_ms) {
        schedule_stop_retry(session);
        return false;
    }
    session.next_stop_attempt_ms = now_ms + STOP_RETRY_INTERVAL_MS;
    stop_debug_tools(session.debug_tools, session.services.overlay.bridge.debug_tools);
    stop_specialty_tools(session.specialty_tools, session.services.overlay.bridge);

    const domain::Result<void> quiesced = quiesce_battle_dispatch(session.battle_dispatch);

    if (!quiesced) {
        schedule_stop_retry(session);
        if (quiesced.error().code != domain::ErrorCode::QueueFull && !session.stop_failure_reported) {
            session.services.overlay.bridge.publish_notice("책략 UI 콜백을 정리하지 못해 종료를 보류했습니다.");
            session.stop_failure_reported = true;
        }
        return false;
    }

    const domain::Result<void> stopped = stop_game(session.runtime, session.game_module);

    if (!stopped) {
        schedule_stop_retry(session);
        if (stopped.error().code != domain::ErrorCode::QueueFull && !session.stop_failure_reported) {
            session.services.overlay.bridge.publish_notice(
                "종료 복구가 아직 완료되지 않았습니다. 다음 안전한 갱신 시점에 재시도합니다.");
            session.stop_failure_reported = true;
        }
        return false;
    }

    const domain::Result<void> battle_stopped = stop_battle_dispatch(session.battle_dispatch, session.runtime);

    if (!battle_stopped) {
        schedule_stop_retry(session);
        if (battle_stopped.error().code != domain::ErrorCode::QueueFull && !session.stop_failure_reported) {
            session.services.overlay.bridge.publish_notice("책략 UI 연결 복구가 완료되지 않아 종료를 보류했습니다.");
            session.stop_failure_reported = true;
        }
        return false;
    }

    session.running.store(false, std::memory_order_release);
    if (session.stop_timer) {
        KillTimer(session.window.load(std::memory_order_acquire), session.stop_timer);
        session.stop_timer = 0;
    }
    publish_stopped_state(session.runtime, session.services.overlay.bridge);
    stop_runtime_entries(session.services);
    overlay::remove_window_subclass(session.window.load(std::memory_order_acquire), session.services.overlay.overlay);
    session.game_stopped.store(true, std::memory_order_release);
    wake_coordinator(session);
    return true;
}

struct DispatchScope {
    RuntimeSession& session;

    explicit DispatchScope(RuntimeSession& value) : session(value) {
        session.dispatching = true;
        session.dispatch_active.store(true, std::memory_order_release);
    }

    ~DispatchScope() {
        session.dispatching = false;
        session.dispatch_active.store(false, std::memory_order_release);
        if (session.stop_requested.load(std::memory_order_acquire) ||
            session.window_destroyed.load(std::memory_order_acquire)) {
            wake_coordinator(session);
        }
    }
};

void adopt_prepared_game_image(RuntimeSession& session, std::uint64_t now_ms) {
    const auto generation = session.image_refresh_requested.load(std::memory_order_acquire);
    if (!generation || generation == session.image_refresh_consumed || session.runtime.stopping ||
        session.stop_requested.load(std::memory_order_acquire)) {
        return;
    }
    if (session.image_refresh_failed.load(std::memory_order_acquire) == generation) {
        session.image_refresh_consumed = generation;
        if (session.image_refresh_failures == 0) {
            session.services.overlay.bridge.publish_notice("게임 정보는 연결했지만 일부 기능 준비를 다시 확인하고 있습니다.");
        }
        session.image_refresh_failures = std::min(session.image_refresh_failures + 1, 5u);
        session.next_image_refresh_ms = now_ms +
            std::min(std::uint64_t{30000}, std::uint64_t{1000} << (session.image_refresh_failures - 1));
        session.image_refresh_retry = true;
        return;
    }
    own::unique_owner<PreparedGameImage> prepared;
    {
        const std::unique_lock lock(session.prepared_image_mutex, std::try_to_lock);
        if (!lock.owns_lock() || !session.prepared_image) {
            return;
        }
        prepared = std::move(session.prepared_image);
    }
    if (prepared->module != session.game_module || prepared->generation != generation || prepared->image.empty()) {
        return;
    }
    session.runtime.module_image.swap(prepared->image);
    session.runtime.image_module = prepared->module;
    merge_native_signature_values(session.runtime, prepared->signatures);
    retry_failed_native_features(session.runtime);
    refresh_battle_preparation(session.battle_dispatch, session.runtime, prepared->module, prepared->metadata_offset);
    // Successful getters and all name batches remain intact. Only the two
    // startup failure caches need revalidation against the fresh image.
    if (!session.officer_names.layout_supported || !session.officer_names.getters_ready) {
        session.officer_names.layout_module = 0;
        session.officer_names.prepared_module = 0;
    }
    session.image_refresh_consumed = generation;
    session.image_refresh_retry = false;
    session.image_refresh_failures = 0;
}

void dispatch_game_jobs(RuntimeSession& session) {
    enum class Job : std::size_t { Discovery, Recurring, Game, Battle, Ui, Debug, Specialty, OfficerNames, Count };
    constexpr std::size_t JOB_COUNT = static_cast<std::size_t>(Job::Count);
    const std::uint64_t now_ms = GetTickCount64();

    // Deliver the initial settings even if root observation consumes the
    // dispatch budget. This only initializes owned values and publishes UI data.
    load_ui_configuration(session.runtime, session.services.overlay.bridge, session.game_module);

    // Root/epoch observation is mandatory. Starting the job deadline before
    // these reads could starve every consumer forever on a slow memory probe.
    // Recurring writes have their own rotating job below.
    observe_game_context(session.runtime, session.game_module, now_ms);
    const domain::WorkBudget dispatch_budget{std::chrono::steady_clock::now() + DISPATCH_BUDGET};

    for (std::size_t attempted = 0; attempted < JOB_COUNT && !dispatch_budget.exhausted(); ++attempted) {
        if (session.stop_requested.load(std::memory_order_acquire)) {
            break;
        }
        const Job job = static_cast<Job>(session.next_job);

        // Failed game reads must not stop configuration delivery or strand a
        // queued UI request. Request services gate their game-dependent work.
        if (!session.runtime.loop_context_observed && job != Job::Discovery && job != Job::Ui &&
            job != Job::Debug && job != Job::Specialty) {
            session.next_job = (session.next_job + 1) % JOB_COUNT;
            continue;
        }
        const auto admitted = std::chrono::steady_clock::now();
        if (dispatch_budget.deadline - admitted < JOB_SLICE) {
            // Keep this job first for the next message. Retiring an almost
            // empty slice can repeatedly skip a command or probe's first item.
            break;
        }
        const domain::WorkBudget slice{admitted + JOB_SLICE};
        // Advance only after admission, before a native call can overrun.
        session.next_job = (session.next_job + 1) % JOB_COUNT;

        switch (job) {
        case Job::Discovery: {
            const bool unresolved = session.runtime.game_base_pointer == 0;
            discover_game_base(session.runtime, session.game_module, now_ms, slice);
            const auto generation = session.image_refresh_requested.load(std::memory_order_acquire);
            if (((unresolved && session.runtime.game_base_pointer && generation == 0) ||
                (session.image_refresh_retry && now_ms >= session.next_image_refresh_ms)) &&
                generation != UINT64_MAX) {
                session.image_refresh_retry = false;
                session.image_refresh_requested.store(generation + 1, std::memory_order_release);
                wake_coordinator(session);
            }
            adopt_prepared_game_image(session, now_ms);
            break;
        }
        case Job::Recurring:
            tick_recurring_features(session.runtime, session.game_module, session.runtime.loop_game_base,
                session.runtime.loop_game_base_ready, session.runtime.loop_player, session.runtime.loop_player_ready);
            break;
        case Job::Game:
            tick_game(session.runtime, session.game_module, now_ms, slice);
            break;
        case Job::Battle: {
            update_battle_dispatch(session.battle_dispatch, session.runtime, session.game_module, now_ms, slice);

            const std::string_view notice = take_battle_dispatch_notice(session.battle_dispatch, now_ms);

            if (!notice.empty()) {
                session.services.overlay.bridge.publish_notice(std::string(notice));
            }
            break;
        }
        case Job::Ui:
            commit_ui_commands(session.runtime, session.services.overlay.bridge,
                session.game_module, COMMANDS_PER_DISPATCH, JOB_SLICE, slice);
            break;
        case Job::Debug:
            update_debug_tools(session.debug_tools, session.services.overlay.bridge.debug_tools,
                session.runtime.epoch, session.game_module, session.runtime.loop_game_base,
                session.runtime.loop_player, now_ms, session.runtime.loop_context_observed, slice);
            break;
        case Job::Specialty:
            update_specialty_tools(session.specialty_tools, session.runtime,
                session.services.overlay.bridge, session.game_module, slice);
            break;
        case Job::OfficerNames:
            update_officer_names(session.officer_names, session.runtime,
                session.services.overlay.bridge, session.game_module, now_ms, slice);
            break;
        case Job::Count:
            break;
        }
    }
}

bool dispatch_window_message(void* context, HWND, UINT message, WPARAM wparam,
                             LPARAM, LRESULT* result) noexcept {
    RuntimeSession& session = *static_cast<RuntimeSession*>(context);
    const bool update = message == session.update_message;
    const bool stop_key = message == WM_KEYUP && wparam == VK_END;
    const bool destroyed = message == WM_NCDESTROY;
    const bool retry = message == WM_TIMER && session.stop_timer && wparam == session.stop_timer;

    if (!update && !stop_key && !destroyed && !retry) {
        return false;
    }

    const platform::windows::RealClockScope real_clock;

    if (retry) {
        KillTimer(session.window.load(std::memory_order_acquire), session.stop_timer);
        session.stop_timer = 0;
    }
    if (update) {
        session.update_pending.store(false, std::memory_order_release);
    }
    if (destroyed && session.attaching.load(std::memory_order_acquire)) {
        session.candidate_destroyed.store(true, std::memory_order_release);
        wake_coordinator(session);
        return false;
    }
    if (stop_key || destroyed) {
        session.stop_requested.store(true, std::memory_order_release);
    }
    if (destroyed) {
        session.window_destroyed.store(true, std::memory_order_release);
        session.running.store(false, std::memory_order_release);
    }
    *result = 0;
    if (session.dispatching) {
        // Nested message pumps consume the message but leave work to the
        // outer dispatcher. A nested stop is observed after native calls unwind.
        return update || stop_key || retry;
    }

    const DispatchScope dispatch_scope(session);

    try {
        if (session.window_destroyed.load(std::memory_order_acquire)) {
            abandon_battle_dispatch(session.battle_dispatch);
            stop_debug_tools(session.debug_tools, session.services.overlay.bridge.debug_tools);
            stop_specialty_tools(session.specialty_tools, session.services.overlay.bridge);
            stop_runtime_entries(session.services);
            OutputDebugStringW(L"S8RPKCheats: window lost; native state retained without further game writes.\n");
        } else if (session.stop_requested.load(std::memory_order_acquire) &&
            session.running.load(std::memory_order_acquire)) {
            stop_on_window_thread(session, GetTickCount64());
        } else if (session.running.load(std::memory_order_acquire)) {
            // A window thread is an explicit dispatch context, not proof of
            // exclusive game-state ownership. Session/target guards remain
            // mandatory around every operation that reads or changes the game.
            dispatch_game_jobs(session);
        }
        if (session.stop_requested.load(std::memory_order_acquire) &&
            session.running.load(std::memory_order_acquire) &&
            !session.window_destroyed.load(std::memory_order_acquire)) {
            stop_on_window_thread(session, GetTickCount64());
        }
    } catch (...) {
        session.stop_requested.store(true, std::memory_order_release);
        schedule_stop_retry(session);
        OutputDebugStringW(L"S8RPKCheats: runtime dispatch failed; retaining state for recovery.\n");
        wake_coordinator(session);
    }
    if (destroyed) {
        wake_coordinator(session);
    }
    return update || stop_key || retry;
}

LRESULT CALLBACK dispatch_on_window_thread(int code, WPARAM removal, LPARAM parameter) {
    RuntimeSession* const session = active_session.load(std::memory_order_acquire);

    if (code < 0 || removal != PM_REMOVE || !session || !parameter) {
        return CallNextHookEx(nullptr, code, removal, parameter);
    }
    MSG& message = *reinterpret_cast<MSG*>(parameter);
    const bool attachment_message = message.message == session->attach_message;
    const bool update_message = message.message == session->update_message;

    if (!attachment_message && !update_message) {
        return CallNextHookEx(nullptr, code, removal, parameter);
    }
    // Register before inspecting mutable candidate state. Failed attachment
    // attempts unhook and drain this count before selecting another window.
    session->message_callbacks.fetch_add(1, std::memory_order_acq_rel);
    const bool target_window = message.hwnd == session->window.load(std::memory_order_acquire);
    const bool attach = target_window && attachment_message &&
        session->attaching.load(std::memory_order_acquire) &&
        message.wParam == session->attach_generation.load(std::memory_order_acquire);
    const bool update = target_window && update_message;

    if (attach) {
        overlay::install_window_subclass(message.hwnd, session->services.overlay.overlay);
        session->attach_finished.store(true, std::memory_order_release);
    } else if (update) {
        LRESULT result = 0;

        // A game can remove registered messages without DispatchMessage. Run
        // our update on this owning thread before the game can discard it.
        dispatch_window_message(session, message.hwnd, message.message,
            message.wParam, message.lParam, &result);
    }
    // PM_NOREMOVE never enters this path, and a later DispatchMessage must not
    // execute the same update again through the window subclass.
    if (attach || update) {
        message.message = WM_NULL;
    }
    const LRESULT result = CallNextHookEx(nullptr, code, removal, parameter);

    session->message_callbacks.fetch_sub(1, std::memory_order_release);
    if (attachment_message || session->attaching.load(std::memory_order_acquire) ||
        session->stop_requested.load(std::memory_order_acquire) ||
        session->window_destroyed.load(std::memory_order_acquire)) {
        wake_coordinator(*session);
    }
    return result;
}

void CALLBACK window_shown(HWINEVENTHOOK, DWORD event, HWND window, LONG object, LONG child, DWORD, DWORD) {
    RuntimeSession* const session = active_session.load(std::memory_order_acquire);

    if (session && object == OBJID_WINDOW && child == CHILDID_SELF) {
        if (event == EVENT_OBJECT_DESTROY && window == session->window.load(std::memory_order_acquire)) {
            session->candidate_destroyed.store(true, std::memory_order_release);
        }
        wake_coordinator(*session);
    }
}

struct WindowSearch {
    HWND window = nullptr;
    std::uint64_t area = 0;
};

BOOL CALLBACK consider_window(HWND window, LPARAM context) {
    WindowSearch& search = *reinterpret_cast<WindowSearch*>(context);
    DWORD process = 0;
    RECT rectangle{};

    GetWindowThreadProcessId(window, &process);
    if (process != GetCurrentProcessId() || !IsWindowVisible(window) ||
        GetWindow(window, GW_OWNER) || (GetWindowLongPtrW(window, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) ||
        !GetClientRect(window, &rectangle)) {
        return TRUE;
    }

    const LONG width = rectangle.right - rectangle.left;
    const LONG height = rectangle.bottom - rectangle.top;
    constexpr LONG MINIMUM_WIDTH = 320;
    constexpr LONG MINIMUM_HEIGHT = 200;

    if (width >= MINIMUM_WIDTH && height >= MINIMUM_HEIGHT) {
        const std::uint64_t area = static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height);

        if (area > search.area) {
            search = {window, area};
        }
    }
    return TRUE;
}

bool wait_for_work(RuntimeSession& session, HANDLE watched_thread = nullptr) noexcept {
    const std::array<HANDLE, 2> events{session.wake.get(), watched_thread};
    const DWORD count = watched_thread ? 2 : 1;
    const DWORD reason = MsgWaitForMultipleObjectsEx(count, events.data(), INFINITE, QS_ALLINPUT, MWMO_INPUTAVAILABLE);

    if (reason == WAIT_FAILED) {
        return false;
    }
    if (watched_thread && reason == WAIT_OBJECT_0 + 1) {
        session.candidate_destroyed.store(true, std::memory_order_release);
    }
    MSG message{};

    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
        if (message.message == WM_QUIT) {
            return false;
        }
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return true;
}

bool wait_for_message_callbacks(RuntimeSession& session) noexcept {
    while (session.message_callbacks.load(std::memory_order_acquire) != 0) {
        if (!wait_for_work(session)) {
            return false;
        }
    }
    return true;
}

bool attach_dispatcher(RuntimeSession& session) {
    session.window_events = SetWinEventHook(EVENT_OBJECT_DESTROY, EVENT_OBJECT_SHOW, nullptr,
        window_shown, GetCurrentProcessId(), 0, WINEVENT_OUTOFCONTEXT);

    if (!session.window_events) {
        return false;
    }
    for (;;) {
        WindowSearch search;

        EnumWindows(consider_window, reinterpret_cast<LPARAM>(&search));
        if (!search.window) {
            if (!wait_for_work(session)) {
                return false;
            }
            continue;
        }
        DWORD process = 0;
        const DWORD window_thread = GetWindowThreadProcessId(search.window, &process);

        if (!window_thread || process != GetCurrentProcessId()) {
            continue;
        }
        winrt::handle thread{OpenThread(SYNCHRONIZE, FALSE, window_thread)};

        if (!thread) {
            return false;
        }
        session.window.store(search.window, std::memory_order_release);
        session.attach_finished.store(false, std::memory_order_release);
        session.candidate_destroyed.store(false, std::memory_order_release);
        const UINT_PTR generation = session.attach_generation.fetch_add(1, std::memory_order_acq_rel) + 1;

        // A zero thread ID would create a desktop-wide hook. This hook is
        // strictly thread-specific and same-process, so hMod must be null.
        session.message_hook = SetWindowsHookExW(WH_GETMESSAGE, dispatch_on_window_thread, nullptr, window_thread);
        if (!session.message_hook) {
            return false;
        }
        const bool posted = PostMessageW(search.window, session.attach_message, generation, 0) != FALSE;
        bool wait_succeeded = true;

        while (posted && !session.attach_finished.load(std::memory_order_acquire) &&
               !session.candidate_destroyed.load(std::memory_order_acquire)) {
            if (!wait_for_work(session, thread.get())) {
                wait_succeeded = false;
                break;
            }
        }

        session.attach_generation.fetch_add(1, std::memory_order_acq_rel);
        const bool attached = wait_succeeded && posted &&
            !session.candidate_destroyed.load(std::memory_order_acquire) && IsWindow(search.window) &&
            session.services.overlay.overlay.window_attached.load(std::memory_order_acquire);

        if (!attached && !remove_message_hook(session)) {
            return false;
        }
        if (!wait_for_message_callbacks(session)) {
            return false;
        }
        if (attached && !session.candidate_destroyed.load(std::memory_order_acquire) &&
            IsWindow(search.window) && session.services.overlay.overlay.window_attached.load(std::memory_order_acquire)) {
            // Keep WH_GETMESSAGE until shutdown so update delivery does not
            // depend on the game's treatment of registered window messages.
            session.attaching.store(false, std::memory_order_release);
            UnhookWinEvent(session.window_events);
            session.window_events = nullptr;
            return true;
        }
        if (!remove_message_hook(session) || !wait_for_message_callbacks(session)) {
            return false;
        }
        if (!wait_succeeded) {
            return false;
        }
        if (session.candidate_destroyed.load(std::memory_order_acquire) || !IsWindow(search.window)) {
            session.services.overlay.overlay.window_attached.store(false, std::memory_order_release);
            continue;
        }
        return false;
    }
}

[[noreturn]] void retain_session(RuntimeSession& session) noexcept {
    // Disabled code may still have a returning native caller. Keep its owner
    // alive on a dormant coordinator; this path has no periodic wake or sleep.
    bool services_finished = false;
    bool message_hook_failure_reported = false;
    std::uint64_t prepared_generation = 0;

    for (;;) {
        const auto requested_generation = session.image_refresh_requested.load(std::memory_order_acquire);
        if (requested_generation != prepared_generation &&
            !session.stop_requested.load(std::memory_order_acquire) &&
            !session.window_destroyed.load(std::memory_order_acquire)) {
            prepared_generation = requested_generation;
            try {
                // No Runtime or patch state is touched on this thread after
                // publication. Heavy reads/searches use a private snapshot.
                auto image = platform::windows::read_module_image(session.game_module);
                if (!image) {
                    session.image_refresh_failed.store(requested_generation, std::memory_order_release);
                } else {
                    auto prepared = own::make_unique<PreparedGameImage>();
                    prepared->module = session.game_module;
                    prepared->generation = requested_generation;
                    prepared->image = std::move(*image);
                    prepared->signatures = prepare_native_signature_values(prepared->module, prepared->image);
                    prepared->metadata_offset = game::find_stratagem5_metadata_offset(prepared->image);
                    const std::lock_guard lock(session.prepared_image_mutex);
                    session.prepared_image = std::move(prepared);
                }
            } catch (...) {
                session.image_refresh_failed.store(requested_generation, std::memory_order_release);
            }
            post_update(session);
        }
        if (!services_finished && !session.dispatch_active.load(std::memory_order_acquire) &&
            (session.game_stopped.load(std::memory_order_acquire) ||
                                   session.window_destroyed.load(std::memory_order_acquire))) {
            if (!remove_message_hook(session)) {
                if (!message_hook_failure_reported) {
                    OutputDebugStringW(L"S8RPKCheats: message hook removal failed; retaining services.\n");
                    message_hook_failure_reported = true;
                }
            } else if (session.message_callbacks.load(std::memory_order_acquire) == 0) {
                try {
                    stop_runtime_entries(session.services);
                    finish_runtime_services(session.services);
                    services_finished = true;
                } catch (...) {
                    OutputDebugStringW(L"S8RPKCheats: service shutdown failed; retaining owners.\n");
                }
            }
        }
        if (session.game_stopped.load(std::memory_order_acquire) &&
            session.services.overlay.overlay.render_stopped.load(std::memory_order_acquire)) {
            finish_overlay_stop(session.services.overlay);
        }
        if (!wait_for_work(session)) {
            session.stop_requested.store(true, std::memory_order_release);
            post_update(session);
            WaitForSingleObject(session.wake.get(), INFINITE);
        }
    }
}

DWORD WINAPI coordinator_thread(void* context) noexcept {
    const std::uintptr_t plugin_module = reinterpret_cast<std::uintptr_t>(context);
    const platform::windows::RealClockScope real_clock;
    own::unique_owner<RuntimeSession> session;

    try {
        session = own::make_unique<RuntimeSession>();
        session->plugin_module = plugin_module;
        session->game_module = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
        session->wake.attach(CreateEventW(nullptr, FALSE, FALSE, nullptr));
        wchar_t instance_name[96]{};

        swprintf_s(instance_name, L"Local\\29thnight.S8RPKCheats.Runtime.%lu", GetCurrentProcessId());
        session->instance.attach(CreateMutexW(nullptr, FALSE, instance_name));
        const DWORD instance_error = GetLastError();

        if (!session->wake || !session->instance || instance_error == ERROR_ALREADY_EXISTS) {
            return 0;
        }

        session->attach_message = RegisterWindowMessageW(L"29thnight.S8RPKCheats.Attach.v1");
        session->update_message = RegisterWindowMessageW(L"29thnight.S8RPKCheats.Update.v1");
        if (!session->attach_message || !session->update_message) {
            return 0;
        }
        SetThreadDescription(GetCurrentThread(), L"S8RPKCheats coordinator");
        initialize_game_base_from_module(session->runtime, session->game_module);
        prepare_native_signatures(session->runtime, session->game_module);
        prepare_battle_dispatch(session->battle_dispatch, session->runtime, session->game_module);

        overlay::OverlayPresent& overlay = session->services.overlay.overlay;

        overlay.message_callback = dispatch_window_message;
        overlay.message_context = &*session;
        overlay.frame_callback = frame_ready;
        overlay.frame_context = &*session;
        active_session.store(&*session, std::memory_order_release);

        if (!attach_dispatcher(*session)) {
            OutputDebugStringW(L"S8RPKCheats: window dispatcher could not attach.\n");
            retain_session(*session);
        }
        if (session->window_destroyed.load(std::memory_order_acquire)) {
            session->game_stopped.store(true, std::memory_order_release);
            session->services.overlay.overlay.render_stopped.store(true, std::memory_order_release);
            retain_session(*session);
        }

        const std::expected<void, int> started =
            start_runtime_services(session->services, session->window.load(std::memory_order_acquire), plugin_module);

        if (!started) {
            OutputDebugStringW(L"S8RPKCheats: runtime services could not start.\n");
            session->stop_requested.store(true, std::memory_order_release);
        }
        session->running.store(true, std::memory_order_release);
        post_update(*session);

        retain_session(*session);
    } catch (...) {
        OutputDebugStringW(L"S8RPKCheats: initialization failed; published callbacks remain retained.\n");
        if (session && session->wake && active_session.load(std::memory_order_acquire) == &*session) {
            session->stop_requested.store(true, std::memory_order_release);
            post_update(*session);
            retain_session(*session);
        }
    }
    return 0;
}

} // namespace

bool start_runtime_async(std::uintptr_t plugin_module) noexcept {
    HMODULE retained_module = nullptr;

    // Pin the already loaded proxy before scheduling the worker. No DLL load,
    // hook installation, allocation of session state, or wait occurs here.
    if (!plugin_module || !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
            GET_MODULE_HANDLE_EX_FLAG_PIN, reinterpret_cast<LPCWSTR>(plugin_module), &retained_module)) {
        return false;
    }

    const HANDLE thread = CreateThread(nullptr, 0, coordinator_thread,
                                       reinterpret_cast<void*>(plugin_module), 0, nullptr);

    if (!thread) {
        return false;
    }
    CloseHandle(thread);
    return true;
}

} // namespace s8::runtime
