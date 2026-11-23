#ifndef BODY_SYNC_SYNC_SOFT_H
#define BODY_SYNC_SYNC_SOFT_H

/* The second layer (config light_mass_max): a guest simulates the host's light bodies itself
   while something of its own is at them, and holds them exactly on the host's pose when they
   rest. Pure C: per body, the pose, motion and contacts of this tick in; what the shell does to
   the body out. The host stays the one judge of these bodies: the guest sends nothing of them.

   A light copy of a free body (sync_authority_hold_kind) has two holds on a guest:
     rest hold  its owner rests (asleep there, or still for SYNC_SOFT_STILL_TICKS: a pile against
                a character that is driven every tick never sleeps) and nothing of this side's
                is at it: it is driven kinematic on the owner's pose like any copy, exact at rest,
                no jitter, outside every island, and keeps the character bits.
     soft       otherwise: dynamic with its own mass, simulated here. A body of this side's that
                moves (sync_soft_mover) marks every light copy within reach of its next steps, so
                a copy is soft before the contact, never a wall for one step.
   A soft copy is steered to the owner's playback pose, each apply in order:
     pause     while it touches a moving body this endpoint simulates (its own character, its own
               thrown body), and for the pause after the last such touch or a strong hit (the step
               changed its velocity by SYNC_SOFT_HIT_SPEED or its spin by SYNC_SOFT_HIT_SPIN beyond
               what this side's apply and gravity gave it): no pull, the local contact plays out.
               The pause is the measured round trip to the host plus the playback delay and a
               margin, at least SYNC_SOFT_PAUSE_TICKS: the host's answer lands before the pull
               starts. A chain of pauses runs at most SYNC_SOFT_PAUSE_MAX_TICKS; a touch that
               outlasts it no longer pauses until a tick without one, so a body hit over and over
               or pressed for ever is still taken back.
     snap      position error |e| past SYNC_SOFT_SNAP_M, paused or not: placed at the playback
               pose with the owner's velocities, the render offset taking the move.
     event     after this side's own throw (sync_replay.h) the host's snapshots of what it set
               going run behind this side's event time by the replay's shift: the host sends the
               thrown body back with this side's tick of each replayed step (sync_codec.h, the
               own-replay origin), which maps the host's ticks to this side's. While the owner
               moves, a soft copy is compared with its own state at the event time the playback
               shows, less what the layer itself changed since, and the error is closed in the
               present: never by steering it back along its own path, and a pull that runs long
               places nothing until the owner rests (its error is this side's own simulation
               against the host's, not a body stuck). An error past SYNC_SOFT_SNAP_M there means the outcomes differ: the
               copy snaps to the playback pose and follows the playback, compared now, until its
               owner rests. Before the playback reaches the copy's first snapshot (its own throw
               just handed back) nothing is compared: it keeps its simulation. A push of this side's
               character has no such shift (the host plays the character behind its own delay):
               it keeps the pause and the pull above.
     dead zone pulled only once the error passes SYNC_SOFT_DEAD_M or SYNC_SOFT_DEAD_RAD, until it
               is back under half of each: the hysteresis keeps a body near the edge from waking
               and stopping by turns.
     pull      v += SYNC_SOFT_BLEND * (v_owner + clamp(e / SYNC_SOFT_PULL_S, SYNC_SOFT_PULL_SPEED) - v),
               w likewise with SYNC_SOFT_PULL_SPIN; v_owner and w_owner are the playback's. A pull
               not done in SYNC_SOFT_STALL_TICKS (an unhindered one ends in about 20: a body pinned,
               dragged by friction or packed among others its owner no longer has) places it, the
               offset taking the move.
     speed     whatever moves it, its speed stays under the seam's max_speed and max_spin (a
               character driven by velocity can fling what it walks into).
   Nothing sets a soft copy's pose but the snaps and a stalled pull, so the guest's own contacts
   are never undone by a teleport; the change to the rest hold is a placement (the offset takes
   what the prediction left), and the layer never puts a body to sleep itself (that sleeps its
   whole island, this side's own bodies with it). */

#include <stdbool.h>
#include <stdint.h>

#include "sync_playback.h"

/* Dead zone: the stand's criterion at rest (1 cm, 2 degrees), above a copy record's 1/512 m grid
   and a 15-bit rotation, so a snapshot's rounding never wakes a body. */
#define SYNC_SOFT_DEAD_M 0.01f
#define SYNC_SOFT_DEAD_RAD 0.035f
/* A touch pauses the pull while the toucher moves faster than this (a walk, a throw); one that
   only leans on it does not. A body of this side's moving faster marks the copies near it. */
#define SYNC_SOFT_TOUCH_SPEED 0.25f
/* How far ahead a mover marks: its travel in this many steps, plus a margin. */
#define SYNC_SOFT_REACH_STEPS 3.0f
#define SYNC_SOFT_REACH_M 0.05f
/* Two kinematic copies closer than this touch for the marking's walk: Box3D's speculative margin. */
#define SYNC_SOFT_TOUCH_M 0.02f
/* A push against a copy in the rest hold, as against a wall: a body stopped from this speed. A
   body only leaning on it stops from nothing. */
