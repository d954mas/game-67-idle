#ifndef BODY_SYNC_SYNC_POLICY_H
#define BODY_SYNC_SYNC_POLICY_H

/* The authority policy: every handover the caller does not ask for. Pure C, no Box3D: the shell
   gathers a body's contacts and motion, sync_authority asks these calls and moves the body as they
   say. Two policies share the calls, chosen by body_sync_config_t.authority:

     host         (BODY_SYNC_AUTHORITY_HOST) the host owns every free body. A body a guest threw
                  or dropped (a hold until settled) goes back to the host at its first touch of a
                  body that is not static, when it sleeps, or after SYNC_POLICY_AGE_TICKS of flight.
     interaction  (BODY_SYNC_AUTHORITY_INTERACTION) the seat whose action sets a light body moving
                  owns it, and through it whatever that body sets moving, until they rest; the
                  others show it through playback. A take is optimistic: the guest simulates the
                  body from its contact on, and the host grants or refuses the claim itself (it
                  never reaches body_sync_poll_claim). A seat's agents are its character, the
                  bodies it carries or threw and the bodies it owns this way; an agent takes a
                  light body it touches while moving faster than a lean, or pushes. Conflicts:
                  the host first (its agents take back what they touch, and what the host's agents
                  set going no guest may take), then the earliest confirmed toucher; a guest never
                  takes another guest's body, and where two guests' bodies touch the host takes
                  both. The loser's body turns into a copy the second layer
                  pulls toward the winner's (sync_soft.h).

   "Light" is class LIGHT at most light_mass_max kilograms, not kinematic at its owner, carried by
   nobody. */

#include <stdbool.h>
#include <stdint.h>

typedef enum sync_policy_mode {
    SYNC_POLICY_HOST = 0,
    SYNC_POLICY_INTERACTION,
} sync_policy_mode_t;

/* Who wins when the host's agent touches a body a guest owns by interaction. */
typedef enum sync_policy_priority {
    SYNC_POLICY_HOST_FIRST = 0, /* the host takes it back: its push shows at once on the host */
    SYNC_POLICY_EARLIEST,       /* the guest keeps it; the host's push reaches it through the guest */
} sync_policy_priority_t;
#define SYNC_POLICY_PRIORITY SYNC_POLICY_HOST_FIRST

/* An agent moving faster than this at its contact (its speed plus spin times its extent) takes;
   one only leaning on a body does not, so a character standing in a pile never owns it. The second
   layer's touch speed (asserted in sync_policy.c). */
#define SYNC_POLICY_AGENT_MM_S 250
#define SYNC_POLICY_AGENT_SPEED ((float)SYNC_POLICY_AGENT_MM_S / 1000.0f)
/* A body still within these of one pose for this many ticks is quiet though awake (a pile leaning
   on a character that is driven every tick never sleeps): the second layer's stillness. */
#define SYNC_POLICY_QUIET_TICKS 15u
#define SYNC_POLICY_QUIET_M 0.002f
#define SYNC_POLICY_QUIET_RAD 0.004f
/* Bounds, README "Authority follows interaction" derives them: bodies a guest owns by interaction
   at once, takes and releases a guest sends in one tick, and ticks a body stays owned after its
   last agent contact while it never quiets. */
#define SYNC_POLICY_OWNED_MAX 64u
#define SYNC_POLICY_TAKES_PER_TICK 16u
#define SYNC_POLICY_RELEASES_PER_TICK 16u
#define SYNC_POLICY_AGE_TICKS 600u
/* The host's own budget: a seat's takes beyond 16 a tick are refused, the unused budget of up to
   8 ticks kept for takes a hitch of the record lane delivers at once. */
#define SYNC_POLICY_TAKE_BURST_TICKS 8u
/* A take's evidence on the host: one of the claimant's agents within reach of the body, judged
   when the claim arrives, before that tick's apply. The claim and the contact's state cross the
   same one-way trip, so the agent's copy trails the contact by a playback delay and a tick; a
   claim the record lane resends arrives a round trip and more late, the copy then that much less
   the delay ahead of the contact. The body the guest touched was its copy of the host's body, a
   one-way trip and a playback delay old, and the claim takes the way back: the host's body is a
   round trip, a delay and a tick on. Both stay within a round trip, a playback delay and a tick
   of travel (sync_policy_take_reach; the guest's delay taken as the host's own). The slop
   is what that leaves: a contact starts at Box3D's 2 cm speculative distance, a pose rounds to
   the copy grid (a 3.4 mm cell diagonal) and its turn by up to 0.0195 rad (1 cm at the corner of
   a 0.5 m body), 0.035 m in all. Over the stand's authority and layer scenes at 100-300 ms no
   granted take's gap passed its travel, and the nearest refusal passed it by 0.26 m: 0.15 m is
   four times the derived bound and stays inside that empty band. */
