#ifndef FEATURES_COOP_NET_COOP_SESSION_INTERNAL_H
#define FEATURES_COOP_NET_COOP_SESSION_INTERNAL_H

#include "features/coop_net/coop_session.h"

#include "coop_acks.h"
#include "coop_clock.h"
#include "coop_pending.h"
#include "coop_wire.h"

enum {
    COOP_LINK_BATCH = 32,
    /* The queue reads a link event only when it is empty (coop_session_poll). One link event
       raises at most COOP_SEATS_MAX events: a takeover is SEAT_LEFT and SEAT_JOINED of its
       seat, and its notices can close each of the six other guests with STALLED once. Poll
       hands one out before the game can act, and the game's calls (kick, a guest's handshake
       timeout) raise at most one SEAT_LEFT per guest still seated; seats refill only from link
       events. A takeover with k stalls leaves k + 1 queued and 7 - k seated: COOP_SEATS_MAX at
       most. A guest holds at most WELCOME, a held BASELINE and CLOSED. Twice that, for room. */
    COOP_EVENT_QUEUE = 2 * COOP_SEATS_MAX,
    /* Baseline bytes a host keeps queued on the link per guest, so a baseline never takes the
       queue room records need. */
    COOP_BASELINE_IN_FLIGHT = 65536,
};
#define COOP_HANDSHAKE_TIMEOUT_DEFAULT_S 5.0
/* A side that went longer than this between two set_time calls measured its own stall:
   round-trip samples taken right after are skipped. A slow tick rate widens it to 3 ticks. */
#define COOP_RTT_GAP_S 0.05

/* One path, host to guest. On the host seat[i] is guest i; on a guest seat[0] is the host and
   the other seats hold only who sits there. */
typedef struct coop_seat {
    bool seated;
    net_link_peer_t peer;
    uint64_t identity;
    uint64_t player_key;
    coop_send_window_t send;
    coop_recv_window_t recv;
    coop_rtt_t rtt;
    coop_clock_t clock;
    bool state_this_tick; /* the game sent a state packet on this path this tick */
    uint32_t idle;        /* ticks since this side last sent a state packet on this path */
    uint32_t state_sent;
    uint32_t state_received;
    uint32_t state_stale;
    /* Host: what this guest accepts, the records sent to it, the baseline cut for it. */
    uint32_t accept_baseline;
    uint32_t records_sent;
    uint8_t *baseline;
    bool baseline_busy;
    uint32_t baseline_size;
    uint32_t baseline_sent;
    uint32_t baseline_tick;
    uint32_t baseline_records_before;
} coop_seat_t;

typedef enum coop_guest_phase {
    COOP_GUEST_CONNECTING = 0,
    COOP_GUEST_GREETING, /* hello sent, no welcome yet */
    COOP_GUEST_SEATED,
    COOP_GUEST_CLOSED,
} coop_guest_phase_t;

struct coop_session {
    net_link_t *link;
    bool host;
    uint8_t seats;
    uint64_t build_key;
    uint64_t player_key;
    uint8_t world_key[COOP_WORLD_KEY_MAX];
    uint32_t world_key_size;
    uint32_t baseline_max;
    double handshake_timeout_s;
    uint32_t tick_hz;
    uint32_t tick;
    double now_s;
    bool drained;        /* a poll has emptied the queue */
    double drained_s;    /* when the last poll did */
    bool rtt_samples_ok; /* false right after a long gap since that poll */
    uint32_t idle_ticks; /* an empty state packet goes on a path idle this many ticks */
    bool open;
    uint8_t *scratch;
    uint32_t scratch_size;
    uint32_t reliable_max; /* the link's reliable limit, capped at COOP_RELIABLE_CAP */

    coop_seat_t seat[COOP_SEATS_MAX];
    coop_pending_t pending[COOP_PENDING_MAX];

    /* Guest */
    coop_guest_phase_t phase;
    uint8_t own_seat;
    uint32_t roster; /* bit per seat the host announced, its own included */
    net_link_peer_t host_peer;
    double greet_s;         /* when the hello left, and when the handshake timer started */
    double handshake_rtt_s; /* hello to welcome; negative until then */
    uint8_t *baseline_in;
    bool baseline_open;     /* a first piece came and the last has not */
    bool baseline_held;     /* whole, waiting for the welcome */
    uint32_t baseline_total;
    uint32_t baseline_got;
    uint32_t baseline_tick;
    uint32_t baseline_records_before;

    /* Events for the game, in order; their data points into `batch` or `baseline_in`. */
    coop_event_t queue[COOP_EVENT_QUEUE];
    uint32_t queue_head;
    uint32_t queue_count;
    net_link_event_t batch[COOP_LINK_BATCH];
    uint32_t batch_count;
    uint32_t batch_next;
};

void coop_session_push(coop_session_t *session, const coop_event_t *event);
/* Sends one session message; false when the link did not take it. */
bool coop_session_send_wire(coop_session_t *session, net_link_peer_t peer, const coop_wire_msg_t *msg);
void coop_seat_reset(coop_seat_t *seat, uint32_t tick_hz);
/* One state packet on a path: its acknowledgements and round-trip sample, the sender's tick
   for the clock, then the packet for the game unless it is empty, came before or is too old
   to place. */
void coop_seat_take_state(coop_session_t *session, coop_seat_t *seat, const coop_wire_msg_t *msg, uint8_t seat_index);

void coop_host_handle(coop_session_t *session, const net_link_event_t *event);
void coop_host_set_time(coop_session_t *session);
void coop_host_end_tick(coop_session_t *session);
void coop_host_leave(coop_session_t *session, uint8_t seat, uint32_t reason, uint32_t told, bool close_link);
void coop_host_destroy(coop_session_t *session);

void coop_guest_handle(coop_session_t *session, const net_link_event_t *event);
void coop_guest_set_time(coop_session_t *session);
void coop_guest_destroy(coop_session_t *session);

#endif
