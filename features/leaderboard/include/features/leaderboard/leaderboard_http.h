#ifndef FEATURES_LEADERBOARD_HTTP_H
#define FEATURES_LEADERBOARD_HTTP_H

#include "features/leaderboard/leaderboard.h"

#include <stdbool.h>
#include <stdint.h>

/* Anonymous HTTP backend for portals without a board of their own: all-time
 * plus UTC-day, keyed by the facade's player id, with the place re-estimated
 * locally from the server's top and its bucket histogram.
 *
 * Wire shape, one GET per board: the request body is JSON obfuscated with the
 * xor key and base64url'd into `?d=`; the server answers the board's top for
 * both scopes. Every request is at once the submit and the poll: the server
 * overwrites the player's row with what the body carries, so each body holds
 * the facade's current value for every declared scope, never only the one
 * being submitted.
 *
 * The board's `portal_id` (its `id` when none) is the metric's name on the
 * wire: the all-time field, `<name>_day` for the day scope and the root
 * object of the response. The rest of the row travels through the `extra`
 * codec: each `k=v;` pair becomes a body field, a value that is a plain
 * integer as a JSON number, anything else as a string; every server row
 * field beyond `user_id` and `value` comes back packed the same way. */

typedef enum {
    LEADERBOARD_HTTP_PENDING = 0,
    LEADERBOARD_HTTP_DONE,   /* a body arrived; whether it parses is decided here */
    LEADERBOARD_HTTP_FAILED, /* transport error, no usable body */
} leaderboard_http_state_t;

/* I/O and clocks behind the backend. NULL in the config selects the engine
 * (`nt_http`, `nt_time`, `time`); tests hand in a canned one. */
typedef struct {
    uint32_t (*request)(const char *url, void *userdata); /* 0 when it could not start */
    leaderboard_http_state_t (*state)(uint32_t request, void *userdata);
    /* The body, malloc'd for the caller; NULL when empty or not done. */
    uint8_t *(*take_body)(uint32_t request, uint32_t *size, void *userdata);
    void (*release)(uint32_t request, void *userdata);
    double (*monotonic_now)(void *userdata); /* seconds; drives the poll cadence */
    int64_t (*unix_now)(void *userdata);     /* anchors the UTC day */
    void *userdata;
} leaderboard_http_transport_t;

typedef struct {
    /* Endpoint of the board function. Empty or NULL keeps the backend dormant:
     * every board answers no capabilities and no request is made. */
    const char *url;
    /* Obfuscation only, mirrored by the server; the server-side clamps are the
     * real guard. Empty is dormant like an empty url. */
    const char *key;
    /* Poll cadence. A healthy client re-asks every `repeat_delay_s`; a failed
     * request retries after `error_delay_s` for `error_fast_tries` attempts,
     * then falls back to `repeat_delay_s`. All three are required. */
    double repeat_delay_s;
    double error_delay_s;
    int error_fast_tries;
    const leaderboard_http_transport_t *transport; /* NULL: the engine */
} leaderboard_http_config_t;

/* The backend userdata. The game fills `config` before `leaderboard_init`;
 * `state` belongs to the backend from init to destroy. */
typedef struct {
    leaderboard_http_config_t config;
    void *state;
} leaderboard_http_t;

const leaderboard_backend_t *leaderboard_http_backend(void);

#endif /* FEATURES_LEADERBOARD_HTTP_H */
