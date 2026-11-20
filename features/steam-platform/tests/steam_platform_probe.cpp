/* Live probe against the developer's Steam client: refuse a start into the wrong app, start as
   Spacewar, receive callbacks and call results through the module's manual dispatch, stop
   from inside a handler, and start again.
   Exit 0 passed, 1 failed, 77 skipped (Steam is not running).
   --simulate-no-client takes the skip path without touching Steam. */

#include "steam_live.hpp"
#include "steam_platform_internal.h"

#include <steam/steam_api.h>

#include <chrono>
#include <stdio.h>
#include <string.h>
#include <thread>

namespace {

constexpr uint32_t SPACEWAR = 480;

struct relay_log {
    int count;
    SteamRelayNetworkStatus_t last;
};

struct connection_log {
    int count;
    int last_state;
};

struct call_log {
    int count;
    bool io_failed;
    int callback_id;
    size_t size;
    int result;
};

void on_relay_status(void *user, int callback_id, const void *data, size_t size) {
    auto *log = static_cast<relay_log *>(user);
    if (callback_id == SteamRelayNetworkStatus_t::k_iCallback && size >= sizeof(SteamRelayNetworkStatus_t)) {
        memcpy(&log->last, data, sizeof log->last);
        log->count++;
    }
}

void on_connection_status(void *user, int callback_id, const void *data, size_t size) {
    auto *log = static_cast<connection_log *>(user);
    if (callback_id == SteamNetConnectionStatusChangedCallback_t::k_iCallback
        && size >= sizeof(SteamNetConnectionStatusChangedCallback_t)) {
        log->last_state = static_cast<const SteamNetConnectionStatusChangedCallback_t *>(data)->m_info.m_eState;
        log->count++;
    }
}

int s_stops_inside;
void stop_inside(void *user, int callback_id, const void *data, size_t size) {
    s_stops_inside++;
    steam_platform_stop();
}

void on_signature(void *user, int callback_id, const void *data, size_t size, bool io_failed) {
    auto *log = static_cast<call_log *>(user);
    log->count++;
    log->io_failed = io_failed;
    log->callback_id = callback_id;
    log->size = size;
    if (data != nullptr && size >= sizeof(CheckFileSignature_t)) {
        log->result = static_cast<const CheckFileSignature_t *>(data)->m_eCheckFileSignature;
    }
}

int fail(const char *what) {
    printf("FAIL: %s\n", what);
    steam_platform_stop();
    return 1;
}

using clock_type = std::chrono::steady_clock;

double seconds_since(clock_type::time_point start) {
    return std::chrono::duration<double>(clock_type::now() - start).count();
}

void frame() {
    steam_platform_run_frame();
    steam_platform_event_t event;
    while (steam_platform_next_event(&event)) {
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(16));
}

template <typename Done> bool pump_until(double limit_s, Done done) {
    const auto start = clock_type::now();
    while (seconds_since(start) < limit_s) {
        if (done()) {
            return true;
        }
        frame();
    }
    return done();
}

/* Two connected local sockets, one end closed at once: a pair is born connected and posts
   nothing, the close posts a status change for each end, which comes through the dispatch by
   id without ISteamNetworkingSockets::RunCallbacks. */
void socket_pair(HSteamNetConnection *a, HSteamNetConnection *b) {
    SteamNetworkingSockets()->CreateSocketPair(a, b, false, nullptr, nullptr);
    SteamNetworkingSockets()->CloseConnection(*a, 0, "probe", false);
}

void close_pair(HSteamNetConnection a, HSteamNetConnection b) {
    SteamNetworkingSockets()->CloseConnection(a, 0, "probe", false);
    SteamNetworkingSockets()->CloseConnection(b, 0, "probe", false);
}

int start_ready(const steam_platform_config_t &config, const char *label) {
    char message[256];
    const steam_platform_status_t status = steam_platform_start(&config, message, sizeof message);
    if (status == STEAM_PLATFORM_NO_CLIENT) {
        return steam_live::no_client(message);
    }
    if (status != STEAM_PLATFORM_READY) {
        printf("FAIL: %s answered %d: %s\n", label, static_cast<int>(status), message);
        return 1;
    }
    printf("%s: %s\n", label, message);
    return 0;
}

int start_rules(const steam_platform_config_t &config) {
    const uint64_t user = steam_platform_user();
    char message[256];
    if (steam_platform_start(&config, message, sizeof message) != STEAM_PLATFORM_READY || !steam_platform_ready()) {
        return fail("a second start while ready did not answer READY");
    }
    if (steam_platform_start(nullptr, message, sizeof message) != STEAM_PLATFORM_FAILED || !steam_platform_ready()) {
        return fail("start(NULL) while ready did not answer FAILED and stay ready");
    }
    if (steam_platform_user() != user) {
        return fail("the user changed across refused starts");
    }
    printf("start rules while ready: READY again, NULL config FAILED, still ready\n");
    return 0;
}

int first_cycle(const char *self_path) {
    const uint64_t user = steam_platform_user();
    if (user == 0) {
        return fail("steam_platform_user() is zero while ready");
    }
    printf("user: ...%04u\n", static_cast<unsigned>(user % 10000U));

    steam_platform_internal_counters_t before;
    steam_platform_internal_counters(&before);

    relay_log relay{};
    connection_log connection{};
    if (!steam_platform_internal_listen(SteamRelayNetworkStatus_t::k_iCallback, on_relay_status, &relay)
        || !steam_platform_internal_listen(SteamNetConnectionStatusChangedCallback_t::k_iCallback, on_connection_status,
                                           &connection)) {
        return fail("listen refused");
    }
    if (steam_platform_internal_listen(SteamRelayNetworkStatus_t::k_iCallback, on_relay_status, &relay)) {
        return fail("the same listener was accepted twice");
    }
    HSteamNetConnection a = 0;
    HSteamNetConnection b = 0;
    socket_pair(&a, &b);
    SteamNetworkingUtils()->InitRelayNetworkAccess();

    call_log call{};
    const SteamAPICall_t handle = SteamUtils()->CheckFileSignature(self_path);
    if (!steam_platform_internal_await_call(handle, CheckFileSignature_t::k_iCallback, on_signature, &call)) {
        return fail("await CheckFileSignature refused");
    }
    if (steam_platform_internal_await_call(handle, CheckFileSignature_t::k_iCallback, on_signature, &call)) {
        return fail("the same handle was awaited twice");
    }
    call_log cancelled{};
    const SteamAPICall_t other = SteamUtils()->CheckFileSignature(self_path);
    if (!steam_platform_internal_await_call(other, CheckFileSignature_t::k_iCallback, on_signature, &cancelled)
        || !steam_platform_internal_cancel_call(other) || steam_platform_internal_cancel_call(other)) {
        return fail("await then cancel of a second call did not behave");
    }

    const auto start = clock_type::now();
    pump_until(15.0, [&] { return relay.count > 0 && connection.count > 0 && call.count > 0 && seconds_since(start) >= 2.0; });
    close_pair(a, b);
    pump_until(0.2, [] { return false; });

    steam_platform_internal_counters_t after;
    steam_platform_internal_counters(&after);
    printf("frames ran %.1f s: callbacks %llu, unhandled %llu, call results %llu, events dropped %llu\n",
           seconds_since(start), static_cast<unsigned long long>(after.callbacks - before.callbacks),
           static_cast<unsigned long long>(after.unhandled - before.unhandled),
           static_cast<unsigned long long>(after.calls_delivered - before.calls_delivered),
           static_cast<unsigned long long>(after.events_dropped - before.events_dropped));
    printf("relay status callbacks: %d; last: avail %d, network config %d, any relay %d, ping measuring %d, \"%s\"\n",
           relay.count, static_cast<int>(relay.last.m_eAvail), static_cast<int>(relay.last.m_eAvailNetworkConfig),
           static_cast<int>(relay.last.m_eAvailAnyRelay), relay.last.m_bPingMeasurementInProgress, relay.last.m_debugMsg);
    printf("connection status callbacks: %d, last state %d\n", connection.count, connection.last_state);
    printf("call result: %d delivered, callback %d, %zu bytes, io_failed %d, signature result %d; cancelled call delivered %d\n",
           call.count, call.callback_id, call.size, call.io_failed ? 1 : 0, call.result, cancelled.count);
    printf("overlay ready: %d\n", steam_platform_overlay_ready() ? 1 : 0);

    if (relay.count == 0) {
        return fail("no SteamRelayNetworkStatus_t reached the listener");
    }
    if (connection.count == 0) {
        return fail("no SteamNetConnectionStatusChangedCallback_t reached the listener");
    }
    if (call.count != 1 || call.callback_id != CheckFileSignature_t::k_iCallback || cancelled.count != 0) {
        return fail("call results: the awaited one not exactly once, or the cancelled one delivered");
    }
    if (after.calls_delivered - before.calls_delivered != 1) {
        return fail("call results delivered more or less than once");
    }
    return 0;
}

int stop_from_inside() {
    s_stops_inside = 0;
    if (!steam_platform_internal_listen(SteamNetConnectionStatusChangedCallback_t::k_iCallback, stop_inside, nullptr)) {
        return fail("listen for the stopping handler refused");
    }
    HSteamNetConnection a = 0;
    HSteamNetConnection b = 0;
    socket_pair(&a, &b);
    pump_until(5.0, [] { return !steam_platform_ready(); });
    if (steam_platform_ready() || s_stops_inside != 1) {
        printf("FAIL: stop inside run_frame: ready %d, handler ran %d times\n", steam_platform_ready() ? 1 : 0, s_stops_inside);
        steam_platform_stop();
        return 1;
    }
    steam_platform_event_t event;
    if (steam_platform_user() != 0 || steam_platform_next_event(&event)) {
        return fail("module not inert after a stop inside run_frame");
    }
    printf("stop inside run_frame: handler ran once, module stopped\n");
    return 0;
}

int last_cycle(const char *self_path) {
    connection_log connection{};
    call_log call{};
    if (!steam_platform_internal_listen(SteamNetConnectionStatusChangedCallback_t::k_iCallback, on_connection_status,
                                        &connection)) {
        return fail("listen refused after a restart");
    }
    HSteamNetConnection a = 0;
    HSteamNetConnection b = 0;
    socket_pair(&a, &b);
    const SteamAPICall_t handle = SteamUtils()->CheckFileSignature(self_path);
    if (!steam_platform_internal_await_call(handle, CheckFileSignature_t::k_iCallback, on_signature, &call)) {
        return fail("await refused after a restart");
    }
    pump_until(10.0, [&] { return connection.count > 0 && call.count > 0; });
    close_pair(a, b);
    printf("after the restart: connection status callbacks %d, call results %d\n", connection.count, call.count);
    if (connection.count == 0 || call.count != 1) {
        return fail("listen or await did not deliver after a restart");
    }
    return 0;
}

struct moments {
    int frame;
    int stopping;
    bool steam_alive_at_stop;
};

void count_moment(void *user, steam_platform_moment_t moment) {
    auto *seen = static_cast<moments *>(user);
    if (moment == STEAM_PLATFORM_MOMENT_FRAME) {
        seen->frame++;
    } else {
        seen->stopping++;
        /* STOPPING comes while Steam can still be called. */
        seen->steam_alive_at_stop = SteamUtils() != nullptr && SteamUtils()->GetAppID() == SPACEWAR;
    }
}

void stop_in_frame(void *user, steam_platform_moment_t moment) {
    count_moment(user, moment);
    if (moment == STEAM_PLATFORM_MOMENT_FRAME) {
        steam_platform_stop();
    }
}

/* Hooks: refused twice and past the table, each moment once per hook, STOPPING with Steam alive. */
int hook_table() {
    moments seen[STEAM_PLATFORM_INTERNAL_MAX_HOOKS + 1] = {};
    if (steam_platform_internal_hook(nullptr, &seen[0])) {
        return fail("a NULL hook was taken");
    }
    /* The duplicate while the table still has room, so only the duplicate rule can refuse it. */
    if (!steam_platform_internal_hook(count_moment, &seen[0]) || steam_platform_internal_hook(count_moment, &seen[0])) {
        return fail("the first hook refused, or the same hook taken twice");
    }
    for (int i = 1; i < STEAM_PLATFORM_INTERNAL_MAX_HOOKS; i++) {
        if (!steam_platform_internal_hook(count_moment, &seen[i])) {
            return fail("a hook was refused with room in the table");
        }
    }
    if (steam_platform_internal_hook(count_moment, &seen[STEAM_PLATFORM_INTERNAL_MAX_HOOKS])) {
        return fail("a hook was taken past a full table");
    }
    frame();
    frame();
    steam_platform_stop();
    for (int i = 0; i < STEAM_PLATFORM_INTERNAL_MAX_HOOKS; i++) {
        if (seen[i].frame != 2 || seen[i].stopping != 1 || !seen[i].steam_alive_at_stop) {
            printf("FAIL: hook %d heard %d frames and %d stops (Steam alive at stop %d)\n", i, seen[i].frame, seen[i].stopping, seen[i].steam_alive_at_stop ? 1 : 0);
            return 1;
        }
    }
    if (seen[STEAM_PLATFORM_INTERNAL_MAX_HOOKS].frame != 0 || seen[STEAM_PLATFORM_INTERNAL_MAX_HOOKS].stopping != 0) {
        return fail("the refused hook was called");
    }
    printf("hooks: NULL, duplicate and fifth refused; each heard 2 frames and 1 stop, Steam alive\n");
    return 0;
}

/* A FRAME hook that stops the module: the hooks after it hear STOPPING, never that FRAME. */
int hook_stops_inside() {
    moments first = {}, second = {};
    if (!steam_platform_internal_hook(stop_in_frame, &first) || !steam_platform_internal_hook(count_moment, &second)) {
        return fail("hooks refused after a start");
    }
    frame();
    if (steam_platform_ready() || first.frame != 1 || first.stopping != 1 || second.frame != 0 || second.stopping != 1) {
        printf("FAIL: stop inside a FRAME hook: ready %d, first %d/%d, second %d/%d\n", steam_platform_ready() ? 1 : 0, first.frame, first.stopping, second.frame, second.stopping);
        return 1;
    }
    frame();
    if (second.frame != 0) {
        return fail("a hook ran after the stop");
    }
    printf("stop inside a FRAME hook: later hooks heard only STOPPING, module stopped\n");
    return 0;
}

} // namespace

