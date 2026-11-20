#ifndef FEATURES_STEAM_PLATFORM_STEAM_PLATFORM_H
#define FEATURES_STEAM_PLATFORM_STEAM_PLATFORM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* The process's connection to Steam: start, one call a frame, events the game pulls, stop.

   Steam's API is one per process, so this module is one per process too and takes no handle.
   All calls come from the thread that called steam_platform_start. Nothing calls back.

   The game never sees Steam's headers: they are C++, and the one file that includes them
   lives in this feature. A build without the Steamworks SDK gets this same header and a
   module that answers NO_SDK, so the game compiles, links and plays solo either way; so does
   a build with the SDK on a machine where Steam is not running. Neither case shows a dialog.

   A build with the SDK needs Steam's runtime library beside the executable (steam_api64.dll
   on 64-bit Windows, libsteam_api.so, libsteam_api.dylib). On Windows it is loaded at start,
   so without it start answers FAILED and names the file; elsewhere the system loader needs it
   before the program runs.

   A NULL pointer or an unknown dialog name from the caller never crashes: each call says what
   it does with one. */

#ifdef __cplusplus
extern "C" {
#endif

typedef enum steam_platform_status {
    STEAM_PLATFORM_READY = 0,
    STEAM_PLATFORM_NO_SDK,     /* this build has no Steamworks SDK */
    /* Steam is not running or cannot be reached. An account that may not run the app is
       FAILED: Steam reports it only as text, in the message. */
    STEAM_PLATFORM_NO_CLIENT,
    /* The game was not started by Steam and Steam is starting it again: this process must
       exit now, without opening a window. */
    STEAM_PLATFORM_RESTARTING,
    STEAM_PLATFORM_FAILED,     /* anything else; the message says what */
} steam_platform_status_t;

typedef struct steam_platform_config {
    /* The app this build belongs to. When not zero, a start into another app (a stale
       steam_appid.txt) is FAILED; zero accepts the app Steam started. */
    uint32_t app_id;
    /* A release build started outside Steam asks Steam to start it properly and reports
       RESTARTING. Steam documents that this also launches a Steam client that is not
       running, so such a build does not fall back to solo there. It needs a non-zero app_id.
       A development build leaves this off and names its app in steam_appid.txt beside the
       executable. */
    bool restart_through_steam;
} steam_platform_config_t;

/* Checked in this order:
   1. A NULL config, or restart_through_steam with app_id 0, is FAILED in every build, also
      while ready; a ready module stays ready.
   2. A valid config while ready answers READY and changes nothing.
   3. From a stopped module, anything but READY leaves it stopped, and every other call is
      then safe and inert.
   `message`, when not NULL and `message_size` is not zero, gets a short English reason for
   logs, cut to fit and always terminated. */
steam_platform_status_t steam_platform_start(const steam_platform_config_t *config, char *message, size_t message_size);

/* Safe to call when not started. Drops the events nobody pulled. */
void steam_platform_stop(void);

bool steam_platform_ready(void);

/* Once a frame: lets Steam do its work and collects what it has to say. */
void steam_platform_run_frame(void);

typedef enum steam_platform_event_type {
    /* Steam's overlay opened over the game or closed: pause what must not run under it. */
    STEAM_PLATFORM_EVENT_OVERLAY = 1,
} steam_platform_event_type_t;

typedef struct steam_platform_event {
    steam_platform_event_type_t type;
    bool active; /* OVERLAY: shown */
} steam_platform_event_t;

/* How many events wait at most between two pulls. */
#define STEAM_PLATFORM_EVENT_CAPACITY 16

/* The next event collected by steam_platform_run_frame, or false when none waits. Events
   that were not pulled before the next run_frame stay queued; when the queue is full the
   oldest are dropped. A NULL `out` answers false and takes nothing. */
bool steam_platform_next_event(steam_platform_event_t *out);

/* The signed-in account as a 64-bit SteamID; zero when not ready. */
uint64_t steam_platform_user(void);

/* True once Steam has hooked the game's window and can draw over it. It needs a few
   presented frames after start, and it stays false when the overlay is turned off in Steam. */
bool steam_platform_overlay_ready(void);

/* Asks Steam to open its overlay on `dialog`: "friends", "community", "settings" and the
   others Steam documents for ActivateGameOverlay; NULL opens "friends", any other name is
   passed to Steam unchanged. The answer is an OVERLAY event. While overlay_ready is false
   Steam may open nothing, and then no event comes. */
void steam_platform_open_overlay(const char *dialog);

#ifdef __cplusplus
}
#endif

#endif
