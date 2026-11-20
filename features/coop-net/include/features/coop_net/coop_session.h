#ifndef FEATURES_COOP_NET_COOP_SESSION_H
#define FEATURES_COOP_NET_COOP_SESSION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "features/net_link/net_link.h"

/* A co-op session: who sits where, whether two builds may play together, and three kinds of
   traffic between the host and each guest. It knows no world and no physics: every payload is
   the game's bytes. It rides any net_link, so the transport is the game's choice.

   The host is seat 0 and every guest talks to the host only; what one guest must know about
   another, the host forwards.

   The game owns the loop and calls, each tick: coop_session_set_time, coop_session_poll until
   it returns false, its own step, the sends, coop_session_end_tick. Nothing calls back and
   nothing is allocated after the session is created.

   Bytes from a peer are untrusted. A packet the session cannot parse, or one that breaks its
   rules, closes that peer with COOP_CLOSE_FORMAT; it never asserts and never reads outside
   the packet. */

typedef struct coop_session coop_session_t;

enum {
    COOP_SESSION_PROTOCOL = 1, /* of the session's own packets; another number is refused with VERSION */
    COOP_SEATS_MAX = 8,
    COOP_SEAT_HOST = 0,
    COOP_SEAT_NONE = 0xFF,
    COOP_WORLD_KEY_MAX = 256, /* bytes */
    /* State packets per seat whose fate the session keeps: about 4 s at 60 a second, so a
       slow path still sees its acknowledgements before a packet ages out. */
    COOP_FATE_WINDOW = 256,
    /* The largest reliable message the session sends, whatever the link allows: a record and
       a baseline piece never take more of a queue than this. */
    COOP_RELIABLE_CAP = 16384,
    /* The longest a path may go without a state packet from this side; see
       coop_session_set_idle_ticks. */
    COOP_IDLE_TICKS_MAX = 16,
};

/* The lanes a session needs from its link, in this order: open the link with these. State is
   lane 0, the cheapest lane on Steam, since it is the common traffic. When the path is short
   of room records go first, then state, then baselines. */
enum {
    COOP_LANE_STATE = 0,    /* unreliable: state packets */
    COOP_LANE_RECORDS = 1,  /* reliable: handshake, notices, records */
    COOP_LANE_BASELINE = 2, /* reliable, behind the others */
    COOP_LANE_COUNT = 3,
};
net_link_lanes_t coop_session_lanes(void);

/* Why a seat or the whole session ended. The values are app codes of net_link, so the other
   side reads the same number. A game closes with codes of its own from COOP_CLOSE_GAME_FIRST. */
enum {
    COOP_CLOSE_LEFT = 1001,      /* the player left */
    COOP_CLOSE_HOST_LEFT = 1002, /* the host ended the session */
    COOP_CLOSE_VERSION = 1003,   /* the two builds speak different session protocols */
    COOP_CLOSE_BUILD = 1004,     /* the build keys differ: other content or other record tables */
    COOP_CLOSE_WORLD = 1005,     /* the guest holds another world than the host runs */
    COOP_CLOSE_FULL = 1006,      /* every seat is taken */
    COOP_CLOSE_SHUT = 1007,      /* the host takes no new players now */
    COOP_CLOSE_FORMAT = 1008,    /* a packet broke the protocol */
    COOP_CLOSE_HANDSHAKE = 1009, /* connected, but no valid hello in time, or a newer connection took its place */
    COOP_CLOSE_KICKED = 1010,
    COOP_CLOSE_REPLACED = 1011, /* the same player connected again and took the seat over */
    COOP_CLOSE_STALLED = 1012,  /* the peer stopped taking reliable messages: a session message did not fit */
    COOP_CLOSE_GAME_FIRST = 1500,
    COOP_CLOSE_GAME_LAST = 1999,
};
/* A short English name for logs; "unknown" for a code it does not know. Link reasons
   (timeout, unreachable, ...) are named too. */
const char *coop_close_name(uint32_t reason);

/* Two builds that play together must step at the same tick_hz and read each other's record
   tables and baselines: fold all of that into build_key. `seats` is the host's alone, and each
   side's baseline_max is respected by the other. */
