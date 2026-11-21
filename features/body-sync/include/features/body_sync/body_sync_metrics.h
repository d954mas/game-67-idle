#ifndef FEATURES_BODY_SYNC_BODY_SYNC_METRICS_H
#define FEATURES_BODY_SYNC_BODY_SYNC_METRICS_H

#include <stdbool.h>
#include <stdint.h>

#include "features/body_sync/body_sync_scenario.h"

#ifdef __cplusplus
extern "C" {
#endif

/* What a stand run measured, as a struct for tests and as JSON (schema "body_sync.metrics.v1",
   written by body_sync_stand_write_json). The JSON holds nothing that differs between two runs
   of one scenario with one seed on one build: no clock, no path, no address, so two such runs
   give byte-equal files.

   Definitions, all on the stand's fixed tick (headless, a frame is a tick):
   - divergence: for each guest and each body bound in both worlds, the distance between the
     guest's drawn position and the host's physics position at the same tick. It includes the
     copy's latency by design (what that guest sees against the truth), so a moving body shows
     divergence however good the sync is: it is not the sync's error. A lag-aligned divergence
     comes with the receiver's playback buffer.
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
   - flush_over_limit: ticks where one path's state packet and that tick's seam records
     together pass the session's largest state payload: a transport that packs one flush into
     one datagram would split them there. */

#define BODY_SYNC_METRICS_SCHEMA "body_sync.metrics.v1"
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
    body_sync_bytes_t to_guest; /* the host's sends to this seat */
    body_sync_bytes_t to_host;  /* this seat's sends to the host */
    double rtt_ms, loss;        /* the session's last estimate on the host, negative unknown */
    uint32_t refused, stale, rejected; /* the seam's counts on both ends of this path */
    uint64_t deferred;                 /* both ends: due records the budget left out */
    uint32_t stale_max_ticks;          /* the worse end */
    uint32_t flush_over_limit;         /* ticks, both directions */
    uint32_t packets_to_guest;         /* state packets the host's seam wrote to this seat */
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
