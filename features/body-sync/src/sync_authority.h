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
#include "sync_policy.h"

/* A state the host took of a guest's body: where, the sender's tick, the host's tick. The ring
   keeps enough of them to span a contact claim's grace at the full state rate of a flying body
   under the slowest honest path (README "Contact claims"). */
enum { SYNC_AUTHORITY_POSES = 12 };
typedef struct sync_authority_pose {
    float position[3];
    float step[3]; /* metres a tick from the state before it in the ring; 0 for the first */
    uint32_t tick, host_tick;
} sync_authority_pose_t;

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
    bool takable;        /* light for the interaction policy: class LIGHT within light_mass_max */
    bool claim_take;     /* guest: the pending claim is a take (sync_authority_take) */
    bool take_refused;   /* guest: a take of it was refused and no move to this seat came since */
    uint8_t number_lag; /* guest: sync_number_lag_t, what the host's number is against b->number */
    /* Host: the owner's newest states the shell took, a ring of SYNC_AUTHORITY_POSES of one
       ownership (seat and number): the only evidence of where its contacts can be, since the
       velocities it declares are its own word. Kept past the ownership's end for its claims. */
    uint8_t pose_seat;
    uint16_t pose_number;
    uint8_t pose_count, pose_next;
    sync_authority_pose_t poses[SYNC_AUTHORITY_POSES];
} sync_authority_body_t;

/* Guest: what its release did to the host's number, until it hears a newer one. */
typedef enum sync_number_lag {
    SYNC_LAG_NONE = 0,
    SYNC_LAG_ONE,     /* it released a body it owned: the host's move adds one */
    /* It released a body it only claimed or took: the host withdrew or refused that (no move),
       or granted it and then took the release (two), or still owes a crossed release's move. */
    SYNC_LAG_UNKNOWN,
} sync_number_lag_t;

typedef struct sync_authority_pending {
    bool active;
    bool polled;
    uint16_t sequence;
    uint8_t hold;
    uint32_t tick;
    uint32_t order;
} sync_authority_pending_t;

enum { SYNC_AUTHORITY_RECORD_BYTES = STUB_RECORD_BYTES_MAX, SYNC_AUTHORITY_OWED_WORDS = BODY_SYNC_BODIES_MAX / 64 };

/* How a guest's ownership of a body ended, for its contact claims (sync_authority_check_contact). */
typedef enum sync_authority_end {
    SYNC_END_NONE = 0,
    SYNC_END_VOLUNTARY,  /* its own release: a flight's return at contact or rest, a let-go */
    /* The host settled a contest: its take-back or claim, a grant or take to another seat. The
       guest simulates on as owner until it hears, so its claims of that time are honest. */
    SYNC_END_ARBITRATED,
    /* The host recalled it (an assign, to any seat; the seat left): late claims are dropped. */
    SYNC_END_REVOKED,
} sync_authority_end_t;

typedef struct sync_authority_ended {
    uint16_t number;
    uint8_t how;   /* sync_authority_end_t */
    bool has_pose; /* the host took a state of that ownership from the seat */
    uint32_t tick; /* the host's `now` at the end */
    /* The seat's last state the host took, its step from the one before (metres a tick), the
       host's `now` then and the fastest travel its taken states showed (metres a tick): the
       evidence left once another ownership's states replaced the ring. */
    float position[3];
    float step[3];
    uint32_t pose_tick;
    float travel;
} sync_authority_ended_t;

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
    sync_policy_mode_t mode;
    /* Counts for body_sync_traffic: a guest's takes sent and returns, the host's answers by seat. */
    uint32_t takes_sent, returns_sent;
    uint32_t granted[BODY_SYNC_SEATS_MAX], refused_takes[BODY_SYNC_SEATS_MAX], reclaimed[BODY_SYNC_SEATS_MAX];
    /* Host: bodies each seat owns by interaction, kept as they move; each seat's take budget
       (SYNC_POLICY_TAKE_BURST_TICKS) and the tick it was last topped up. The budget runs on
       `now`, the shell's last apply or collect tick: a record's tick is its sender's stamp, and
       stamps far apart would refill it at will. */
    uint32_t owned_by[BODY_SYNC_SEATS_MAX];
    float take_budget[BODY_SYNC_SEATS_MAX];
    uint32_t take_budget_tick[BODY_SYNC_SEATS_MAX];
    /* Host: a bit per seat and slot, set by a refused take and cleared by the next move of the
       body to that seat, which skips a number (sync_authority_declared_number). */
    uint64_t refused[BODY_SYNC_SEATS_MAX][SYNC_AUTHORITY_OWED_WORDS];
    uint32_t now;
    /* Host: each seat's newest ownership of each body that ended, kept through a leave until the
       body is bound again or the seat joins after one. */
    sync_authority_ended_t ended[BODY_SYNC_SEATS_MAX][BODY_SYNC_BODIES_MAX];
    /* A planted defect for tests that require a check to catch it: a revocation is recorded as
       an arbitrated end. Never set outside tests. */
    bool plant_revoked_as_arbitrated;
    /* A planted defect: every take's evidence reads an unbounded reach. Never set outside tests. */
    bool plant_take_reach_unbounded;
    /* Host: the shell's measure of a take's evidence (sync_policy_grant_in_t agent_gap, *reach).
       None: every take counts as touching, for the parts tested without a world. */
    float (*take_gap)(void *context, uint8_t seat, uint32_t slot, float *reach);
    void *take_context;
    char *error; /* the shell's last-error text, written on INVALID */
    size_t error_size;
} sync_authority_t;

