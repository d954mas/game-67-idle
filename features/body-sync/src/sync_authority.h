#ifndef BODY_SYNC_SYNC_AUTHORITY_H
#define BODY_SYNC_SYNC_AUTHORITY_H

/* Who simulates each body: claims, answers, releases, forced moves, seats leaving, and the
   reliable records that carry them, with the bytes of stub_wire's record format. Pure C, no
   Box3D: the shell (the stub or the real seam) asks it and reports what it received; it never
   touches a body. Both shells link this one copy, so they decide authority alike.

   A body that becomes local continues the last owner's: its flags, hold and (for a seat that
   left) holder from the newest state received, now; its motion type, locks and velocities at
   the shell's next apply, which `resume` asks for. The records that move authority (a grant, a
   forced move, a release) carry the body's flags, motion type and locks as its owner left
   them, and those win over the state received, which may not have arrived yet. A claimant
   keeps on top what it changed itself since its claim: the flags bit by bit, the motion type
   and the locks, each apart. The host's records carry its body as it is when it answers or
   moves it: the shell notes the live motion first (sync_authority_note_motion). */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "features/body_sync/body_sync_types.h"
#include "stub_wire.h"

typedef struct sync_authority_body {
    bool bound;
    uint16_t generation;
    uint8_t seat;
    uint16_t number;
    uint8_t hold;
    body_sync_id_t holder;
    body_sync_id_t last_holder; /* a settled hold ignores contacts with the character that let it go */
    uint8_t flags;
    bool claim_pending;   /* guest: simulating optimistically until the host answers claim_seq */
    uint16_t claim_seq;
    bool holder_mine;     /* guest: the caller set the holder since this endpoint became local */
    uint16_t granted_seq; /* host: the claim the current owner was granted, 0 after any other move */
    bool resume;          /* became local: the shell continues it from the newest state at apply */
    /* The newest state received for the body, as far as authority reads it. */
    bool has_in;
    uint8_t in_hold;
    body_sync_id_t in_holder;
    uint8_t in_flags;
    /* Motion type and locks: a local body's as the shell read them at collect, a copy's as
       received; what a record of this side carries. */
    bool motion_known;
    bool kinematic;
    uint8_t locks;
    /* A record's motion to continue from at the next resume. */
    bool take_motion;
    bool take_kinematic;
    uint8_t take_locks;
    /* What the shell set at the last resume, to tell the owner's own changes since. */
    bool applied_valid;
    bool applied_kinematic;
    uint8_t applied_locks;
    uint8_t flags_mine; /* the flag bits set_flags changed since this endpoint became local */
    /* The opaque bytes: a local body's, and the newest received; a new owner keeps its own
       once it set them since its claim, as with flags. */
    uint8_t data_size;
    uint8_t data[STUB_DATA_MAX];
    uint8_t in_data_size;
    uint8_t in_data[STUB_DATA_MAX];
    bool data_mine;
    bool kinematic_mine; /* the owner changed the motion type since the last resume */
    bool locks_mine;     /* the owner changed the locks since the last resume */
} sync_authority_body_t;

typedef struct sync_authority_pending {
    bool active;
    bool polled;
    uint16_t sequence;
    uint8_t hold;
    uint32_t tick;
    uint32_t order;
} sync_authority_pending_t;

enum { SYNC_AUTHORITY_RECORD_BYTES = STUB_RECORD_BYTES_MAX, SYNC_AUTHORITY_OWED_WORDS = BODY_SYNC_BODIES_MAX / 64 };