typedef struct coop_session_config {
    /* The link this session uses. It must have the lanes of coop_session_lanes. The session
       never destroys it: destroy the session first, then the link. */
    net_link_t *link;
    /* Seats of the session, the host's included: 1..COOP_SEATS_MAX. Host only. */
    uint32_t seats;
    /* One number for everything two builds must share to play together: content, the tables
       of every record the game sends, the tick rate. A guest with another key is refused with
       BUILD. */
    uint64_t build_key;
    /* What tells this player's game from another instance with the same link identity (two
       windows of one account). The host reports it with the seat. Guest only. */
    uint64_t player_key;
    /* The world this side holds, as the game's own bytes; the host's is sent to a guest with
       its welcome. A guest that passes none takes the host's; a guest that passes one is
       refused with WORLD when it is not byte-equal to the host's. */
    const void *world_key;
    uint32_t world_key_size;
    /* The largest baseline the game will send (host) or accept (guest), in bytes. A guest's
       value travels in its hello; the host refuses a larger baseline for it with TOO_LARGE.
       Zero: no baselines. */
    uint32_t baseline_max;
    /* A connected peer that has not sent a valid hello after this long is closed with
       HANDSHAKE; a guest not welcomed in this time closes with HANDSHAKE. Zero: 5 seconds. */
    float handshake_timeout_s;
    /* Ticks per second of the game's fixed step, 1..65535; it reaches every guest with the
       welcome. Host only. */
    uint32_t tick_hz;
    /* This side's tick when the session starts, so that it continues the game's own count. */
    uint32_t tick;
} coop_session_config_t;

/* NULL when the config is out of range, the link lacks the lanes, or memory is short. */
coop_session_t *coop_session_host(const coop_session_config_t *config);

/* The guest's link must already be connecting to the host; the session greets the host when
   the link reports the connection. */
coop_session_t *coop_session_join(const coop_session_config_t *config);

/* A host closes every guest with HOST_LEFT, a guest leaves with LEFT; both let the close
   reach the other side. The link stays the caller's. */
void coop_session_destroy(coop_session_t *session);

bool coop_session_is_host(const coop_session_t *session);
/* This side's seat: COOP_SEAT_HOST on the host, COOP_SEAT_NONE on a guest not yet welcomed. */
uint8_t coop_session_seat(const coop_session_t *session);
/* The host's seats and tick rate; on a guest, 0 before the welcome. */
uint32_t coop_session_seats(const coop_session_t *session);
uint32_t coop_session_tick_hz(const coop_session_t *session);

/* The host's world changed (a run began, the players went back to the base): guests welcomed
   from now on receive this key. Host only. */
bool coop_session_set_world_key(coop_session_t *session, const void *world_key, uint32_t size);

/* True when a player sits in `seat`; the host's seat is always taken. On a guest it follows
   the SEAT_JOINED and SEAT_LEFT events polled so far. */
bool coop_session_seated(const coop_session_t *session, uint8_t seat);

/* A shut host refuses new players with SHUT; players already seated stay. Host only. */
void coop_session_set_open(coop_session_t *session, bool open);

/* Closes one guest with `reason`: a COOP_CLOSE code or a game code. Host only. The host gets
   SEAT_LEFT for it on a later poll. */
void coop_session_kick(coop_session_t *session, uint8_t seat, uint32_t reason);

/* ---- time ---- */

/* The game's clock, in seconds; any origin, never going back: an earlier time is ignored.
   Call before polling. */
void coop_session_set_time(coop_session_t *session, double now_s);

/* This side's own tick: it starts at config.tick, steps once per coop_session_end_tick, is
   never adjusted, and stamps everything sent. */
uint32_t coop_session_tick(const coop_session_t *session);

/* Sends what the tick produced and steps the tick by one. A seated path that got no state
   packet from this side for the idle ticks gets an empty one (a numbered header, no payload),
   so acknowledgements flow whatever the game sends; the other side delivers no event for it.
   Then the link's flush. */
void coop_session_end_tick(coop_session_t *session);

