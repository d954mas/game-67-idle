#ifndef FEATURES_BODY_SYNC_BODY_SYNC_METRICS_H
#define FEATURES_BODY_SYNC_BODY_SYNC_METRICS_H

#include <stdbool.h>
#include <stdint.h>

#include "features/body_sync/body_sync_scenario.h"

#ifdef __cplusplus
extern "C" {
#endif

/* What a stand run measured, as a struct for tests and as JSON (schema "body_sync.metrics.v2",
   written by body_sync_stand_write_json). The JSON holds nothing that differs between two runs
   of one scenario with one seed on one build: no clock, no path, no address, so two such runs
   give byte-equal files.

   Definitions, all on the stand's fixed tick (headless, a frame is a tick):
   - divergence: for each guest and each body bound in both worlds, the distance between the
     guest's drawn position and the host's physics position at the same tick. It includes the
     copy's latency by design (what that guest sees against the truth), so a moving body shows
     divergence however good the sync is: it is not the sync's error, and it grows with the
     receiver's playback delay by design (a body at speed v shows v times the delay more).
   - the playback measures, for each copy whose owner sends to it directly (the host's bodies
     on a guest, a guest's on the host; a guest's body relayed to another guest plays on a
     clock the stand cannot map back and is left out), at the owner tick P the copy shows
     (body_sync_playback):
       lag-aligned divergence  |drawn(t) - owner(P)|, the owner's own position at P: what the
                               sync gets wrong once its latency is taken out
       path error              the distance from drawn(t) to the polyline of the owner's
                               positions over owner ticks [P - 30, P + 30]: a cut corner, free of
                               any time shift
       behind                  the owner's newest tick less P, in ticks: how late a copy
                               plays (a frozen or stuck clock shows here)
       steps                   body-ticks where the drawn move exceeds 1.25 times the owner's
                               move over the same playback interval plus 1 cm; those while the
                               copy's render offset is giving a move back are offset steps,
                               counted apart (a slide by design)
       jump                    the drawn move less the owner's move over that interval, largest;
                               a move past 2 m (the render offset's reach) is a snap, counted
                               apart and not a jump
     Each is attributed to the guest on the path: the copy's endpoint, or the owner when the
     copy is on the host. A copy the second layer predicts (body_sync_is_predicted) is left out:
     its drawn pose is not its playback, and the layer's own measures below watch it. A check of any of them on a guest with no copy measured fails: no
     sample is no evidence.
   - jerk: for each copy the guest draws, |d(t) - d(t-1)| where d(t) is the drawn position's
     move over tick t: a sudden change of motion. Bodies at rest on both ticks are skipped.
   - own delay: ticks from a player's action on its own endpoint until the host's world shows
     it (host_ticks), until the host's answer shows on the actor's world (echo_ticks), and
     until the last other endpoint shows it (seen_ticks). Zero for what is local already.
     host_ticks and seen_ticks count an action that produced something to see: a walk that
     moved its character, a grab or a hit that sent a claim (or, on the host, took effect), a
     throw or a drop. echo_ticks counts only what the host answers: a guest's grab claim and a
     guest's hit claim, set when the answer shows on the actor. A wait, a walk that started at
     its target, a failed grab and a missed punch are in none of the three. A counted action
     never seen within 10 s reads -1 in the JSON and is unbounded for a metric check, which then
     fails and names its label.
   - bytes per second: payload handed to the session, per direction and kind (the seam's kinds
     and the stand's records), mean over the run and the peak one-second window. The session's
     own header bytes are not in them. link_estimate_bytes_per_s is the mean of the link's own
     smoothed rate estimate (net_link_stats), sampled each tick: it lags and is not a count, so
     it may read below the payload rate.

   Metric names for `expect <metric> <= <value>` (worst guest; unit in brackets):
     divergence.max divergence.p95 divergence.mean [m]   rotation.max [deg]
     jerk.max jerk.p95 [m]                                jerk.frames_over [frames]
     host_ticks.max echo_ticks.max seen_ticks.max [ticks]
     bytes_per_s.to_guest bytes_per_s.to_host [bytes/s, mean, all kinds]
     peak_bytes_per_s.to_guest peak_bytes_per_s.to_host [bytes/s]
     stale_ticks.max [ticks]: the most ticks an awake body went between two state packets
       that carried it, either direction (a budgeted sender's starvation; 0 for the stub)
     deferred.count [records]: records due that a packet's budget left out, both directions
     lag_divergence.max lag_divergence.p95 path_error.max path_error.p95 jump.max [m]
     step.count offset_step.count snap.count [body-ticks]   behind_ticks.max behind_ticks.p95 [ticks]
     response_ticks.max response_ticks.p95 [ticks]   response_misses.count [hits]   rest_jitter.max [m]
     rest_wakes.count [body-ticks]   rest_breaks.count [breaks]   host_wakes.count [wakes]   rest_lag_ticks.max [ticks]   soft_places.max [placements]
     carry_gap.max [m]: the farthest the host shows a body this guest's character carries from
       that character's hold point as the host shows it (a guest with no carry fails a check)
     pause_window.max [ticks]: the most ticks one copy's pull was paused in any 120 it was drawn
       (twice the layer's chain cap; the cap bounds it by (120 + 60) / 2, sync_soft.h)
     aligned_gap.max aligned_gap.p95 local_correction.max [m]   local_turn.max [deg]
     takes.count take_refusals.count reclaims.count returns.count [bodies]
     claims.count claims_overflow.count claims_repeat.count claims_rejected.count claims_idle.count
     claim_effects.count claims_format.count [claims]
   - claims: a guest's hit and impact claims the host parsed. Each lands in exactly one of
     overflow (its seat's queue for the tick was full), repeat (an impact key judged before),
     rejected (the seam's gate said STALE or REVOKED, or the stand's limits or rules refused it:
     a gap past the reach, a claimed speed past the approach the host saw, a hit out of range),
     idle (accepted, nothing left to do: a target already broken or felled) and effects.
     claims_format counts the guest's stand records the parser refused (dropped).
   - lag-aligned gap: two pictures of one moment, while the body moves. A copy an endpoint plays
     (a guest the host's bodies, the host a guest's) against what its owner drew at the owner tick
     the copy shows, a body a guest predicts (the second layer) too once its pull runs: the snap
     places it within 1 m of that pose and its render offset adds at most 2 m. It takes the
     latency out of divergence for every way a body is shown, so it compares the policies. A
     prediction in its pause is this guest's own answer to its touch, drawn ahead of the playback
     tick, and is measured apart, reported only:
     prediction_gap: a pause that began with its owner at rest, against the host's picture when the
     host's playback of that guest's character reaches the guest's tick; past the contact the two
     simulations differ by chaos, not lateness.
     flight_pause_gap: a pause that began while its owner moved the body, which shares no such
     moment, against the owner at the playback tick; it holds the pause's lead on that tick. A
     third seat's body plays a relayed clock and is left out of all three.
   - local correction: what the seam moved a body a guest simulates (its character excepted)
     beyond its own step, its move at the apply plus the change of its render offset: the
     thrower's own corrections; local_turn the same of its rotation, the offset's turn after the
     seam's turn at the apply.
   - local response: a hit is a contact that begins, in a guest's step, between a body that guest
     simulates (its character, or a body under its authority) and a LIGHT copy on its world whose
     body on the host then moves 1 cm or more within 60 ticks to a pose 1 cm or more from where
     the guest drew the copy at the hit (the hit was real; a host catching up to a push the guest
     showed before the hit is no hit of the host's). Its response is
     the guest's ticks from that step until the copy's drawn position there is 1 mm or more from
     where it was drawn at the step (a copy waiting for the host does not move at all), if that
     comes within 24 ticks; later it is a miss, counted in response_misses (the pull moved it, not
     the hit). Only contacts that raise a hit event count, not grazes. A guest with no hit fails
     a check.
   - rest: over body-ticks where every copy and its host body have rested (asleep, or slower
     than 5 cm/s, the run's own rest) for the last 30 ticks and nothing this guest simulates touches it, rest_jitter
     is the largest drawn move in a tick of a copy whose host body sleeps (one kept awake, against a
     character, is followed on the copy grid) and rest_wakes counts the ticks a copy starts to move;
     rest_breaks counts the times a guest's whole world, once it had rested 30 ticks after the
     scenario's last action, stopped resting (a copy woken, moved or drawn off its host body);
     a rest that ends only because host bodies moved (a pile creeping against a character slips)
     is the host's physics and counts in host_wakes instead: so does a copy that moves because its
     host body's path, at the owner ticks the copy showed over the last 30, left a copy-grid step
     within 8 ticks (the seam's own test of a move, which takes the copy out of the rest hold);
     creep_max (reported only) is the
     fastest an awake host body moved while the world rested. A driven (kinematic) body rests by
     its speed over 15 ticks: a copy of a still owner steps on the copy grid;
     with no such body-tick both checks fail. rest_lag_ticks is the longest run of ticks a copy
     moved while its host body rested and nothing of the guest's touched it for 60 ticks (a copy
     that never comes to rest). soft_places counts, per predicted copy, the placements its render
     offset took (a stalled pull, a snap): a copy pinned and placed over and over shows there.
     copy_speed_max is reported only.
   - flush_over_limit: ticks where one path's state packet and that tick's seam records
     together pass the session's largest state payload: a transport that packs one flush into
     one datagram would split them there. */

#define BODY_SYNC_METRICS_SCHEMA "body_sync.metrics.v2"
/* A drawn move that changes by more than this between two ticks counts in jerk.frames_over. */
#define BODY_SYNC_JERK_OVER_M 0.05f

/* The stand's own record kinds, counted beside the seam's (body_sync_traffic_kind_t). */
typedef enum body_sync_stand_kind {
    BODY_SYNC_STAND_KIND_CLAIM = 0, /* hit and impact claims from a guest */
    BODY_SYNC_STAND_KIND_VERDICT,   /* the host's answers: refusal, knockdown, rejected hit */
    BODY_SYNC_STAND_KIND_SPAWN,
    BODY_SYNC_STAND_KIND_REMOVE,
    BODY_SYNC_STAND_KIND_COUNT,
} body_sync_stand_kind_t;
/* "claim", "verdict", "spawn", "remove"; "unknown" past the last. */
const char *body_sync_stand_kind_name(body_sync_stand_kind_t kind);

/* Seam kinds first, then the stand's: the JSON's per-kind keys in this order. The seam's count
   is repeated here to keep this header free of Box3D; the stand asserts it statically against
   BODY_SYNC_TRAFFIC_COUNT. */
enum {
    BODY_SYNC_SEAM_KINDS = 5,
    BODY_SYNC_KINDS_ALL = BODY_SYNC_SEAM_KINDS + BODY_SYNC_STAND_KIND_COUNT,
};

typedef struct body_sync_bytes {
    double mean_bytes_per_s[BODY_SYNC_KINDS_ALL];
    double peak_bytes_per_s[BODY_SYNC_KINDS_ALL];
    uint64_t total_bytes[BODY_SYNC_KINDS_ALL];
    double link_estimate_bytes_per_s; /* the link's own lagging estimate, not a count; -1 unknown */
} body_sync_bytes_t;

typedef struct body_sync_guest_metrics {
    uint8_t seat;
    double divergence_max_m, divergence_p95_m, divergence_mean_m;
    double rotation_max_deg;
    double jerk_max_m, jerk_p95_m;
    uint32_t jerk_frames_over;
    double lag_divergence_max_m, lag_divergence_p95_m;
    double path_error_max_m, path_error_p95_m;
    uint32_t step_count, offset_step_count, snap_count;
    double jump_max_m;
    /* The lag-aligned gap: each picture of a moving body against its owner's own picture at the
       owner tick it shows (README "Metrics"); local_correction: what the seam moved a body this
       guest simulates beyond its own step (the thrower's corrections), an offset left from its
       time as a copy decaying uncounted. */
    double aligned_gap_max_m, aligned_gap_p95_m;
    uint32_t aligned_samples;
    /* Reported only: a prediction in a pause that began with its owner at rest against the host's
       answer at the guest's moment (README "Metrics"). Two simulations of one contact: past it
       they differ by chaos, which no bound derives. */
    double prediction_gap_max_m, prediction_gap_p95_m;
    /* Reported only: a prediction in a pause begun on a moving owner (no shared moment) against the
       owner at the playback tick it is drawn ahead of. */
    double flight_pause_gap_max_m, flight_pause_gap_p95_m;
    double local_correction_max_m;
    double local_turn_max_deg; /* its turn: the same correction of the body's rotation */
    /* Authority following interaction: the guest's takes the host granted and refused, its bodies
       the host's agents took back, and those it returned at rest. */
    uint32_t takes, take_refusals, reclaims, returns;
    /* Its takes that lost while their claim waited, and the farthest drawn pose handed back to the
       winner's (body_sync_lost_take); the glide off it is left out of the lag-aligned gap. */
    uint32_t lost_takes;
    double lost_take_max_m;
    double behind_max_ticks, behind_p95_ticks;
    uint64_t playback_samples; /* copy-ticks the playback measures saw */
    /* The second layer's measures (body_sync_metrics.h "local response", "rest"). */
    double response_max_ticks, response_p95_ticks;
    uint32_t response_events, response_misses;
    double rest_jitter_max_m;
    uint32_t rest_wakes;
    uint32_t rest_breaks;
    uint32_t host_wakes; /* rests that ended only because host bodies moved: the host's physics */
    double creep_max;    /* m/s, the fastest an awake host body moved while this guest's world rested */
    /* "carry": the farthest the host showed a body this guest's character carries from that
       character's hold point, over the ticks it showed one (carry_ticks). */
    double carry_gap_max_m;
    uint32_t carry_ticks;
    uint32_t rest_ticks;         /* copy-ticks the rest measures saw; none fails their checks */
    uint32_t rest_lag_max_ticks; /* the longest a copy moved on while its owner rested, untouched */
    uint32_t soft_places_max;    /* the most placements one predicted copy took (a stall, a snap) */
    uint32_t pause_window_max_ticks;   /* the most ticks one copy's pull was paused in any 120 */
    uint32_t aligned_predicted_samples; /* lag-aligned samples of copies the layer predicts */
    double copy_speed_max; /* m/s, the fastest any copy on this guest moved */
    double first_hit_tick;         /* the stand tick of the first hit measured; -1 none */
    double pause_divergence_max_m; /* the largest gap, drawn here to the host's pose, a hit body
                                      shows at the pause's end (24 ticks after the hit) */
    body_sync_bytes_t to_guest; /* the host's sends to this seat */
    body_sync_bytes_t to_host;  /* this seat's sends to the host */
    double rtt_ms, loss;        /* the session's last estimate on the host, negative unknown */
    uint32_t refused, stale, rejected; /* the seam's counts on both ends of this path */
    uint64_t deferred;                 /* both ends: due records the budget left out */
    uint32_t stale_max_ticks;          /* the worse end */
    uint32_t flush_over_limit;         /* ticks, both directions */
    uint32_t packets_to_guest;         /* state packets the host's seam wrote to this seat */
    /* This seat's claims on the host (see "claims" above): claims = overflow + repeat + rejected
       + idle + effects. */
    uint32_t claims, claims_overflow, claims_repeat, claims_rejected, claims_idle, claim_effects, claims_format;
} body_sync_guest_metrics_t;

typedef struct body_sync_action_metrics {
    uint32_t line;
    char label[BODY_SYNC_LABEL_MAX];
    uint8_t player;
    body_sync_verb_t verb;
    uint32_t start_tick;          /* UINT32_MAX when it never started */
    body_sync_outcome_t outcome;
    int32_t host_ticks, echo_ticks, seen_ticks; /* -1: never seen */
    bool shows;    /* counts for host_ticks and seen_ticks */
    bool answered; /* counts for echo_ticks */
} body_sync_action_metrics_t;

typedef struct body_sync_check_metrics {
    uint32_t line;
    body_sync_expect_kind_t kind;
    bool passed;
    double value; /* what was measured, in the check's unit; for counts, the count */
    double limit;
    char detail[BODY_SYNC_ERROR_TEXT_MAX]; /* the first offender when it failed: body, tick, endpoint */
} body_sync_check_metrics_t;

typedef struct body_sync_report {
    char scenario[BODY_SYNC_NAME_MAX];
    char scene[BODY_SYNC_NAME_MAX];
    char implementation[BODY_SYNC_NAME_MAX]; /* body_sync_implementation() */
    uint64_t seed;
    uint32_t tick_hz;
    float light_mass_max;  /* the second layer run, kilograms; 0 off */
    uint8_t authority;     /* body_sync_authority_mode_t the seams ran */
    uint32_t ticks;        /* scenario ticks run */
    uint32_t rest_tick;    /* first tick the host's world was at rest after the last item; UINT32_MAX: never */
    uint32_t guest_count;
    body_sync_guest_metrics_t guests[BODY_SYNC_GUESTS_MAX];
    uint32_t action_count;
    body_sync_action_metrics_t actions[BODY_SYNC_SCENARIO_ACTIONS_MAX];
    uint32_t check_count;
    body_sync_check_metrics_t checks[BODY_SYNC_SCENARIO_EXPECTS_MAX];
    uint32_t sessions_closed; /* seats that left during the run, and why, is in the JSON */
    bool passed; /* every check passed */
} body_sync_report_t;

#ifdef __cplusplus
}
#endif

#endif