typedef struct sync_authority {
    uint8_t self;
    uint8_t seats;
    bool joined[BODY_SYNC_SEATS_MAX];
    sync_authority_body_t bodies[BODY_SYNC_BODIES_MAX];
    /* guest: claims and releases to the host, in order */
    uint8_t queue[BODY_SYNC_RECORDS_QUEUED][SYNC_AUTHORITY_RECORD_BYTES];
    uint8_t queue_size[BODY_SYNC_RECORDS_QUEUED];
    uint32_t queue_head, queue_count;
    uint16_t claim_counter;
    /* host: claims, and what each seat is owed (a body's latest authority, the answer to its claim) */
    sync_authority_pending_t pending[BODY_SYNC_SEATS_MAX][BODY_SYNC_BODIES_MAX];
    uint32_t pending_count, order;
    uint64_t owed[BODY_SYNC_SEATS_MAX][SYNC_AUTHORITY_OWED_WORDS];
    uint16_t answer[BODY_SYNC_SEATS_MAX][BODY_SYNC_BODIES_MAX];
    bool forced[BODY_SYNC_SEATS_MAX][BODY_SYNC_BODIES_MAX]; /* the owed move answers no claim */
    char *error; /* the shell's last-error text, written on INVALID */
    size_t error_size;
} sync_authority_t;

void sync_authority_init(sync_authority_t *a, uint8_t self, uint8_t seats, char *error, size_t error_size);
bool sync_authority_is_host(const sync_authority_t *a);

/* Joining forgets what the seat was owed and its claims. Leaving does too, and on the host
   every body of that seat returns to the host with its received hold and holder. */
void sync_authority_seat_joined(sync_authority_t *a, uint8_t seat);
void sync_authority_seat_left(sync_authority_t *a, uint8_t seat);

/* The shell has checked the id and the body; TAKEN when the slot holds a body. */
body_sync_result_t sync_authority_bind(sync_authority_t *a, body_sync_id_t id, uint8_t seat, uint16_t number);
body_sync_result_t sync_authority_unbind(sync_authority_t *a, body_sync_id_t id);

/* NULL when nothing is bound under `id` (slot, generation). */
sync_authority_body_t *sync_authority_body(sync_authority_t *a, body_sync_id_t id);
const sync_authority_body_t *sync_authority_body_const(const sync_authority_t *a, body_sync_id_t id);
bool sync_authority_local(const sync_authority_t *a, const sync_authority_body_t *b);
/* Hold and holder are the owner's: the received ones for a copy. */
void sync_authority_describe(const sync_authority_t *a, const sync_authority_body_t *b, body_sync_authority_t *out);

/* The body_sync.h calls of the same names, with the same results. */
body_sync_result_t sync_authority_claim(sync_authority_t *a, body_sync_id_t id, body_sync_hold_t hold);
body_sync_result_t sync_authority_release(sync_authority_t *a, body_sync_id_t id);
bool sync_authority_poll(sync_authority_t *a, body_sync_claim_event_t *out);
body_sync_result_t sync_authority_answer(sync_authority_t *a, const body_sync_claim_event_t *claim, bool granted);
body_sync_result_t sync_authority_assign(sync_authority_t *a, body_sync_id_t id, uint8_t seat, body_sync_hold_t hold);
body_sync_result_t sync_authority_set_holder(sync_authority_t *a, body_sync_id_t id, body_sync_id_t holder);
body_sync_result_t sync_authority_set_flags(sync_authority_t *a, body_sync_id_t id, uint8_t flags);
uint8_t sync_authority_flags(const sync_authority_t *a, body_sync_id_t id);
body_sync_result_t sync_authority_set_data(sync_authority_t *a, body_sync_id_t id, const void *data, uint32_t size);
/* A local body's bytes, or a copy's newest received. */
uint32_t sync_authority_data(const sync_authority_t *a, body_sync_id_t id, void *out, uint32_t capacity);

/* The shell stored a newer state of the body: what a take inherits from it. */
void sync_authority_received(sync_authority_body_t *b, uint8_t hold, body_sync_id_t holder, uint8_t flags);
/* The newest data bytes received for a copy. */
void sync_authority_received_data(sync_authority_body_t *b, uint8_t size, const uint8_t *data);
/* A copy's motion as received (known false: never received). */
void sync_authority_received_motion(sync_authority_body_t *b, bool known, bool kinematic, uint8_t locks);

/* A local body's motion as the shell read it at collect. */
void sync_authority_note_motion(sync_authority_t *a, sync_authority_body_t *b, bool kinematic, uint8_t locks);
/* The motion a resume continues from: a record's, else the state received, else the bind's. */
void sync_authority_resume_motion(const sync_authority_body_t *b, bool received_known, bool received_kinematic, uint8_t received_locks,
                                  bool bound_kinematic, uint8_t bound_locks, bool *kinematic, uint8_t *locks);
