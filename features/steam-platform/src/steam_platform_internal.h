#ifndef FEATURES_STEAM_PLATFORM_INTERNAL_H
#define FEATURES_STEAM_PLATFORM_INTERNAL_H

#include "features/steam_platform/steam_platform.h"

/* For the other translation units of this feature and for its tests; a game never includes it.

   Steam has one callback queue per process and steam_platform_run_frame empties it, so code
   that needs Steam callbacks the public events do not carry hears them here, by Steam's
   callback id, while run_frame dispatches. A handler runs on the game's thread inside
   run_frame; `data` is Steam's struct and lives until the handler returns. A handler may stop
   listening or stop the module; stopping takes effect after the current callback.
   steam_platform_stop forgets every listener and pending call.

   The networking library's callbacks (SteamNetConnectionStatusChangedCallback_t,
   SteamRelayNetworkStatus_t) reach this queue like any other once its interface exists, and
   are heard by id here. Its function-pointer callbacks (k_ESteamNetworkingConfig_Callback_*,
   fired by ISteamNetworkingSockets::RunCallbacks) are not used and must not be mixed in:
   Steam then reports every change twice, once through each path. */

#ifdef __cplusplus
extern "C" {
#endif

#define STEAM_PLATFORM_INTERNAL_MAX_LISTENERS 16
#define STEAM_PLATFORM_INTERNAL_MAX_CALLS 16
/* The largest call result delivered; a bigger one arrives as failed with no data. */
#define STEAM_PLATFORM_INTERNAL_CALL_RESULT_BYTES 4096

typedef void (*steam_platform_callback_fn)(void *user, int callback_id, const void *data, size_t size);

/* False when not ready, `fn` is NULL, the same (id, fn, user) already listens, or the table
   is full. */
bool steam_platform_internal_listen(int callback_id, steam_platform_callback_fn fn, void *user);
void steam_platform_internal_unlisten(int callback_id, steam_platform_callback_fn fn, void *user);

/* `io_failed` is Steam's own failure flag for the call; on it `data` may be NULL. */
typedef void (*steam_platform_call_result_fn)(void *user, int callback_id, const void *data, size_t size, bool io_failed);

/* Delivers the result of an asynchronous Steam call (a SteamAPICall_t) once, during a later
   run_frame. Await right after issuing the call: a result that completes in a run_frame
   before the await is registered is freed as unhandled. False when not ready, the handle is
   zero or already awaited, `fn` is NULL or the table is full. */
bool steam_platform_internal_await_call(uint64_t api_call, int expected_callback_id, steam_platform_call_result_fn fn, void *user);

/* Forgets an awaited call, for an owner that goes away first; its result is then unhandled.
   False when the handle is not awaited. */
bool steam_platform_internal_cancel_call(uint64_t api_call);

typedef struct steam_platform_internal_counters {
    uint64_t callbacks;    /* every callback taken from Steam or fed */
    uint64_t unhandled;    /* freed with nobody to read it: no event, listener or pending call */
    uint64_t calls_delivered;
    uint64_t events_dropped; /* overwritten in a full queue */
} steam_platform_internal_counters_t;

/* Counted since the module was loaded; never reset, so a test reads differences. */
void steam_platform_internal_counters(steam_platform_internal_counters_t *out);

/* Steam's ESteamAPIInitResult values; the shim checks they match. */
enum {
    STEAM_PLATFORM_INTERNAL_INIT_FAILED_GENERIC = 1,
    STEAM_PLATFORM_INTERNAL_INIT_NO_STEAM_CLIENT = 2,
    STEAM_PLATFORM_INTERNAL_INIT_VERSION_MISMATCH = 3,
};

/* Test hook: the next steam_platform_start does not touch Steam and acts as if the restart
   check answered `restart` and init then failed with `init_result` and `message`. It lets a
   test reach every failure path while the developer's Steam client keeps running. Inert in a
   build without the SDK, which always answers NO_SDK. */
void steam_platform_internal_simulate_failure(bool restart, int init_result, const char *message);

/* Test hook: dispatches one raw callback as run_frame would, also while stopped (listeners
   exist only while ready). A call result cannot be fed. False when refused; always false in a
   build without the SDK. */
bool steam_platform_internal_feed(int callback_id, const void *data, size_t size);

#ifdef __cplusplus
}
#endif

#endif