#define SYNC_SOFT_LEAN_SPEED 0.05f
/* A strong hit: the sender's own "just hit" (1 m/s), and a spin change of a crate tumbling off. */
#define SYNC_SOFT_HIT_SPEED 1.0f
#define SYNC_SOFT_HIT_SPIN 4.0f
/* The host's answer to a guest's hit reaches the guest's playback one way, the host's step, one
   way and the guest's playback delay after it. The shell passes that, measured; this is its
   floor (also before the first measure) and the chain's cap, which bounds the pause too. */
#define SYNC_SOFT_PAUSE_TICKS 24u
#define SYNC_SOFT_PAUSE_MAX_TICKS 60u
/* The pull: a quarter second to close the error, no faster than a walk, blended into the body's
   own velocity at 0.3 a tick (about 0.05 s at 60 Hz), so what it meets on the way still pushes it
   while friction does not stall it short of its owner's pose. */
#define SYNC_SOFT_PULL_S 0.25f
#define SYNC_SOFT_PULL_SPEED 2.0f
#define SYNC_SOFT_PULL_SPIN 4.0f
#define SYNC_SOFT_BLEND 0.3f
/* Past a metre the prediction is wrong enough to be seen: placed, inside the render offset's 2 m. */
#define SYNC_SOFT_SNAP_M 1.0f
#define SYNC_SOFT_STALL_TICKS 30u
/* A body its owner moves this fast is in flight there and driven along the owner's path (the
   bounce off a wall stays exact); soft again under SYNC_SOFT_LAND_SPEED. Above a walk and a
   push, below a throw. */
#define SYNC_SOFT_FLIGHT_SPEED 4.0f
#define SYNC_SOFT_LAND_SPEED 1.0f
/* An owner whose playback pose stays within SYNC_SOFT_HOLD_M and SYNC_SOFT_HOLD_RAD of where it
   was SYNC_SOFT_STILL_TICKS ago rests even awake: a quarter second within a step of the copy
   grid, which a still owner's rounded pose crosses now and then. */
#define SYNC_SOFT_STILL_TICKS 15u
/* A copy in the rest hold of a still but awake owner follows it only past this: a copy record's
   grid (1/512 m) and a step of its rotation, which a still owner's rounded pose wanders by. */
#define SYNC_SOFT_HOLD_M 0.002f
#define SYNC_SOFT_HOLD_RAD 0.004f

typedef struct sync_soft {
    bool correcting;    /* past the dead zone and not yet back under half of it */
    uint16_t pause;     /* ticks of pause left */
    uint16_t chain;     /* ticks since the pause's first trigger, while it lasts */
    uint16_t stall;     /* ticks this pull has run */
    /* Its outcome and the owner's differed at event time: it was set on the owner's playback
       pose and follows that, compared now, until the owner rests. */
    bool apart;
    float velocity[3];  /* what the apply left the body with, to tell a hit from the pull */
    float angular[3];
} sync_soft_t;

typedef struct sync_soft_in {
    float position[3], rotation[4]; /* the body now */
    float velocity[3], angular[3];
    bool asleep;
    bool touches_local;   /* in contact with a moving body this endpoint simulates */
    /* The playback pose against this copy's own state at the same event time (sync_event_lag):
       its error, turn and velocity differences. Known, the pull and the snap close these
       instead of the gap to the playback pose now. */
    bool aligned;
    float aligned_error[3], aligned_turn[3], aligned_dv[3], aligned_dw[3];
    /* Event time known but the playback not yet at this copy's first snapshot (this side's own
       throw just handed back): there is nothing to compare, and the copy keeps its simulation. */
    bool waiting;
    bool owner_rests; /* sync_soft_owner_rests */
    float max_speed, max_spin;
    uint16_t pause_ticks; /* the pause after a touch or hit; 0 for SYNC_SOFT_PAUSE_TICKS */
} sync_soft_in_t;

typedef enum sync_soft_action {
    SYNC_SOFT_NONE = 0, /* leave the body to its simulation */
    SYNC_SOFT_VELOCITY, /* set velocity and spin to the output's */
    SYNC_SOFT_PLACE,    /* place at the output's pose with its velocities */
} sync_soft_action_t;

typedef struct sync_soft_out {
    sync_soft_action_t action;
    float position[3], rotation[4]; /* PLACE: where */
    float velocity[3], angular[3];
} sync_soft_out_t;

typedef struct sync_soft_still {
    uint16_t ticks;     /* since the owner's pose last left the anchor */
    float anchor[3], anchor_rotation[4];
} sync_soft_still_t;