/* How many ticks a path may go without a state packet before end_tick sends an empty one:
   1 (the default) is every tick; a game on a weak channel that sends every other tick sets 2
   and the session adds nothing. Clamped to 1..COOP_IDLE_TICKS_MAX: acknowledgements must come
   often enough that the peer's packets since the last one, even two a tick, stay within the
   33 one acknowledgement names. The longer the wait, the fewer acknowledgements name each
   packet: at 16 and 5% loss about one delivered packet in 200 reads LOST. */
void coop_session_set_idle_ticks(coop_session_t *session, uint32_t ticks);

/* The tick of `seat` now, as this side sees it: the tick of its newest state packet that has
   had time to arrive, carried forward at tick_hz, fractions included. It follows the least-
   delayed state packets of about the last second and adds no round-trip term, so it trails
   the sender by the path's shortest one-way delay. Unwrapped: past 2^32 it keeps counting.
   Negative while no state packet of that seat has arrived; a hostile stamp never makes it
   negative once known. Measured limits: a steady path keeps it within about a tick of the
   sender's tick minus that delay, more with jitter, and it trails by up to one more local
   frame (a packet waits for this side's poll); a sender stepping at another rate than tick_hz
   drifts by up to one second times the difference; a faster path lands at once as a jump
   forward, a slower one shows only after up to one window (about a second) and then lands as
   one step back; when the sender stops stepping it runs on, and steps back by the length of
   the pause once the sender's packets resume. */
double coop_session_peer_tick(const coop_session_t *session, uint8_t seat);

/* ---- events ---- */

typedef enum coop_event_type {
    /* Guest: the host seated this player. `seat` is its seat, `tick` the host's tick at the
       welcome (information: this side's tick is its own), `data` the host's world key. */
    COOP_EVENT_WELCOME = 1,
    /* A player took `seat`. The host gets it for each guest; a guest gets it for every other
       player, the host and those seated before it included. A seat is returned to the same
       link identity with the same player key: a player who is still seated and connects
       again takes its seat over (the host closes the old connection with REPLACED and reports
       SEAT_LEFT, then SEAT_JOINED). That is safe where the transport authenticates identities
       (Steam) and not where a client names its own, where a connection can also take over
       another player's waiting connection (one waiting entry per identity); identity zero
       never takes a seat over. */
    COOP_EVENT_SEAT_JOINED,
    /* The player of `seat` is gone; `reason` says why. A guest's own close code reaches the
       host's game as sent and the other guests as LEFT, unless it is LEFT or a game code. */
    COOP_EVENT_SEAT_LEFT,
    /* Guest: the session is over for this side; `reason` says why. Nothing follows. */
    COOP_EVENT_CLOSED,
    /* One state packet from `seat`, stamped with the sender's `tick`. Packets may be missing
       and may come out of order; a packet never comes twice. The promise is against the
       network: a peer that forges its own stamps only disturbs its own path. */
    COOP_EVENT_STATE,
    /* One reliable record from `seat`, stamped with the sender's `tick`. Records of one seat
       come once and in the order sent. */
    COOP_EVENT_RECORD,
    /* Guest: a whole baseline from the host, stamped with the host's `tick` when it was sent.
       Lanes do not wait for each other, so records overtake a baseline on its way, and the
       session does not hold them back. Count the RECORD events from the host since WELCOME:
       the first `records_before` of them were sent before the baseline was cut and are already
       inside it; the later ones follow it and must be applied after it. Between WELCOME and
       BASELINE, state packets describe a world this side does not hold yet. */
    COOP_EVENT_BASELINE,
} coop_event_type_t;

typedef struct coop_event {
    coop_event_type_t type;
    uint8_t seat;
    uint64_t identity;   /* SEAT_JOINED, SEAT_LEFT: the link identity of the player */
    uint64_t player_key; /* SEAT_JOINED on the host; a guest is not told others' keys and reads 0 */
    uint32_t reason;     /* SEAT_LEFT, CLOSED */
    uint32_t tick;       /* WELCOME, STATE, RECORD, BASELINE */
    const uint8_t *data; /* readable until the next coop_session_poll */
    uint32_t size;
    uint32_t records_before; /* BASELINE */
} coop_event_t;

/* The next event, or false when none waits. A seat's events come in the order they happened;
   SEAT_JOINED precedes anything from that seat and SEAT_LEFT is its last. */
bool coop_session_poll(coop_session_t *session, coop_event_t *out);