int main(int argc, char **argv) {
    const bool simulate = argc > 1 && strcmp(argv[1], "--simulate-no-client") == 0;
    if (simulate) {
        steam_platform_internal_simulate_failure(false, STEAM_PLATFORM_INTERNAL_INIT_NO_STEAM_CLIENT,
                                                 "simulated: Steam is not running");
    }

    /* steam_appid.txt names 480, so a build that says it is 481 must be refused. */
    const steam_platform_config_t wrong = {SPACEWAR + 1, false};
    char message[256];
    const steam_platform_status_t refused = steam_platform_start(&wrong, message, sizeof message);
    if (refused == STEAM_PLATFORM_NO_CLIENT) {
        return steam_live::no_client(message);
    }
    if (refused != STEAM_PLATFORM_FAILED || steam_platform_ready()) {
        printf("FAIL: start as app 481 answered %d: %s\n", static_cast<int>(refused), message);
        return 1;
    }
    printf("start as 481: FAILED (%s)\n", message);

    const steam_platform_config_t config = {SPACEWAR, false};
    int result = start_ready(config, "start 1");
    if (result == 0) {
        result = start_rules(config);
    }
    if (result == 0) {
        result = first_cycle(argv[0]);
    }
    if (result != 0) {
        return result;
    }
    steam_platform_stop();
    steam_platform_event_t event;
    if (steam_platform_ready() || steam_platform_user() != 0 || steam_platform_next_event(&event)) {
        return fail("module not inert after stop");
    }
    steam_platform_stop();

    if ((result = start_ready(config, "start 2")) != 0 || (result = stop_from_inside()) != 0) {
        return result;
    }
    if ((result = start_ready(config, "start 3")) != 0 || (result = last_cycle(argv[0])) != 0) {
        return result;
    }
    steam_platform_stop();
    if ((result = start_ready(config, "start 4")) != 0 || (result = hook_table()) != 0) {
        return result;
    }
    if ((result = start_ready(config, "start 5")) != 0 || (result = hook_stops_inside()) != 0) {
        return result;
    }
    if (steam_platform_ready()) {
        return fail("module ready after the last stop");
    }
    printf("PASS: refused app, start rules, callbacks, call results, stop inside run_frame, restart, hooks\n");
    return 0;
}