#define SYNC_POLICY_TAKE_SLOP_M 0.15f

/* A thrown or dropped body's flight (a hold until settled), after a step. One that never sleeps and
   meets nothing but static bodies (rolling on a floor) goes back after SYNC_POLICY_AGE_TICKS of
   flight, as a body owned by interaction does with no agent on it: no throw owns a body for ever. */
typedef enum sync_flight {
    SYNC_FLIGHT_ON = 0,    /* keeps flying under its thrower */
    SYNC_FLIGHT_RETURNS,   /* back to the host (on the host: the hold ends) */
    SYNC_FLIGHT_INTERACTS, /* stays its thrower's as a body owned by interaction (hold until rest) */
} sync_flight_t;

typedef struct sync_policy_flight_in {
    sync_policy_mode_t mode;
    bool asleep;
    bool touched_body;       /* began touching a body that is neither static nor its last holder */
    bool touched_untakable;  /* ...one of which this seat may not take (heavy, a character, owned elsewhere) */
    uint32_t age;            /* ticks in a row this side has flown it */
} sync_policy_flight_in_t;
sync_flight_t sync_policy_flight(const sync_policy_flight_in_t *in);

/* A body the shell found touched by one of this side's agents. */
typedef struct sync_policy_take_in {
    sync_policy_mode_t mode;
    bool host;           /* this side is the host: a take marks its own body host-active */
    bool light;
    bool local;          /* this side simulates the body already */
    uint8_t owner_seat;  /* the body's seat as this side knows it */
    uint8_t owner_hold;  /* body_sync_hold_t, the owner's */
    bool held;           /* a holder is named */
    bool owner_kinematic;
    float agent_speed;   /* the agent's speed at the contact, m/s */
    bool pushed;         /* the contact's impulse is more than stopping the agent from a lean */
    uint32_t owned;      /* bodies this side owns by interaction now */
    uint32_t taken;      /* takes this side sent this tick */
    bool returned_pressed; /* this side gave it back while one of its characters or the bodies they
                              carry pressed it (a contact that acted), and that press has not ended */
} sync_policy_take_in_t;
bool sync_policy_takes(const sync_policy_take_in_t *in);

/* Host: a guest's take of a body. Granted or refused; the claim never reaches the caller. */
typedef struct sync_policy_grant_in {
    sync_policy_mode_t mode;
    bool light;
    uint8_t claimant;
    uint8_t seat;        /* the body's seat now */
    uint8_t hold;        /* the body's hold on the host */
    bool held;
    bool kinematic;      /* on the host, as last read or received */
    uint32_t owned;      /* bodies the claimant owns by interaction now */
    float budget;        /* takes the claimant may still make (SYNC_POLICY_TAKE_BURST_TICKS) */
    float agent_gap;     /* metres from the body to the claimant's nearest agent on the host */
    float agent_reach;   /* how far apart they may be after a real touch (sync_policy_take_reach) */
} sync_policy_grant_in_t;
bool sync_policy_grants(const sync_policy_grant_in_t *in);
/* The reach of a take's evidence: SYNC_POLICY_TAKE_SLOP_M plus the agent's and the body's travel
   (m/s) over `age_s`, the time the host's copy of the agent trails the guest. */
float sync_policy_take_reach(float agent_speed, float body_speed, float age_s);
/* The longest the host's evidence may trail a guest, seconds. A guest that acknowledges late
   raises its measured round trip, and with it a take's and a contact's reach and a contact
   claim's grace: past this neither grows. Above the stand's slowest honest path (a 300 ms round
   trip, the default longest delay of 15 ticks at 60 Hz and a tick: 0.57 s). */
#define SYNC_POLICY_EVIDENCE_AGE_MAX_S 1.0f
/* Ticks the host's copy of a guest's agent trails that guest: a round trip, a playback delay
   and a tick, at most SYNC_POLICY_EVIDENCE_AGE_MAX_S. */
float sync_policy_evidence_ticks(float round_trip_ticks, float delay_ticks, uint32_t tick_hz);
/* Ticks the host keeps a slot it unbound from a generation equal modulo 16 to the one it held
   (sync_authority_bind): the evidence cap and the longest playback delay. A record the old owner
   sent before it heard of the unbind arrives within that on a path whose round trip is inside the
   cap, so the real seam's 4 generation bits never read it as the new body's. */
uint32_t sync_policy_rebind_ticks(uint32_t tick_hz);
/* The sender ticks between two states the host keeps of an agent past its ring (`poses` of them):
   enough to span SYNC_POLICY_EVIDENCE_AGE_MAX_S and a chord's spacing, at least one. Each kept
   state extends along its own step for at most SYNC_POLICY_CHORD_TICKS_MAX ticks, so up to that
   stride (252 Hz with 64 states) every contact tick lies on from one of them; a higher rate leaves
   gaps, contacts missed, never forged. */
