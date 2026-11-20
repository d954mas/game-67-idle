/* The only translation unit that includes Steam's C++ headers: it implements the C contract
   over them with manual callback dispatch, so nothing in the SDK calls the game back. */

#include "steam_platform_common.h"
#include "steam_platform_internal.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include <steam/steam_api.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static_assert(STEAM_PLATFORM_INTERNAL_INIT_FAILED_GENERIC == k_ESteamAPIInitResult_FailedGeneric, "init result mismatch");
static_assert(STEAM_PLATFORM_INTERNAL_INIT_NO_STEAM_CLIENT == k_ESteamAPIInitResult_NoSteamClient, "init result mismatch");
static_assert(STEAM_PLATFORM_INTERNAL_INIT_VERSION_MISMATCH == k_ESteamAPIInitResult_VersionMismatch, "init result mismatch");
static_assert(sizeof(SteamAPICall_t) == sizeof(uint64_t), "call handle width");

namespace {

struct listener {
    int callback_id;
    steam_platform_callback_fn fn;
    void *user;
};

struct pending_call {
    SteamAPICall_t handle;
    int expected_callback_id;
    steam_platform_call_result_fn fn;
    void *user;
};

struct state {
    bool ready;
    bool in_frame;
    /* Set when stop is called inside run_frame: Steam is shut down only after the callback
       being dispatched has been freed. */
    bool shutdown_pending;
    HSteamPipe pipe;
    listener listeners[STEAM_PLATFORM_INTERNAL_MAX_LISTENERS];
    pending_call calls[STEAM_PLATFORM_INTERNAL_MAX_CALLS];
};

struct simulated_failure {
    bool armed;
    bool restart;
    int init_result;
    char message[256];
};

state s_state;
steam_platform_event_queue_t s_queue;
steam_platform_internal_counters_t s_counters;
simulated_failure s_simulated;
alignas(16) unsigned char s_call_result[STEAM_PLATFORM_INTERNAL_CALL_RESULT_BYTES];

#if defined(__clang__) || defined(__GNUC__)
__attribute__((format(printf, 3, 4)))
#endif
void write_message(char *message, size_t message_size, const char *format, ...) {
    if (message == nullptr || message_size == 0) {
        return;
    }
    va_list args;
    va_start(args, format);
    int written = vsnprintf(message, message_size, format, args);
    va_end(args);
    if (written < 0) {
        message[0] = '\0';
    }
}

/* The runtime library is delay-loaded on Windows: without this check a missing DLL would end
   the process inside the first Steam call. Loading it here, from the application's folder
   only, also makes the delay-load helper reuse this copy. */
bool runtime_present(char *message, size_t message_size) {
#ifdef _WIN32
    static HMODULE s_runtime;
    if (s_runtime != nullptr) {
        return true;
    }
    /* A copy something else already loaded is the one the delay-load helper binds to. */
    s_runtime = GetModuleHandleA(STEAM_PLATFORM_RUNTIME_NAME);
    if (s_runtime != nullptr) {
        return true;
    }
    /* By full path, so only the copy beside the executable is taken; its own dependencies
       resolve as for a static import. */
    wchar_t path[1024];
    const DWORD length = GetModuleFileNameW(nullptr, path, static_cast<DWORD>(sizeof path / sizeof path[0]));
    const char name[] = STEAM_PLATFORM_RUNTIME_NAME;
    DWORD folder = length;
    while (folder > 0 && path[folder - 1] != L'\\' && path[folder - 1] != L'/') {
        folder--;
    }
    if (length == 0 || length >= sizeof path / sizeof path[0] || folder + sizeof name > sizeof path / sizeof path[0]) {
        write_message(message, message_size, "cannot locate the executable's folder to load %s", STEAM_PLATFORM_RUNTIME_NAME);
        return false;
    }
    for (size_t i = 0; i < sizeof name; i++) {
        path[folder + i] = static_cast<wchar_t>(name[i]);
    }
    DWORD old_mode = 0;
    SetThreadErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX, &old_mode);
    s_runtime = LoadLibraryExW(path, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    const DWORD error = GetLastError();
    SetThreadErrorMode(old_mode, nullptr);
    if (s_runtime == nullptr) {
        write_message(message, message_size, "%s is missing beside the executable (Windows error %lu)",
                      STEAM_PLATFORM_RUNTIME_NAME, static_cast<unsigned long>(error));
        return false;
    }
#else
    (void)message;
    (void)message_size;
#endif
    return true;
}

void forget_everything() {
    steam_platform_queue_reset(&s_queue);
    memset(s_state.listeners, 0, sizeof s_state.listeners);
    memset(s_state.calls, 0, sizeof s_state.calls);
}

bool deliver_call_result(const CallbackMsg_t &msg) {
    if (msg.m_cubParam < static_cast<int>(sizeof(SteamAPICallCompleted_t))) {
        return false;
    }
    const auto *completed = reinterpret_cast<const SteamAPICallCompleted_t *>(msg.m_pubParam);
    for (pending_call &slot : s_state.calls) {
        if (slot.fn == nullptr || slot.handle != completed->m_hAsyncCall) {
            continue;
        }
        /* Freed before the handler runs, so the handler may await its next call in this slot. */
        pending_call call = slot;
        slot = pending_call{};
        bool io_failed = true;
        const void *data = nullptr;
        size_t size = 0;
        if (completed->m_cubParam <= sizeof s_call_result
            && SteamAPI_ManualDispatch_GetAPICallResult(s_state.pipe, call.handle, s_call_result,
                                                        static_cast<int>(completed->m_cubParam),
                                                        call.expected_callback_id, &io_failed)) {
            data = s_call_result;
            size = completed->m_cubParam;
        } else {
            io_failed = true;
        }
        s_counters.calls_delivered++;
        call.fn(call.user, completed->m_iCallback, data, size, io_failed);
        return true;
    }
    return false;
}

bool dispatch(const CallbackMsg_t &msg) {
    if (msg.m_iCallback == SteamAPICallCompleted_t::k_iCallback) {
        return deliver_call_result(msg);
    }
    bool handled = false;
    if (msg.m_iCallback == GameOverlayActivated_t::k_iCallback
        && msg.m_cubParam >= static_cast<int>(sizeof(GameOverlayActivated_t))) {
        const auto *overlay = reinterpret_cast<const GameOverlayActivated_t *>(msg.m_pubParam);
        steam_platform_queue_overlay(&s_queue, overlay->m_bActive != 0);
        handled = true;
    }
    /* By index and re-read each time: a handler may change the table while this loop runs. */
    for (unsigned i = 0; i < STEAM_PLATFORM_INTERNAL_MAX_LISTENERS && s_state.ready; i++) {
        listener entry = s_state.listeners[i];
        if (entry.fn != nullptr && entry.callback_id == msg.m_iCallback) {
            entry.fn(entry.user, msg.m_iCallback, msg.m_pubParam, static_cast<size_t>(msg.m_cubParam));
            handled = true;
        }
    }
    return handled;
}

void take(const CallbackMsg_t &msg) {
    s_counters.callbacks++;
    if (!dispatch(msg)) {
        s_counters.unhandled++;
    }
}

} // namespace

