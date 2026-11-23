#ifndef BODY_SYNC_SYNC_SENDER_H
#define BODY_SYNC_SYNC_SENDER_H

/* What goes into each seat's state packet, under a byte budget. Pure C: the Box3D shell feeds
   it the bodies after collect and the packet fates from body_sync_resolve; it owns every
   per-seat memory (priorities, what was sent and acknowledged) and decides, never touches a
   body. Fixed memory.

   Rate: each body has a rate, the packets per tick it wants. Each tick, for each seat it goes
   to, acc += rate. A body is due when acc >= 1, and a sent body's acc goes to 0. A packet takes,
   within min(budget, capacity):
     1. characters (class CHARACTER), every packet;
     2. bodies unsent for SYNC_AGE_MAX ticks or more, oldest first;
     3. due bodies, highest acc first, ties by slot from a cursor that turns every tick.
   A record that does not fit is skipped and the next one tried.

   rate = 1                      held, flying (hold until settled), just hit (its speed changed
                                 by SYNC_HIT_SPEED in one tick), or a rare group unacknowledged
          SYNC_RATE_MIN + (1 - SYNC_RATE_MIN) * min(1, speed / SYNC_FAST_SPEED)
                                 any other awake body, speed = |v| + SYNC_SPIN_RADIUS * |w|
          SYNC_RATE_REST         asleep, its rest pose not acknowledged by this seat
          0                      asleep and acknowledged: not sent until it wakes

   Staleness bound: with M awake bodies for a seat and room for N >= 1 records a packet after
   the characters, an awake body goes at most max(1 / SYNC_RATE_MIN, SYNC_AGE_MAX + ceil(M / N))
   ticks between two packets that carry it, budget overflowing or not. sync_sender_budget_ok
   refuses a budget without room for one widest record per seat's character and one more, so
   N >= 1 holds while each seat has at most one character (more characters need a budget with
   room for them). A body's priority and age reset when the packet that
   carried it is handed to the session (sync_sender_sent), not when it is written.

   A rest pose goes out as the precise record (sim at rest) whatever the seat's record kind:
   it is the pose the worlds agree on and the one a new owner starts from.

   Acknowledgement: a packet's rest poses and rare groups are remembered per seat for
   SYNC_SENT_WINDOW packets under the 16-bit number the session gave it. ARRIVED acknowledges a
   rest pose when the body is still in the sleep it was sent for, and a rare group when the
   group still has the version it carried. LOST, a packet never handed to the session, and one
   that leaves the window unresolved acknowledge nothing. Numbers are matched exactly inside
   the window, far shorter than the 16-bit wrap. "Changed" means changed on the wire: a body
   that falls asleep again on the quantized pose already acknowledged is not sent again, unless
   a moving record of it went out after that acknowledgement (the peer then holds that one).
   Without fates (no body_sync_resolve) nothing is ever acknowledged: sleeping bodies stay due
   at SYNC_RATE_REST and every rare group at every packet. */

#include <stdbool.h>
#include <stdint.h>

#include "sync_codec.h"

enum {
    SYNC_AGE_MAX = 30,
    /* Fates awaited per seat: the session names 33 packets an acknowledgement, plus about half
       a second of round trip at 60 packets a second. */
    SYNC_SENT_WINDOW = 64,
    /* Rest poses and rare groups one packet's fate can acknowledge; past it a packet still
       carries them, and they stay due until a later packet's fate acknowledges them. */
    SYNC_SENT_RECORDS = 128,
};
#define SYNC_RATE_MIN 0.25f
#define SYNC_RATE_REST 0.5f
#define SYNC_FAST_SPEED 2.0f   /* m/s at which a body is sent every tick */
#define SYNC_HIT_SPEED 1.0f    /* m/s of speed change in one tick that counts as a hit */
#define SYNC_SPIN_RADIUS 0.25f /* m: turns spin into a comparable speed without the body's size */
#define SYNC_DEFAULT_BUDGET 1000u

/* One body after collect. `state` is at full precision with every rare group's current value
   (its groups member is ignored); the sender limits, picks and quantizes. */
typedef struct sync_body_input {
    bool present;
    bool character;
    bool held;
    bool flying;
    bool asleep;
    sync_record_t state;
} sync_body_input_t;

typedef struct sync_candidate {
    uint16_t slot;
    uint8_t rank; /* 0 characters, 1 aged, 2 due */
    float key;    /* larger first */
} sync_candidate_t;

typedef struct sync_seat_body {
    float acc;
    uint32_t last_sent_tick; /* or the tick it became eligible */
    bool eligible;
    bool asleep;
    uint8_t sleep_epoch;
    bool rest_acked;
    bool rest_pose_known;
    float sleep_pose[7];     /* the quantized pose of the current sleep */
    float acked_pose[7];     /* the quantized rest pose this seat acknowledged */
    bool groups_known;
    sync_rare_t last_value;
    uint8_t version[SYNC_RARE_GROUPS];
    uint8_t acked_version[SYNC_RARE_GROUPS];
} sync_seat_body_t;

typedef struct sync_sent_record {
    uint16_t slot;
    bool rest;
    uint8_t sleep_epoch;
    uint8_t groups;
    uint8_t version[SYNC_RARE_GROUPS];
} sync_sent_record_t;

