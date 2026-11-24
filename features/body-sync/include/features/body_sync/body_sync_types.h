#ifndef FEATURES_BODY_SYNC_BODY_SYNC_TYPES_H
#define FEATURES_BODY_SYNC_BODY_SYNC_TYPES_H

/* The seam's numbers and plain types, without Box3D: body_sync.h includes this, and the parts
   of the real implementation that decide without touching a body (codec, sender, receiver,
   authority) include only this. */

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    BODY_SYNC_SEATS_MAX = 4,
    BODY_SYNC_SEAT_HOST = 0,
    BODY_SYNC_SEAT_NONE = 0xFF,
    /* Slots of bound bodies per endpoint; the copy record names a slot in 10 bits. */
    BODY_SYNC_BODIES_MAX = 1024,
    /* Low bits of body_sync_set_flags that travel with a body's pose; higher bits are refused. */
    BODY_SYNC_FLAG_BITS = 4,
    /* The largest record the seam writes; a capacity below it gets no record. */
    BODY_SYNC_RECORD_MAX = 64,
    /* A guest's claims and releases queued and not yet popped; past it they answer FULL. The
       host's records never fill: what it owes a seat is kept per body and the newest is sent. */
    BODY_SYNC_RECORDS_QUEUED = 256,
    /* Opaque bytes a caller attaches to a body (body_sync_set_data). */
    BODY_SYNC_DATA_MAX = 8,
    /* Shapes per body whose collision mask a copy's class may change (character_category). */
    BODY_SYNC_SHAPES_MAX = 4,
};

/* A body's network number: slot in the low 16 bits (below BODY_SYNC_BODIES_MAX), generation in
   the high 16 bits (1..65535; a slot taken again takes the next one). Zero is no body. Every
   endpoint binds the same number to its own copy of the same body. An implementation may send
   fewer generation bits; it names how many in its README, since a stale packet whose
   generation matches modulo that width reads as current. */
typedef uint32_t body_sync_id_t;
#define BODY_SYNC_ID_NONE 0u

static inline body_sync_id_t body_sync_id_make(uint32_t slot, uint32_t generation) {
    return (slot & 0xFFFFu) | (generation << 16);
}
static inline uint32_t body_sync_id_slot(body_sync_id_t id) { return id & 0xFFFFu; }
static inline uint32_t body_sync_id_generation(body_sync_id_t id) { return id >> 16; }

typedef enum body_sync_result {
    BODY_SYNC_OK = 0,
    BODY_SYNC_INVALID, /* a call outside the contract: id, seat or role out of range; nothing changed */
    BODY_SYNC_TAKEN,   /* bind: the slot already holds a body */
    BODY_SYNC_UNKNOWN, /* no body under that id, or no such claim pending; nothing changed */
    BODY_SYNC_FULL,    /* a guest's record queue is full: retry after popping; nothing changed */
    BODY_SYNC_FORMAT,  /* peer bytes refused whole; close that peer */
} body_sync_result_t;

/* A hint to the receiver about the body; how it is honoured is the implementation's. The stub
   ignores it. */
typedef enum body_sync_class {
    BODY_SYNC_CLASS_LIGHT = 0, /* a copy need not block this endpoint's own character */
    BODY_SYNC_CLASS_HEAVY,     /* a copy blocks */
    BODY_SYNC_CLASS_CHARACTER, /* a player's capsule: sent in every packet, its seat's timeline */
} body_sync_class_t;

typedef enum body_sync_hold {
    BODY_SYNC_HOLD_UNTIL_RELEASE = 0, /* carried: authority stays until body_sync_release */
    /* Thrown or punched by its owner: the seam itself returns it to the host on its first
       contact with a non-static body other than its last holder, or when it falls asleep, so
       one side judges its contacts. Contacts are seen only on shapes with contact events on. */
    BODY_SYNC_HOLD_UNTIL_SETTLED,
    /* Owned by interaction (config authority INTERACTION): the seat whose action set it moving
       simulates it until it and what it touches rest, then the seam hands it back to the host;
       on the host's own body it marks one the host's agents set going. Only the seam sets it: a
       caller's claim with it is INVALID. */
    BODY_SYNC_HOLD_UNTIL_REST,
} body_sync_hold_t;

/* Who owns a free body (body_sync_config_t.authority). */
typedef enum body_sync_authority_mode {
    BODY_SYNC_AUTHORITY_HOST = 0,    /* the host, but for what a guest carries or threw in flight */
    BODY_SYNC_AUTHORITY_INTERACTION, /* the seat whose action set it moving, until it rests */
} body_sync_authority_mode_t;