/* ---- sending ---- */

typedef enum coop_send_result {
    COOP_SEND_OK = 0,
    COOP_SEND_DROPPED,   /* state, and the path had no room: gone, as if lost */
    COOP_SEND_FULL,      /* reliable, and the peer is not keeping up: not taken */
    COOP_SEND_TOO_LARGE, /* over coop_session_state_max, _record_max, or the baseline_max of either side */
    COOP_SEND_NO_SEAT,   /* nobody sits there, or a guest addressed a seat other than the host */
    COOP_SEND_INVALID,   /* no bytes, or a call the role does not have */
} coop_send_result_t;

/* The largest payload of one state packet and of one record on this session's link; a record
   also stays under COOP_RELIABLE_CAP. */
uint32_t coop_session_state_max(const coop_session_t *session);
uint32_t coop_session_record_max(const coop_session_t *session);

/* One state packet to `seat`, unreliable. The session prefixes its number, its tick and the
   acknowledgements of the packets received from that seat. `number`, when not NULL, gets the
   packet's number for coop_session_packet_fate. */
coop_send_result_t coop_session_send_state(coop_session_t *session, uint8_t seat, const void *data, uint32_t size, uint16_t *number);

/* One reliable record to `seat`, stamped with this side's tick. */
coop_send_result_t coop_session_send_record(coop_session_t *session, uint8_t seat, const void *data, uint32_t size);

/* A baseline to one guest: the whole world as the game packs it, sent on the low-priority
   lane in pieces of at most COOP_RELIABLE_CAP and handed to the guest whole. One baseline per
   guest at a time; a second call before the first has left returns FULL. Host only. */
coop_send_result_t coop_session_send_baseline(coop_session_t *session, uint8_t seat, const void *data, uint32_t size);

/* A baseline on its way: on the host, bytes of `seat`'s baseline handed to the link so far;
   on a guest (seat COOP_SEAT_HOST), bytes received of the one arriving. False when none is in
   flight. */
bool coop_session_baseline_progress(const coop_session_t *session, uint8_t seat, uint32_t *done, uint32_t *total);

/* What became of a state packet sent to `seat`. Numbers wrap at 16 bits; the session knows
   the fate of the last COOP_FATE_WINDOW packets it sent to a seat, older ones answer LOST:
   read a fate before it ages out. A packet is LOST once the peer acknowledged a packet more
   than 32 newer without it; ARRIVED and LOST are final. A receiver that takes more than 33
   packets in one poll acknowledges only the newest 33: the rest read LOST though they arrived,
   the safe direction (one redundant resend). */
typedef enum coop_packet_fate {
    COOP_PACKET_PENDING = 0, /* sent, no word yet */
    COOP_PACKET_ARRIVED,     /* the other side acknowledged it */
    COOP_PACKET_LOST,        /* never acknowledged while the session could still tell */
} coop_packet_fate_t;
coop_packet_fate_t coop_session_packet_fate(const coop_session_t *session, uint8_t seat, uint16_t number);

/* ---- measurements ---- */

/* What the session measured on the path to one seat. Unknown values are negative. */
typedef struct coop_seat_stats {
    /* Smoothed round trip, from acknowledged state packets. A guest reports its hello-to-
       welcome round trip until the first state sample. Samples taken right after a poll that
       came long after the last one that emptied the queue are skipped: that side measured its
       own wait. */
    float rtt_ms;
    float rtt_jitter_ms; /* smoothed deviation of the samples */
    float loss;          /* share of recent state packets never acknowledged, the session's empty ones too, 0..1 */
    uint32_t state_sent;     /* the game's state packets; the session's empty ones are not counted */
    uint32_t state_received; /* the game's state packets delivered */
    uint32_t state_stale;    /* any state packet received again or too old to place: dropped */
} coop_seat_stats_t;

/* False when nobody sits in `seat`. A guest asks about COOP_SEAT_HOST. */
bool coop_session_seat_stats(const coop_session_t *session, uint8_t seat, coop_seat_stats_t *out);

/* The link peer behind a seat, for net_link_stats; NET_LINK_PEER_NONE when nobody sits there. */
net_link_peer_t coop_session_peer(const coop_session_t *session, uint8_t seat);

#endif