extern "C" {

steam_platform_status_t steam_platform_start(const steam_platform_config_t *config, char *message, size_t message_size) {
    if (steam_platform_check_config(config, message, message_size) != STEAM_PLATFORM_READY) {
        return STEAM_PLATFORM_FAILED;
    }
    if (s_state.ready) {
        write_message(message, message_size, "already started");
        return STEAM_PLATFORM_READY;
    }
    if (s_state.in_frame) {
        write_message(message, message_size, "cannot start inside run_frame");
        return STEAM_PLATFORM_FAILED;
    }
    /* Consumed by this start whatever its answer, so it never reaches a later one. */
    const bool simulated = s_simulated.armed;
    s_simulated.armed = false;
    if (!runtime_present(message, message_size)) {
        return STEAM_PLATFORM_FAILED;
    }

    if (config->restart_through_steam) {
        const bool restart = simulated ? s_simulated.restart : SteamAPI_RestartAppIfNecessary(config->app_id);
        if (restart) {
            write_message(message, message_size, "Steam is starting the app again");
            return STEAM_PLATFORM_RESTARTING;
        }
    }

    SteamErrMsg error = "";
    ESteamAPIInitResult result;
    if (simulated) {
        result = static_cast<ESteamAPIInitResult>(s_simulated.init_result);
        if (result == k_ESteamAPIInitResult_OK) {
            result = k_ESteamAPIInitResult_FailedGeneric;
        }
        snprintf(error, sizeof error, "%s", s_simulated.message);
    } else {
        result = SteamAPI_InitEx(&error);
    }
    if (result != k_ESteamAPIInitResult_OK) {
        if (result == k_ESteamAPIInitResult_NoSteamClient) {
            write_message(message, message_size, "%s", error[0] != '\0' ? error : "Steam is not running");
            return STEAM_PLATFORM_NO_CLIENT;
        }
        write_message(message, message_size, "%s", error[0] != '\0' ? error : "Steam init failed");
        return STEAM_PLATFORM_FAILED;
    }

    if (SteamUser() == nullptr || SteamUtils() == nullptr || SteamFriends() == nullptr) {
        SteamAPI_Shutdown();
        write_message(message, message_size, "Steam started without its user interfaces");
        return STEAM_PLATFORM_FAILED;
    }
    const AppId_t running = SteamUtils()->GetAppID();
    if (config->app_id != 0 && running != config->app_id) {
        SteamAPI_Shutdown();
        write_message(message, message_size, "Steam started app %u, this build is app %u", running, config->app_id);
        return STEAM_PLATFORM_FAILED;
    }

    SteamAPI_ManualDispatch_Init();
    s_state.pipe = SteamAPI_GetHSteamPipe();
    forget_everything();
    s_state.shutdown_pending = false;
    s_state.ready = true;
    write_message(message, message_size, "ready, app %u", running);
    return STEAM_PLATFORM_READY;
}

void steam_platform_stop(void) {
    /* Also when stopped: events fed by the test hook drop the same way. */
    steam_platform_queue_reset(&s_queue);
    if (!s_state.ready) {
        return;
    }
    s_state.ready = false;
    forget_everything();
    if (s_state.in_frame) {
        s_state.shutdown_pending = true;
        return;
    }
    SteamAPI_Shutdown();
}

bool steam_platform_ready(void) { return s_state.ready; }

void steam_platform_run_frame(void) {
    if (!s_state.ready || s_state.in_frame) {
        return;
    }
    s_state.in_frame = true;
    SteamAPI_ManualDispatch_RunFrame(s_state.pipe);
    CallbackMsg_t msg;
    while (s_state.ready && SteamAPI_ManualDispatch_GetNextCallback(s_state.pipe, &msg)) {
        take(msg);
        SteamAPI_ManualDispatch_FreeLastCallback(s_state.pipe);
    }
    /* Under manual dispatch nothing else frees the per-thread memory Steam calls allocate. */
    SteamAPI_ReleaseCurrentThreadMemory();
    s_state.in_frame = false;
    if (s_state.shutdown_pending) {
        s_state.shutdown_pending = false;
        SteamAPI_Shutdown();
    }
}

bool steam_platform_next_event(steam_platform_event_t *out) { return steam_platform_queue_pop(&s_queue, out); }

uint64_t steam_platform_user(void) {
    return s_state.ready ? SteamUser()->GetSteamID().ConvertToUint64() : 0;
}

bool steam_platform_overlay_ready(void) { return s_state.ready && SteamUtils()->IsOverlayEnabled(); }

void steam_platform_open_overlay(const char *dialog) {
    if (!s_state.ready) {
        return;
    }
    SteamFriends()->ActivateGameOverlay(steam_platform_overlay_dialog(dialog));
}

bool steam_platform_internal_listen(int callback_id, steam_platform_callback_fn fn, void *user) {
    if (!s_state.ready || fn == nullptr) {
        return false;
    }
    listener *free_slot = nullptr;
    for (listener &entry : s_state.listeners) {
        if (entry.fn == fn && entry.callback_id == callback_id && entry.user == user) {
            return false;
        }
        if (entry.fn == nullptr && free_slot == nullptr) {
            free_slot = &entry;
        }
    }
    if (free_slot == nullptr) {
        return false;
    }
    *free_slot = listener{callback_id, fn, user};
    return true;
}

void steam_platform_internal_unlisten(int callback_id, steam_platform_callback_fn fn, void *user) {
    for (listener &entry : s_state.listeners) {
        if (entry.fn == fn && entry.callback_id == callback_id && entry.user == user) {
            entry = listener{};
        }
    }
}

bool steam_platform_internal_await_call(uint64_t api_call, int expected_callback_id, steam_platform_call_result_fn fn, void *user) {
    if (!s_state.ready || fn == nullptr || api_call == k_uAPICallInvalid) {
        return false;
    }
    pending_call *free_slot = nullptr;
    for (pending_call &slot : s_state.calls) {
        if (slot.fn != nullptr && slot.handle == api_call) {
            return false;
        }
        if (slot.fn == nullptr && free_slot == nullptr) {
            free_slot = &slot;
        }
    }
    if (free_slot == nullptr) {
        return false;
    }
    *free_slot = pending_call{api_call, expected_callback_id, fn, user};
    return true;
}

bool steam_platform_internal_cancel_call(uint64_t api_call) {
    for (pending_call &slot : s_state.calls) {
        if (slot.fn != nullptr && slot.handle == api_call) {
            slot = pending_call{};
            return true;
        }
    }
    return false;
}

void steam_platform_internal_counters(steam_platform_internal_counters_t *out) {
    if (out != nullptr) {
        *out = s_counters;
        out->events_dropped = s_queue.dropped;
    }
}

void steam_platform_internal_simulate_failure(bool restart, int init_result, const char *message) {
    s_simulated.armed = true;
    s_simulated.restart = restart;
    s_simulated.init_result = init_result;
    snprintf(s_simulated.message, sizeof s_simulated.message, "%s", message != nullptr ? message : "");
}

bool steam_platform_internal_feed(int callback_id, const void *data, size_t size) {
    /* A call result needs Steam to read it back, so it cannot be fed. */
    if (callback_id == SteamAPICallCompleted_t::k_iCallback || (data == nullptr && size != 0) || size > INT32_MAX) {
        return false;
    }
    CallbackMsg_t msg{};
    msg.m_iCallback = callback_id;
    msg.m_pubParam = static_cast<uint8 *>(const_cast<void *>(data));
    msg.m_cubParam = static_cast<int>(size);
    take(msg);
    return true;
}

} // extern "C"