uint32_t sync_policy_agent_stride(uint32_t tick_hz, uint32_t poses);
/* How far a body's own path may bend from its straight line before its contact, m/s^2: gravity
   (the stand's 12) with room for a game's stronger one and drag. It applies only where the host
   saw the path: over a short chord (4 ticks: 4.4 cm) and over the slack after the last state (a
   playback delay and a tick: 0.14 m at 7 ticks). */
#define SYNC_POLICY_CONTACT_BEND_MS2 20.0f
/* The longest spacing, ticks, of two taken states whose chord still shows the path between them.
   A longer one is a gap in the host's evidence: its two states count only as points, never as a
   path grown over the gap. At 25 % loss three states lost in a row come once in 64; an awake body
   slower than SYNC_FAST_SPEED goes out every 1-4 ticks and moves at most 3.4 cm between two of its
   states (sync_sender.h), inside the slop of its points. */
#define SYNC_POLICY_CHORD_TICKS_MAX 4u
/* How late a contact claim or a take may reach the host after a state it rests on, in the
   claim's graces (a round trip, a playback delay and a tick; at most
   SYNC_POLICY_EVIDENCE_AGE_MAX_S): one for its own trip, one for the record lane resending it.
   The host's own ticks measure it, never the claim's stamp. Over the claims sweep the latest
   honest claim came 1.33 graces after its state (two resends at 25 % loss); a take 8 ticks. */
#define SYNC_POLICY_CLAIM_LAG_GRACES 2u
/* The distance between two axis-aligned boxes given by their low and high corners, 0 where
   they overlap: the evidence's gap. INFINITY for a corner that is not finite, so a broken pose
   never reads as touching. */
float sync_policy_box_gap(const float lo[3], const float hi[3], const float other_lo[3], const float other_hi[3]);

/* Host: one of its agents touches a copy a guest owns by interaction. */
typedef struct sync_policy_reclaim_in {
    sync_policy_mode_t mode;
    sync_policy_priority_t priority;
    uint8_t owner_seat;
    uint8_t owner_hold;
    float agent_speed;
    bool pushed;
} sync_policy_reclaim_in_t;
bool sync_policy_host_reclaims(const sync_policy_reclaim_in_t *in);

/* Host: two copies touch, each owned by interaction (`hold`, `other_hold`) by its seat. */
typedef struct sync_policy_contest_in {
    sync_policy_mode_t mode;
    uint8_t seat, other_seat;
    uint8_t hold, other_hold;
} sync_policy_contest_in_t;
bool sync_policy_host_takes_contested(const sync_policy_contest_in_t *in);

/* A body this side owns by interaction (a hold until rest), after a step. */
typedef struct sync_policy_rest_in {
    bool host;            /* the host ends its hold; a guest releases the body to the host */
    bool quiet;
    bool neighbours_quiet; /* every body of this side's owned by interaction it touches is quiet */
    bool rest_acked;      /* guest: the host acknowledged the rest pose and every rare group */
    uint32_t age;         /* ticks since its last agent contact or its take */
    bool out_of_box;
    uint32_t released;    /* releases this side sent this tick */
    float round_trip_ticks; /* host: the farthest joined guest's measured round trip, 0 unknown */
} sync_policy_rest_in_t;
bool sync_policy_returns_at_rest(const sync_policy_rest_in_t *in);

/* The host's hysteresis: it ends its own hold only once its agents have been off the body this
   long. An agent leaving at the slowest speed that marks (SYNC_POLICY_AGENT_SPEED) stays inside
   the marking walk's reach of the body (three steps of its travel plus 5 cm, sync_soft.h) for
   15 ticks, so the same walk may touch it again without having left. A guest's take sent on the
   end's first state reaches the host a round trip later, and a touch between the end and that
   take's arrival makes the host refuse it: the hold outlasts the walk's reach by the farthest
   guest's round trip, so a walk's second touch inside both finds one hold still on. */
#define SYNC_POLICY_HOST_CLEAR_TICKS 15u
uint32_t sync_policy_host_hold_ticks(float round_trip_ticks);

/* Stillness: asleep, or within SYNC_POLICY_QUIET_M and SYNC_POLICY_QUIET_RAD of one pose for
   SYNC_POLICY_QUIET_TICKS ticks. */
typedef struct sync_policy_still {
    uint16_t ticks;
    float anchor[3], anchor_rotation[4];
} sync_policy_still_t;
bool sync_policy_quiet(sync_policy_still_t *still, const float position[3], const float rotation[4], bool asleep);

#endif
