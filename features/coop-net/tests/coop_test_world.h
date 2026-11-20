#ifndef COOP_TEST_WORLD_H
#define COOP_TEST_WORLD_H

#include "features/coop_net/coop_session.h"
#include "features/net_link/net_link_sim.h"

/* A host and guests on one simulated network, stepped in virtual time. Each step advances the
   network, then every live side sets its time, polls everything into its log (and the
   handler), runs the tick hook and ends its tick. */

enum {
    COOP_WORLD_GUESTS = 8,
    COOP_WORLD_LOG = 512,
    COOP_WORLD_DATA = 64,
    COOP_WORLD_HOST = -1, /* side of the host; guests are 0.. */
    COOP_WORLD_HOST_IDENTITY = 100,
};

typedef struct coop_logged {
    coop_event_t event;
    uint8_t data[COOP_WORLD_DATA]; /* the first bytes of event.data */
} coop_logged_t;

typedef struct coop_log {
    coop_logged_t items[COOP_WORLD_LOG];
    uint32_t count; /* keeps counting past the array; items past it are not kept */
} coop_log_t;

typedef struct coop_world coop_world_t;
typedef void (*coop_world_event_fn)(void *context, coop_world_t *world, int side, const coop_event_t *event);
typedef void (*coop_world_tick_fn)(void *context, coop_world_t *world, int side);

struct coop_world {
    net_link_sim_t *sim;
    net_link_t *host_link;
    coop_session_t *host;
    net_link_t *guest_link[COOP_WORLD_GUESTS];
    coop_session_t *guest[COOP_WORLD_GUESTS];
    coop_log_t host_log;
    coop_log_t guest_log[COOP_WORLD_GUESTS];
    uint32_t max_guests;
    uint32_t frozen; /* bit per guest that is not stepped: it neither reads nor sends */
    double dt;
    coop_world_event_fn on_event;
    coop_world_tick_fn on_tick;
    void *context;
};

typedef struct coop_side_config {
    uint64_t identity;
    uint64_t player_key;
    uint64_t build_key;
    const void *world_key;
    uint32_t world_key_size;
    uint32_t baseline_max;
    uint32_t seats;
    float handshake_timeout_s;
    uint32_t tick_hz; /* zero: 60 */
} coop_side_config_t;

/* `config` may be NULL for defaults; its lanes are always the session's. */
void coop_world_open(coop_world_t *world, uint64_t seed, uint32_t max_guests, const net_link_sim_config_t *config);
void coop_world_close(coop_world_t *world);
coop_session_t *coop_world_host(coop_world_t *world, const coop_side_config_t *side);
coop_session_t *coop_world_join(coop_world_t *world, int guest, const coop_side_config_t *side);
/* Destroys a guest's session and its link. */
void coop_world_drop(coop_world_t *world, int guest);
void coop_world_step(coop_world_t *world);
void coop_world_run(coop_world_t *world, double seconds);
coop_log_t *coop_world_log(coop_world_t *world, int side);
void coop_world_conditions(coop_world_t *world, int guest, const net_link_conditions_t *to_host, const net_link_conditions_t *to_guest);

uint32_t coop_log_count(const coop_log_t *log, coop_event_type_t type);
/* The first logged event of `type` (and `seat`, unless COOP_SEAT_NONE), or NULL. */
const coop_logged_t *coop_log_find(const coop_log_t *log, coop_event_type_t type, uint8_t seat);
void coop_log_clear(coop_log_t *log);

/* A link with no session on it, driven by hand: a peer that speaks the wire itself. */
typedef struct raw_end {
    net_link_t *link;
    net_link_peer_t peer;
    bool connected;
    bool disconnected;
    uint32_t reason;
    uint32_t messages;
    uint8_t first[512]; /* the first message since `messages` was zero */
    uint32_t first_size;
    uint8_t first_lane;
    uint8_t last[512];
    uint32_t last_size;
    uint8_t last_lane;
} raw_end_t;

/* Polls everything, keeps the last message, then flushes. */
void raw_poll(raw_end_t *raw);

/* A deterministic generator for tests. */
uint32_t coop_test_rand(uint64_t *state);

#endif
