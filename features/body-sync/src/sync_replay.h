#ifndef BODY_SYNC_SYNC_REPLAY_H
#define BODY_SYNC_SYNC_REPLAY_H

/* The host's replay of a guest's throw. Pure C: the owner's newest flight state and this side's
   ticks in; what the shell does to the body out.

   A body a guest threw or dropped (its hold until settled, no holder, not kinematic there) flies
   free until its first hit, and its flight follows from one state alone. So the host does not
   play it from the delayed buffer: from the first flight state it receives (the release, or the
   next one when that packet was lost) it places its own body there with that state's velocity
   and spin, once, and simulates it freely. A resting pile is the same in both worlds, so the same
   start gives the same impact and the same outcome, a fixed shift later: the host's step h
   replays the owner's step h - shift, shift = the apply's tick - the state's tick - 1 (0 when the
   state arrives in the next tick).

   Later flight records do not steer it. Each newer one is checked against this side's own pose
   after the step it maps to; one off by more than SYNC_REPLAY_MISMATCH_M (the owner's body was
   stopped or turned by something this world lacks) restarts the replay from it, the render
   offset taking the move. After this side's body touched a body (not a static one) here, nothing
   is checked: the outcome is this world's, and the owner's later records describe a flight that
   no longer happened here. The replay ends when the owner's flight ends (a hold that settled, a
   holder, a kinematic owner) or when the body becomes local here; a body taken over keeps its
   own simulation. */

#include <stdbool.h>
#include <stdint.h>

enum {
    SYNC_REPLAY_HISTORY = 64, /* own poses kept, a tick each: a round trip of a second */
};
/* Far beyond a copy grid's rounding and the quantized start's drift over a second of flight
   (velocity at 1/32 m/s: about 1.5 cm), far below what a different contact does. */
#define SYNC_REPLAY_MISMATCH_M 0.25f

typedef struct sync_replay {
    bool active;
    bool landed;     /* touched a body here: no more checks */
    int32_t shift;   /* this side's tick minus the owner's tick of the same step */
    uint32_t newest; /* the owner's newest flight state seen */
    uint32_t restarts;
    bool known[SYNC_REPLAY_HISTORY];
    uint32_t ticks[SYNC_REPLAY_HISTORY]; /* the local tick each pose was taken after */
    float history[SYNC_REPLAY_HISTORY][3];
} sync_replay_t;

typedef enum sync_replay_action {
    SYNC_REPLAY_OFF = 0, /* not replayed: the shell holds it as before */
    SYNC_REPLAY_START,   /* place at the owner's state with its velocities, then simulate */
    SYNC_REPLAY_FREE,    /* leave it to this world's simulation */
} sync_replay_action_t;

/* One apply of a copy whose owner flies it. `usable`: the owner's newest state is a moving sim
   record (pose and velocities) of tick `sample`, at `position`. */
sync_replay_action_t sync_replay_decide(sync_replay_t *r, uint32_t local_tick, bool usable, uint32_t sample, const float position[3]);
/* After this side's step of `local_tick`: its pose, and whether it touched a non-static body. */
void sync_replay_stepped(sync_replay_t *r, uint32_t local_tick, const float position[3], bool touched_body);
void sync_replay_stop(sync_replay_t *r);

/* A guest's release of a body it throws is the state the host replays from: there this side's
   own body takes the value the wire carries, so both worlds go on from the same bits. At no other
   tick, since rounding every step would walk the motion; a rest pose is sent as it is and the
   copy takes it. */
typedef struct sync_round {
    bool flight; /* the current flight was rounded */
} sync_round_t;
/* A local body after the step: `flying` held until settled with no holder, dynamic, on a guest.
   True when this collect rounds it. */
bool sync_round_due(sync_round_t *r, bool flying);

#endif
