#ifndef FEATURES_TELEMETRY_TELEMETRY_H
#define FEATURES_TELEMETRY_TELEMETRY_H

#include <stdbool.h>
#include <stdint.h>

/* The studio's own funnel, portal-independent. A game names the moments a
 * playtest is judged on -- a level begun, won or lost, a merge, a purchase, a
 * heartbeat while playing -- and this module batches them into small JSON
 * bodies and POSTs them to the studio collector (ai_studio/telemetry). It is
 * dormant when the collector URL is empty, so a build without a collector
 * plays exactly like one with it. Nothing here is a save; the one persistent
 * id (the player) is the game's to store. No PII: ids are random hex. */

typedef struct telemetry_config_t {
    const char *url;      /* collector function URL; "" or NULL -> dormant */
    const char *key;      /* api key the collector checks */
    const char *game;     /* game id, e.g. "example-game" */
    const char *build;    /* build id string the report groups by */
    const char *platform; /* publish target name: poki, yandex, playgama, local, itch */
    const char *player;   /* 32 hex chars, persisted by the game (telemetry_make_id) */
    float flush_interval_s; /* 0 -> 15 */
    float heartbeat_s;      /* 0 -> 30 */
} telemetry_config_t;

/* One POST at a time. `send` returns an opaque handle or NULL when it could
 * not even start; `poll` answers 0 while pending, 1 on a 2xx answer, -1 on
 * anything else; `release` frees the handle. The default is the engine's
 * nt_http; tests inject their own. */
typedef struct telemetry_transport_t {
    void *(*send)(const char *url, const char *body, uint32_t length, void *userdata);
    int (*poll)(void *handle, void *userdata);
    void (*release)(void *handle, void *userdata);
    void *userdata;
} telemetry_transport_t;

/* A NULL transport or an empty URL leaves the module dormant. */
void telemetry_init(const telemetry_config_t *config, const telemetry_transport_t *transport);
void telemetry_shutdown(void);
bool telemetry_enabled(void);

/* Once a frame. `playing` is the game's own word for "the player is in play":
 * the heartbeat and the session's play seconds advance only then. */
void telemetry_update(float dt, bool playing);
/* Send what is buffered now if nothing is in flight: the page is hiding, or
 * the game is closing. */
void telemetry_flush(void);

/* An event is opened by name, given flat fields, and closed. Fields between
 * begin and end only; a begin without an end is closed by the next begin. */
void telemetry_event_begin(const char *name);
void telemetry_int(const char *key, long long value);
void telemetry_float(const char *key, double value);
void telemetry_str(const char *key, const char *value);
void telemetry_event_end(void);

/* Events that did not fit or were given up after retries. Health only. */
uint64_t telemetry_dropped(void);
/* The session id of this launch, 32 hex chars; "" while dormant. */
const char *telemetry_session_id(void);
/* A fresh 32-hex random id for the game to persist as the player id. */
void telemetry_make_id(char out[33]);

/* The nt_http transport (telemetry_http.c), for the game's init. Native builds
 * without curl get the engine's stub, which fails every request -- the module
 * then drops and stays quiet. */
const telemetry_transport_t *telemetry_http_transport(void);

#endif /* FEATURES_TELEMETRY_TELEMETRY_H */