void sync_authority_init(sync_authority_t *a, uint8_t self, uint8_t seats, char *error, size_t error_size);
bool sync_authority_is_host(const sync_authority_t *a);

/* Joining forgets what the seat was owed and its claims and starts its take budget full; a join
   after a leave (not a repeated one) also forgets its ended ownerships. Leaving forgets the rest
   too, and on the host every body of that seat returns to the host with its received hold and
   holder, an end the seat's contact claims read as revoked. */
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

/* Host: the shell took a state of the body from its owner `seat` stamped with that sender's
   `tick`, at the host's `now`: the evidence of that ownership's contact claims. A state of
   another ownership starts the ring over. The time from the state before it is the sender's
   span, but no less than the host's own span less `jitter_ticks` (the spread its playback delay
   already absorbs), and at least a tick: a sender stamping its states close together cannot
   shorten the host's clock past the jitter it is granted anyway. */
void sync_authority_received_pose(sync_authority_body_t *b, uint8_t seat, uint32_t tick, uint32_t now, uint32_t jitter_ticks,
                                  const float position[3]);
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
   mass at most light_mass_max; the caller passes it for a flight held until settled only near
   this side's agents) for a body of another seat with no holder and not kinematic at its owner.
   DRIVEN otherwise: heavy copies, characters and held bodies stay as 0.6.0 holds them. */
sync_hold_kind_t sync_authority_hold_kind(const sync_authority_t *a, const sync_authority_body_t *b, bool owner_kinematic, bool light);

/* Whether this side's packets carry moving bodies as sim records: a guest's do, since the host
   may take any of them back at any tick; the host's go to guests as copies. */
bool sync_authority_sends_sim(const sync_authority_t *a);

/* A local body with a settled hold, after the step: asleep, or touched a moving body other
   than its last holder (`touched_untakable`: one this side may not take). The policy decides
   (sync_policy_flight): true when the hold ended (see sync_authority_settled); a flight that goes
   on owned by interaction takes the hold until rest instead. */
bool sync_authority_settle(sync_authority_t *a, body_sync_id_t id, bool asleep, bool touched_moving, bool touched_untakable);

/* ---- authority follows interaction (sync_policy.h) ---- */

/* Guest: an optimistic take of a body one of its agents set moving, as a claim with the hold
   until rest that the host answers itself. FULL when the queue to the host has no room. */
body_sync_result_t sync_authority_take(sync_authority_t *a, body_sync_id_t id);
/* The authority number a local body's state declares: while a take waits, the number its grant
   will give (the host's move adds one), so the host takes its states from the grant on instead
   of a round trip later. A refusal moves no number: another seat's take granted next gets the
   number its states declared, and refusals, which spend no take budget, cannot drive a number
   on. The host instead remembers the refused seat (one bit per seat and slot), and its next
   move of the body to that seat adds two; the guest, told of the refusal, declares two ahead
   for its next take as well. A state of the refused take declared at most the claimant's
   number then plus one, and the claimant's number never passes the host's, which is N when
   that later move comes: N + 2 is one no such state carries, however the number moved while
   the refusal was on its way. A guest that released a body it owned and has heard no newer
   number counts from one on, as the host's move of the release did (or a forced move before
   it); one that released a body it only claimed or took cannot know the host's number until it
   hears a newer one (sync_authority_contact_number names none meanwhile). */