/* The shell set this motion at a resume. */
void sync_authority_applied(sync_authority_body_t *b, bool kinematic, uint8_t locks);

/* The seats a body's state goes to this tick (bit per seat): a guest sends what it simulates
   to the host; the host sends each joined guest every body that guest does not simulate, a
   guest's own body only when it has a state of it to relay. */
uint8_t sync_authority_route(const sync_authority_t *a, uint32_t slot, bool have_state);
/* How a copy is held here: on the host a body a guest simulates stays dynamic with its own
   mass, its velocity corrected toward the guest's state, so it pushes light bodies and is held
   by heavy ones; unless the owner holds it kinematic (carried), and everywhere else, a copy is
   kinematic and driven to its playback pose. */
bool sync_authority_holds_dynamic(const sync_authority_t *a, const sync_authority_body_t *b, bool owner_kinematic);

/* The hold of a copy here, holds_dynamic's successor once the second layer lands. */
typedef enum sync_hold_kind {
    SYNC_HOLD_DRIVEN = 0, /* kinematic, driven to the playback pose (sync_drive_reachable) */
    SYNC_HOLD_CORRECTED,  /* dynamic, the owner's velocity plus a bounded correction (sync_drive_dynamic) */
    SYNC_HOLD_SOFT,       /* dynamic, simulated here and pulled softly (sync_soft.h) */
} sync_hold_kind_t;
/* CORRECTED as holds_dynamic says. SOFT on a guest with the layer on (`light`: class LIGHT and
   mass at most light_mass_max) for a body of another seat with no holder, no settled hold (not
   in flight) and not kinematic at its owner. DRIVEN otherwise: heavy copies, characters, held
   and flying bodies stay as 0.6.0 holds them. */
sync_hold_kind_t sync_authority_hold_kind(const sync_authority_t *a, const sync_authority_body_t *b, bool owner_kinematic, bool light);

/* Whether this side's packets carry moving bodies as sim records: a guest's do, since the host
   may take any of them back at any tick; the host's go to guests as copies. */
bool sync_authority_sends_sim(const sync_authority_t *a);

/* A local body with a settled hold, after the step: asleep, or touched a moving body other
   than its last holder. True when that ended the hold (see sync_authority_settled). */
bool sync_authority_settle(sync_authority_t *a, body_sync_id_t id, bool asleep, bool touched_moving);

/* The id a record's holder names on this side, by slot and low generation bits; NONE when no
   body of that generation is bound there. */
body_sync_id_t sync_authority_holder_id(const sync_authority_t *a, uint32_t slot, uint8_t generation_low, uint8_t generation_bits);

/* Guest: the authority a host state names (seat, number, hold). A state never makes this
   endpoint local: only an announcement or its own claim does. */
void sync_authority_from_state(sync_authority_t *a, sync_authority_body_t *b, uint8_t seat, uint16_t number, uint8_t hold);

/* One reliable record from `seat`: FORMAT for bytes stub_wire refuses, its reason in `why`.
   A record naming no bound body counts in *stale. */
body_sync_result_t sync_authority_receive(sync_authority_t *a, uint8_t seat, uint32_t tick, const uint8_t *data, uint32_t size, uint32_t *stale,
                                          char *why, size_t why_size);

/* A settled hold ended at collect (asleep, or a contact): the host keeps the body with no hold,
   a guest releases it when its queue has room. Returns the hold the body has now. */
uint8_t sync_authority_settled(sync_authority_t *a, body_sync_id_t id);

/* The records owed to `seat`, as body_sync_peek_record and body_sync_pop_record; pop returns
   the bytes it removed, zero when none. */
bool sync_authority_peek(const sync_authority_t *a, uint8_t seat, uint8_t *buffer, uint32_t capacity, uint32_t *size);
uint32_t sync_authority_pop(sync_authority_t *a, uint8_t seat);

#endif