typedef struct body_sync_authority {
    uint8_t seat;    /* the seat that simulates the body */
    uint16_t number; /* grows on every change the host makes; older numbers lose */
    body_sync_hold_t hold;
    body_sync_id_t holder; /* the character carrying it, BODY_SYNC_ID_NONE when free */
} body_sync_authority_t;

/* A claim that reached the host, in arrival order. Host only; false when none waits. Every
   claim is polled exactly once unless its claimant released the body or claimed it again
   first (only its newest claim of a body counts), or left, or the body was unbound. */
typedef struct body_sync_claim_event {
    body_sync_id_t id;
    uint8_t seat;
    uint16_t sequence; /* the claimant's own count of its claims, never 0 */
    uint32_t tick;     /* the claimant's tick when it claimed */
    body_sync_hold_t hold;
} body_sync_claim_event_t;

/* The host's verdict on a guest's contact claim (body_sync_check_contact). */
typedef enum body_sync_contact_verdict {
    /* The seat simulates the agent under that number now, or that ownership ended by its own
       release or the host's arbitration within the grace. */
    BODY_SYNC_CONTACT_OK = 0,
    /* Nothing to judge: a body, generation, seat or number the host does not know for that seat,
       or an end past its grace. */
    BODY_SYNC_CONTACT_STALE,
    /* The host recalled that ownership (an assign, the seat leaving): its late claims are dropped. */
    BODY_SYNC_CONTACT_REVOKED,
} body_sync_contact_verdict_t;

/* The host's own world around a contact claim, metres and m/s; the game sets its thresholds. */
typedef struct body_sync_contact {
    /* Between the boxes of the agent and the target, the agent along the nearest path its taken
       states show (README "Contact claims"). INFINITY when the verdict is STALE or the host took
       no state of that ownership within the grace. */
    float gap;
    /* How far apart they may be after a real touch along that path: the take slop, the target's
       travel and the path's bend over its time. */
    float reach;
    /* The part of `reach` a touch itself adds (Box3D's contact distance, a pose's rounding and turn,
       with margin): the rest is the evidence's own slop, what a rule judging a reach other than a
       touch (a grab) adds to that reach instead. */
    float slop;
    /* The agent's fastest travel between its taken states, at most max_speed, plus the target's
       shown speed. */
    float approach_max;
    /* The verdict read an ownership that ended, `ended_age_ticks` of the host's ticks ago. */
    bool ended;
    uint32_t ended_age_ticks;
} body_sync_contact_t;

typedef enum body_sync_traffic_kind {
    BODY_SYNC_TRAFFIC_HEADER = 0, /* the seam's own bytes in a packet: counts, ticks, acks */
    BODY_SYNC_TRAFFIC_COPY,       /* poses for interpolated copies */
    BODY_SYNC_TRAFFIC_SIM,        /* poses with velocities a receiver simulates from */
    BODY_SYNC_TRAFFIC_HELD,       /* carried bodies riding their holder's record */
    BODY_SYNC_TRAFFIC_AUTHORITY,  /* claims, releases, announcements, authority numbers */
    BODY_SYNC_TRAFFIC_COUNT,
} body_sync_traffic_kind_t;

/* A JSON key: "header", "copy", "sim", "held", "authority"; "unknown" past the last kind. */
const char *body_sync_traffic_kind_name(body_sync_traffic_kind_t kind);

/* Counts since create on the path to and from one seat. The host asks about each guest seat,
   a guest about BODY_SYNC_SEAT_HOST. */
typedef struct body_sync_traffic {
    uint64_t sent_bytes[BODY_SYNC_TRAFFIC_COUNT];
    uint64_t received_bytes[BODY_SYNC_TRAFFIC_COUNT];
    uint32_t packets_sent;
    uint32_t records_sent;
    uint32_t stale;    /* records naming an unbound id, an old generation, an old tick or number */
    uint32_t rejected; /* poses of bodies the sender had no authority over */
    uint32_t refused;  /* packets and records refused with FORMAT */
    /* A budgeted sender's starvation, sent side only (zero for the stub): records that were due
       but did not fit a packet, and the most ticks an awake body went between two packets. */
    uint64_t deferred;
    uint32_t stale_max_ticks;
    /* Authority following interaction (zero under the host policy and for the stub). On a guest,
       for the host: takes sent and bodies returned at rest. On the host, for a guest: its takes
       granted and refused, and its bodies the host's agents took back. */
    uint32_t takes;
    uint32_t take_refusals;
    uint32_t reclaims;
    uint32_t returns;
} body_sync_traffic_t;

#ifdef __cplusplus
}
#endif

#endif