uint16_t sync_authority_declared_number(const sync_authority_t *a, const sync_authority_body_t *b);
/* Bodies `seat` owns by interaction now (a hold until rest); this guest's own count includes its
   takes waiting for the grant. */
uint32_t sync_authority_owned(const sync_authority_t *a, uint8_t seat);
/* Host: its own body set going by its agents (hold until rest), or that hold ended. */
void sync_authority_host_active(sync_authority_t *a, body_sync_id_t id, bool active);
/* Host: one of its agents touched a guest's body owned by interaction: it comes back to the host
   at once, forced, still owned by interaction there. */
body_sync_result_t sync_authority_reclaim(sync_authority_t *a, body_sync_id_t id);

/* Host: whether `seat` may claim a contact of `agent` under `number` (body_sync_check_contact,
   the ownership half): every argument untrusted, anything unknown STALE. A voluntary or
   arbitrated end counts for `grace_ticks` of `now` after it. *ended names the end the verdict
   read, NULL while the seat owns the agent or for STALE. */
body_sync_contact_verdict_t sync_authority_check_contact(const sync_authority_t *a, uint8_t seat, body_sync_id_t agent, uint16_t number,
                                                         uint32_t grace_ticks, const sync_authority_ended_t **ended);
/* What the host's world says around a contact or a take, for sync_authority_contact_measure:
   the target's box and speed (m/s), the agent's box less its position (where it sits around a
   pose), and the limits. */
typedef struct sync_contact_scene {
    float target_lo[3], target_hi[3];
    float agent_lo[3], agent_hi[3];
    float target_speed;
    uint32_t tick_hz;
    float max_speed;
    float slack_ticks;     /* a state may cross slower than the claim behind it: the playback delay and a tick */
    uint32_t window_ticks; /* how old a taken state may be and still place the contact: the claim's grace */
    /* The least time the target may have travelled since the touch, ticks: the guest touched its
       copy of it, a one-way trip and a delay old, and the claim takes the way back. */
    float target_ticks;
    /* Ticks the agent may have pushed its own copy of the target ahead of the host's picture (the
       second layer predicts a light copy an agent touches): the reach adds the agent's travel over
       them. 0 for a contact claim. */
    float push_ticks;
} sync_contact_scene_t;

/* Host: a contact claim measured on what the host itself took of `seat`'s ownership of the
   agent (`ended` from sync_authority_check_contact, NULL for a current one), never on the
   velocity the owner declares. A contact lies on the path the taken states show: between two
   consecutive ones, or on from a state along the step that led to it (a contact turns a body
   only at the contact) for the ticks to the next one, or after the last for the ticks since it
   was taken plus slack. *gap is the agent's box from the target's along the nearest of those
   paths of the window; *reach is SYNC_POLICY_TAKE_SLOP_M, the target's travel over that time
   and SYNC_POLICY_CONTACT_BEND_MS2's bend of the path, the time at most
   SYNC_POLICY_EVIDENCE_AGE_MAX_S; *approach_max the agent's fastest travel of the window (at
   most max_speed) plus the target's speed. False when the host took no state of that ownership
   in the window: it has no evidence of the contact, and the claim is a missed effect. */
bool sync_authority_contact_measure(const sync_authority_t *a, body_sync_id_t agent, uint8_t seat, const sync_authority_ended_t *ended,
                                    const sync_contact_scene_t *scene, float *gap, float *reach, float *approach_max);
/* The same of the body `b` itself: a take's evidence asks it of each of the claimant's agents. */
bool sync_authority_path_measure(const sync_authority_t *a, const sync_authority_body_t *b, uint8_t seat, const sync_authority_ended_t *ended,
                                 const sync_contact_scene_t *scene, float *gap, float *reach, float *approach_max);
/* The number a contact claim of a local body names (body_sync_contact_number). */
bool sync_authority_contact_number(const sync_authority_t *a, const sync_authority_body_t *b, uint16_t *number);

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
