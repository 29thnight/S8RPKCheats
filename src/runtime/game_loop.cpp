#include "game_loop.h"

#include "runtime.h"
#include "runtime_services.h"
#include "command_commit.h"
#include "debug_tools.h"
#include "native_preparation.h"
#include "../domain/ownership.h"
#include "../overlay/window_subclass.h"
#include "../platform/windows/speed_clock.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <winrt/base.h>

#include <atomic>
#include <array>
#include <chrono>
#include <cwchar>
#include <expected>

namespace s8::runtime {
namespace {

constexpr std::size_t COMMANDS_PER_DISPATCH = 4;
constexpr std::chrono::microseconds COMMAND_BUDGET{750};
constexpr std::uint64_t STOP_RETRY_INTERVAL_MS = 100;

struct RuntimeSession {
    Runtime runtime;
    DebugTools debug_tools;
    RuntimeServices services;
    winrt::handle wake;
    winrt::handle instance;
    std::uintptr_t plugin_module = 0;
    std::uintptr_t game_module = 0;
    std::atomic<HWND> window{nullptr};
    HHOOK attach_hook = nullptr;
    HWINEVENTHOOK window_events = nullptr;
    UINT attach_message = 0;
    UINT update_message = 0;
    std::atomic<bool> attach_finished{false};
    std::atomic<bool> attaching{true};
    std::atomic<bool> candidate_destroyed{false};
    std::atomic<unsigned> attach_callbacks{0};
    std::atomic<UINT_PTR> attach_generation{0};
    std::atomic<bool> running{false};
    std::atomic<bool> update_pending{false};
    std::atomic<bool> stop_requested{false};
    std::atomic<bool> game_stopped{false};
    std::atomic<bool> window_destroyed{false};
    std::atomic<bool> dispatch_active{false};
    UINT_PTR stop_timer = 0;
    bool dispatching = false;
    std::uint64_t next_stop_attempt_ms = 0;
    bool stop_failure_reported = false;
};

// This pointer borrows the coordinator's process-lifetime owner. Published
// callbacks cannot outlive the pinned module, event, or RuntimeSession storage.
std::atomic<RuntimeSession*> active_session{nullptr};

void wake_coordinator(RuntimeSession& session) noexcept {
    SetEvent(session.wake.get());
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
            stop_debug_tools(session.debug_tools, session.services.overlay.bridge.debug_tools);
            stop_runtime_entries(session.services);
            OutputDebugStringW(L"S8RPKCheats: window lost; native state retained without further game writes.\n");
        } else if (session.stop_requested.load(std::memory_order_acquire) &&
            session.running.load(std::memory_order_acquire)) {
            stop_on_window_thread(session, GetTickCount64());
        } else if (session.running.load(std::memory_order_acquire)) {
            // A window thread is an explicit dispatch context, not proof of
            // exclusive game-state ownership. Session/target guards remain
            // mandatory around every operation that reads or changes the game.
            tick_game(session.runtime, session.game_module, GetTickCount64());
            if (!session.stop_requested.load(std::memory_order_acquire)) {
                commit_ui_commands(session.runtime, session.services.overlay.bridge,
                                   session.game_module, COMMANDS_PER_DISPATCH, COMMAND_BUDGET);
                update_debug_tools(session.debug_tools, session.services.overlay.bridge.debug_tools,
                    session.runtime.epoch, session.game_module, session.runtime.loop_game_base,
                    session.runtime.loop_player, GetTickCount64());
            }
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

LRESULT CALLBACK attach_on_window_thread(int code, WPARAM removal, LPARAM parameter) {
    RuntimeSession* const session = active_session.load(std::memory_order_acquire);

    if (code >= 0 && removal == PM_REMOVE && session && parameter) {
        session->attach_callbacks.fetch_add(1, std::memory_order_acq_rel);
        MSG& message = *reinterpret_cast<MSG*>(parameter);

        if (message.hwnd == session->window.load(std::memory_order_acquire) &&
            message.message == session->attach_message &&
            message.wParam == session->attach_generation.load(std::memory_order_acquire)) {
            overlay::install_window_subclass(message.hwnd, session->services.overlay.overlay);
            message.message = WM_NULL;
            session->attach_finished.store(true, std::memory_order_release);
            wake_coordinator(*session);
        }
        session->attach_callbacks.fetch_sub(1, std::memory_order_release);
        wake_coordinator(*session);
    }
    return CallNextHookEx(nullptr, code, removal, parameter);
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
        session.attach_hook = SetWindowsHookExW(WH_GETMESSAGE, attach_on_window_thread, nullptr, window_thread);
        if (!session.attach_hook) {
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
        if (!UnhookWindowsHookEx(session.attach_hook)) {
            return false;
        }
        session.attach_hook = nullptr;
        while (session.attach_callbacks.load(std::memory_order_acquire) != 0) {
            if (!wait_for_work(session)) {
                return false;
            }
        }
        if (!wait_succeeded) {
            return false;
        }
        if (session.candidate_destroyed.load(std::memory_order_acquire) || !IsWindow(search.window)) {
            session.services.overlay.overlay.window_attached.store(false, std::memory_order_release);
            continue;
        }
        if (!posted || !session.services.overlay.overlay.window_attached.load(std::memory_order_acquire)) {
            return false;
        }

        session.attaching.store(false, std::memory_order_release);
        UnhookWinEvent(session.window_events);
        session.window_events = nullptr;
        return true;
    }
}

[[noreturn]] void retain_session(RuntimeSession& session) noexcept {
    // Disabled code may still have a returning native caller. Keep its owner
    // alive on a dormant coordinator; this path has no periodic wake or sleep.
    bool services_finished = false;

    for (;;) {
        if (!services_finished && !session.dispatch_active.load(std::memory_order_acquire) &&
            (session.game_stopped.load(std::memory_order_acquire) ||
                                   session.window_destroyed.load(std::memory_order_acquire))) {
            try {
                stop_runtime_entries(session.services);
                finish_runtime_services(session.services);
                services_finished = true;
            } catch (...) {
                OutputDebugStringW(L"S8RPKCheats: service shutdown failed; retaining owners.\n");
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
