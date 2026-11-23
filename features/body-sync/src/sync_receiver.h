#ifndef BODY_SYNC_SYNC_RECEIVER_H
#define BODY_SYNC_SYNC_RECEIVER_H

/* The naive receiver: the newest state per body, applied as the stub applies it (a kinematic
   copy driven to the newest pose, placed outright when far, new or at rest, held still without
   news). A playback buffer replaces it behind the same calls. Pure C: it keeps states and
   decides; the shell moves the Box3D body as the decision says. */

#include <stdbool.h>
#include <stdint.h>

#include "sync_codec.h"

#define SYNC_SNAP_DISTANCE 2.0f /* m: a copy farther than this from its pose jumps there */

typedef struct sync_received {
    bool has;
    bool fresh;    /* not applied yet */
    uint8_t from;  /* the sender: a new sender's ticks are not comparable with the old one's */
    uint32_t tick; /* the sender's, of the packet, or the owner's sample tick when relayed */
    bool ordered;  /* `tick` orders the sender's next records: one session of that seat */
    sync_record_t newest;
    /* A sim record's own; for a copy record, estimated from its pose and the one before it
       from the same sender, so a body taken over from a copy keeps moving. */
    float velocity[3];
    float angular[3];
    /* Rare groups are sticky: the last value of each group received stays until replaced. */
    uint8_t known;
    sync_rare_t rare;
    /* The sender's tick of the newest record whose groups are kept: ahead of `tick` after a
       merge without the pose (sync_receiver_merge_rare). */
    uint32_t rare_tick;
} sync_received_t;

/* Host: what each guest last declared about a body it sends, in its own tick order. */
typedef struct sync_declared {
    bool seen;      /* a record of this body from that seat passed the tick order */
    bool known;     /* one of them carried the authority group */
    uint16_t number;
    uint32_t tick;
} sync_declared_t;

typedef struct sync_receiver {
    float dt;
    float max_speed;
    sync_received_t bodies[BODY_SYNC_BODIES_MAX];
    sync_declared_t declared[BODY_SYNC_SEATS_MAX][BODY_SYNC_BODIES_MAX];
} sync_receiver_t;

void sync_receiver_init(sync_receiver_t *r, uint32_t tick_hz, float max_speed);
void sync_receiver_forget(sync_receiver_t *r, uint16_t slot);
/* A seat joined or left. The tick order holds within one session of a seat: a seat that comes
   back counts its ticks anew. A join forgets every state received from that seat; a leave
   keeps them (the host continues that seat's bodies from them) but drops their order. What
   the seat declared starts over either way. */
void sync_receiver_forget_seat(sync_receiver_t *r, uint8_t seat, bool keep_states);

typedef enum sync_intake {
    SYNC_INTAKE_STALE = 0, /* older than a record of that seat already seen: nothing of it counts */
    SYNC_INTAKE_REJECTED,  /* that seat does not simulate the body under the number it declared */
    SYNC_INTAKE_TAKE,      /* store it */
} sync_intake_t;

/* Host: a guest's record of a body that `owner_seat` simulates under `owner_number`. The tick
   order comes first, so a late packet can neither move the declared number back nor be taken;
   the number counts from the authority group of a record that passed it. */
sync_intake_t sync_receiver_host_intake(sync_receiver_t *r, uint8_t seat, uint32_t tick, const sync_record_t *record, uint8_t owner_seat,
                                        uint16_t owner_number);

/* One decoded record from `from` stamped `tick`. False (stale) when that sender's same or a
   newer tick already arrived for the body; nothing of a stale record is kept. */
bool sync_receiver_store(sync_receiver_t *r, uint8_t from, uint32_t tick, const sync_record_t *record);
/* Only the record's rare groups, for a body this side simulates while its claim waits: the
   sender counts every group a packet carried as known once the packet arrived and does not send
   it again unchanged, so a group dropped here would stay unknown. The stored pose stays. False
   (nothing kept) for a record not newer than the groups kept, or with no ordered state of that
   sender to order it against. */
bool sync_receiver_merge_rare(sync_receiver_t *r, uint8_t from, uint32_t tick, const sync_record_t *record);

typedef enum sync_copy_move {
    SYNC_COPY_HOLD = 0, /* stop where it is */
    SYNC_COPY_DRIVE,    /* move to the pose over one step */
    SYNC_COPY_PLACE,    /* set the pose, velocities zero */
} sync_copy_move_t;

/* What the shell does with a copy at apply, from where it is now and whether it was placed
   since it became a copy. Consumes `fresh`; target is set for DRIVE and PLACE. */
sync_copy_move_t sync_receiver_decide(sync_receiver_t *r, uint16_t slot, const float now_position[3], const float now_rotation[4], bool placed,
                                      float target_position[3], float target_rotation[4]);

#endif