/* A body's own states by local tick, for comparing at event time. */
/* What a comparison at event time looks back: the host line's one-way and playback delay plus the
   replay's shift (one way back). 64 covers a 49-tick (816 ms) round trip at the default delay_max
   of 15; past it the state is not kept and the copy keeps the pause and the pull. */
enum { SYNC_HISTORY_TICKS = 64 };
typedef struct sync_history {
    bool known[SYNC_HISTORY_TICKS];
    uint32_t tick[SYNC_HISTORY_TICKS];
    float position[SYNC_HISTORY_TICKS][3];
    float rotation[SYNC_HISTORY_TICKS][4];
    float velocity[SYNC_HISTORY_TICKS][3];
    float angular[SYNC_HISTORY_TICKS][3];
    /* What the layer itself changed at each apply (a placement's move, a pull's velocity): a
       state kept before it does not show it yet. */
    bool corrected[SYNC_HISTORY_TICKS];
    uint32_t corrected_tick[SYNC_HISTORY_TICKS];
    float moved[SYNC_HISTORY_TICKS][3];
    float dv[SYNC_HISTORY_TICKS][3];
    float dw[SYNC_HISTORY_TICKS][3];
} sync_history_t;
void sync_history_put(sync_history_t *h, uint32_t tick, const float position[3], const float rotation[4], const float velocity[3], const float angular[3]);
/* The state at `tick`, between the two steps around it (linearly; the rotation on the shorter
   arc); false when either is not kept. */
bool sync_history_at(const sync_history_t *h, double tick, float position[3], float rotation[4], float velocity[3], float angular[3]);

/* The layer's own change at this apply's `tick`. */
void sync_history_correct(sync_history_t *h, uint32_t tick, const float moved[3], const float dv[3], const float dw[3]);
/* An error found at event time `at` against this copy's state then, carried to `now`: less what
   the layer itself changed since (a move, and a velocity change times the time it has run), so
   a correction is never applied twice while its effect has yet to reach the compared past. */
void sync_history_since(const sync_history_t *h, double at, uint32_t now, float dt, float error[3], float turn[3], float dv[3], float dw[3]);

/* A shift serves this long after the thrower last heard it: the pause, and what the throw set
   going coming to rest. */
#define SYNC_EVENT_LAG_TICKS 300u
/* The longest round trip (ticks) at which the replay's 6-bit tick still names one step: the
   record comes back within it plus the host's wait to start and the jitter, under 64. */
#define SYNC_EVENT_RTT_MAX 48u

/* Whether the owner rests for its copy: asleep there, or still for SYNC_SOFT_STILL_TICKS. */
bool sync_soft_owner_rests(sync_soft_still_t *still, const sync_play_t *play);

/* The rest hold: the owner rests, the copy is out of a pause and nothing of this side's moves
   at it. */
bool sync_soft_rest_hold(bool owner_rests, const sync_soft_t *s, bool soft_now, bool marked);

/* Whether a body of this side's (local, or a soft copy) marks the copies near it, and how far:
   true when a point of it moves faster than SYNC_SOFT_TOUCH_SPEED, its speed plus its spin times
   `extent` (its farthest point from the centre: a long body spinning, a character turning);
   `reach` is that travel in SYNC_SOFT_REACH_STEPS plus SYNC_SOFT_REACH_M. */
bool sync_soft_mover(const float velocity[3], const float angular[3], float extent, float dt, float *reach);

/* Whether a body of this side's pushed a copy in the rest hold in the last step: the contact's
   normal impulse is more than stopping `pusher_mass` at SYNC_SOFT_LEAN_SPEED. The game moves its
   bodies after the apply, so a push from standstill is seen only here, after the step it was
   stopped in; the copy is soft for the next one, a wall for one step at most. */
bool sync_soft_pushed(float normal_impulse, float pusher_mass);

/* The pause in ticks: the round trip to the host as the packet fates measure it, the playback
   delay of the host's snapshots, and 2 (the host's step and the acknowledgement's wait), within
   SYNC_SOFT_PAUSE_TICKS and SYNC_SOFT_PAUSE_MAX_TICKS. */
uint16_t sync_soft_pause(float round_trip_ticks, uint32_t delay_ticks);

/* A body that becomes a soft copy: from local (a release, a settled throw, a refused claim) it
   starts paused, so the handover never pulls it toward the older playback pose; from a driven
   or corrected hold it starts correcting at once. */
void sync_soft_enter(sync_soft_t *s, bool from_local, uint16_t pause_ticks);

/* After the step: whether it hit the body, from its velocities then against those the apply left
   it (`gravity_dv` is what gravity alone gave it in the step). */
bool sync_soft_hit(const sync_soft_t *s, const float velocity[3], const float angular[3], const float gravity_dv[3]);

/* One apply of a soft copy whose playback sample is known. */
void sync_soft_decide(sync_soft_t *s, const sync_soft_in_t *now, bool hit, const sync_play_t *play, sync_soft_out_t *out);

#endif