typedef struct sync_sent_packet {
    bool used;
    uint16_t number;
    uint32_t tick;
    uint16_t count;
    sync_sent_record_t records[SYNC_SENT_RECORDS];
} sync_sent_packet_t;

typedef struct sync_sender_stats {
    uint64_t bits[BODY_SYNC_TRAFFIC_COUNT];
    uint64_t deferred;
    uint32_t stale_max_ticks;
    uint32_t packets;
} sync_sender_stats_t;

typedef struct sync_sender {
    const sync_codec_t *codec;
    uint8_t seats;
    uint32_t budget;
    uint32_t tick;
    bool joined[BODY_SYNC_SEATS_MAX];
    bool wrote[BODY_SYNC_SEATS_MAX];
    bool staged_valid[BODY_SYNC_SEATS_MAX];
    uint16_t cursor;
    const sync_body_input_t *inputs; /* the tick's, valid until the next sync_sender_tick */
    bool hit[BODY_SYNC_BODIES_MAX]; /* this tick's speed changed by SYNC_HIT_SPEED or more */
    bool has_velocity[BODY_SYNC_BODIES_MAX];
    float velocity[BODY_SYNC_BODIES_MAX][3];
    sync_candidate_t candidates[BODY_SYNC_BODIES_MAX]; /* one write's scratch */
    /* Every body of the packet written and not yet handed over, for the reset at the hand-over. */
    uint16_t staged_slots[BODY_SYNC_SEATS_MAX][BODY_SYNC_BODIES_MAX];
    bool staged_rest[BODY_SYNC_SEATS_MAX][BODY_SYNC_BODIES_MAX];
    uint16_t staged_slot_count[BODY_SYNC_SEATS_MAX];
    sync_seat_body_t bodies[BODY_SYNC_SEATS_MAX][BODY_SYNC_BODIES_MAX];
    sync_sent_packet_t staged[BODY_SYNC_SEATS_MAX];
    sync_sent_packet_t sent[BODY_SYNC_SEATS_MAX][SYNC_SENT_WINDOW];
    uint32_t sent_next[BODY_SYNC_SEATS_MAX]; /* ring position of the next packet */
    sync_sender_stats_t stats[BODY_SYNC_SEATS_MAX];
    float round_trip[BODY_SYNC_SEATS_MAX]; /* ticks from a packet's send to its arrival fate, smoothed; 0 unknown */
} sync_sender_t;

/* Room for one widest record for every seat's character and one more, with the end bit:
   below it the staleness bound has nothing to stand on. 0 takes SYNC_DEFAULT_BUDGET. */
bool sync_sender_budget_ok(const sync_codec_t *codec, uint8_t seats, uint32_t budget);

/* budget 0 takes SYNC_DEFAULT_BUDGET. */
void sync_sender_init(sync_sender_t *sender, const sync_codec_t *codec, uint8_t seats, uint32_t budget);

/* A seat joined or left: its priorities, acknowledgements and window start over, so a seat
   that comes back gets every rest pose and rare group again. */
void sync_sender_seat(sync_sender_t *sender, uint8_t seat, bool joined);
/* A slot bound or unbound: its memory starts over, and no packet still in a window can
   acknowledge anything of the new body. */
void sync_sender_forget(sync_sender_t *sender, uint16_t slot);

/* After collect: `bodies` holds BODY_SYNC_BODIES_MAX inputs by slot and `send_to` a seat mask
   per slot (which seats that body goes to); both stay valid until the writes of this tick are
   done. Updates rates, sleeps, rare versions and staleness. */
void sync_sender_tick(sync_sender_t *sender, uint32_t tick, const sync_body_input_t *bodies, const uint8_t *send_to);

/* The tick's one packet for `seat`: at most min(budget, capacity) bytes, moving bodies as sim
   records when `sim`, as copies otherwise. Returns its size; 0 when nothing is due, the seat
   already had its packet this tick, or the capacity is below BODY_SYNC_RECORD_MAX. A packet
   written before without sync_sender_sent counts as lost. */
uint32_t sync_sender_write(sync_sender_t *sender, uint8_t seat, bool sim, uint8_t *buffer, uint32_t capacity);

/* The packet just written left under `number`: it enters the window, evicting the oldest. */
void sync_sender_sent(sync_sender_t *sender, uint8_t seat, uint16_t number);

/* Numbers in the window whose fate is awaited, oldest first. */
uint32_t sync_sender_unresolved(const sync_sender_t *sender, uint8_t seat, uint16_t *numbers, uint32_t max);
/* A final fate; a number not in the window changes nothing. */
void sync_sender_resolve(sync_sender_t *sender, uint8_t seat, uint16_t number, bool arrived);
/* The round trip to a seat in ticks, as the fates measure it (send to acknowledgement, which
   includes the other side's wait to acknowledge); 0 before the first fate. */
float sync_sender_round_trip(const sync_sender_t *sender, uint8_t seat);
/* `seat` acknowledged the body's current rest pose and every rare group it carries now. */
bool sync_sender_rest_acked(const sync_sender_t *sender, uint8_t seat, uint16_t slot);

#endif
