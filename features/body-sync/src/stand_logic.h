#ifndef BODY_SYNC_STAND_LOGIC_H
#define BODY_SYNC_STAND_LOGIC_H

/* The stand's decisions that need no physics: random draws, small vector math, the action
   schedule, the host rules' verdicts, the stand's own records, metric arithmetic and JSON.
   Nothing here includes Box3D, so these parts are tested alone. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "features/body_sync/body_sync_stand.h"

/* BODY_SYNC_BODIES_MAX of body_sync.h, repeated to keep Box3D out; the runner asserts it. */
enum { STAND_BODIES_MAX = 1024 };
/* Spawns one break may make; the runner keeps the ids of this many per effect. */
enum { STAND_BREAK_SPAWNS_MAX = 40 };
/* How far past the scene's bounds a peer's numbers may go, and the speeds they may reach. */
#define STAND_BOUNDS_MARGIN 10.0f
#define STAND_MAX_SPEED 100.0f
#define STAND_MAX_SPIN 200.0f

/* What a peer's numbers must stay within. */
typedef struct stand_limits {
    float bounds_min[3];
    float bounds_max[3];
    float max_speed;
    float max_spin;
} stand_limits_t;
/* The scene's bounds widened by STAND_BOUNDS_MARGIN, with the stand's speeds. */
stand_limits_t stand_limits_of(const body_sync_scene_t *scene);

#define STAND_GRAVITY 12.0f
#define STAND_DT (1.0f / 60.0f)
/* The game's upward lift that makes a throw at a point arc onto it. */
#define STAND_THROW_LIFT 1.2f
/* Slack the host grants a claim measured on its own, older, world: it shows a guest's
   character one way plus the playback delay late, and 1 m is 280 ms at walking speed. */
#define STAND_CLAIM_SLACK_M 1.0f

/* ---- random draws (splitmix64: tiny, seedable, the same on every platform) ---- */

typedef struct stand_rng {
    uint64_t state;
} stand_rng_t;
void stand_rng_seed(stand_rng_t *rng, uint64_t seed);
uint64_t stand_rng_next(stand_rng_t *rng);
float stand_rng_unit(stand_rng_t *rng); /* [0, 1) */

/* ---- math on float[3] and quaternions x, y, z, w ---- */

float stand_length3(const float v[3]);
float stand_distance3(const float a[3], const float b[3]);
float stand_quat_angle_deg(const float a[4], const float b[4]);
void stand_quat_rotate(const float q[4], const float v[3], float out[3]);
void stand_quat_mul(const float a[4], const float b[4], float out[4]);
void stand_quat_yaw(float yaw, float out[4]);
/* Volume of a shape in m^3; zero for an invalid one. */
float stand_shape_volume(body_sync_shape_t shape, const float size[3]);
/* Radius of the body's footprint on the floor, for reach and cone tests. */
float stand_footprint_radius(const body_sync_body_desc_t *body);
/* Velocity that carries a throw from `from` onto `to` despite gravity, at roughly `speed`. */
void stand_ballistic(const float from[3], const float to[3], float speed, float out[3]);

/* ---- schedule of player actions and network items ---- */

typedef struct stand_schedule {
    const body_sync_scenario_t *scenario;
    uint16_t order[BODY_SYNC_PLAYERS_MAX][BODY_SYNC_SCENARIO_ACTIONS_MAX];
    uint32_t count[BODY_SYNC_PLAYERS_MAX];
    uint32_t cursor[BODY_SYNC_PLAYERS_MAX];
    int32_t active[BODY_SYNC_PLAYERS_MAX]; /* action index, -1 idle */
    uint32_t net_next;
    uint32_t fired[BODY_SYNC_SCENARIO_ACTIONS_MAX]; /* an `on` action's start tick + 1, 0 before its event */
} stand_schedule_t;

void stand_schedule_init(stand_schedule_t *schedule, const body_sync_scenario_t *scenario);
/* The action player starts at `tick`, or -1. A due timed action skips the untimed ones before
   it and cuts off the active one; an untimed one waits until the player is idle. At most one
   start per player per tick, so instant actions in a row take one tick each. */
int32_t stand_schedule_next(stand_schedule_t *schedule, uint8_t player, uint32_t tick);
void stand_schedule_end(stand_schedule_t *schedule, uint8_t player);
/* The event of action `index` (its `on`) happened at `tick`: from then it starts as a timed action
   at tick plus its delay. Only the first counts; nothing for an action without one. */
void stand_schedule_fire(stand_schedule_t *schedule, uint32_t index, uint32_t tick);
int32_t stand_schedule_active(const stand_schedule_t *schedule, uint8_t player);
/* Every player idle with nothing left to start but actions whose event has not happened (and
   the untimed ones behind them), and every network item fired. */
bool stand_schedule_done(const stand_schedule_t *schedule);
/* The next network item due at `tick`, or NULL; each is returned once. */
const body_sync_net_event_t *stand_schedule_net(stand_schedule_t *schedule, uint32_t tick);

/* ---- host rules ---- */

typedef enum stand_effect {
    STAND_EFFECT_NONE = 0,
    STAND_EFFECT_KNOCK,
    STAND_EFFECT_BREAK,
    STAND_EFFECT_PUSH,
} stand_effect_t;

/* What the host's world says about a claimed body. */
typedef struct stand_target {
    bool live;
    const body_sync_body_desc_t *desc; /* NULL for a character */
    bool character;
    bool knocked;
    bool host_simulates; /* the host has authority over it now */
    bool held;           /* some character carries it */
    float position[3];
} stand_target_t;

/* A grab claim: granted when the body is live, grabbable, not carried and the claimant's
   capsule centre is within reach plus slack of the body's footprint. */
bool stand_rules_grab(const stand_target_t *target, const float claimant[3], float reach);
/* A hit claim from a punch at `origin`: accepted when the target is live and within range plus
   slack; the effect says what the host does. A knocked mannequin or a static body is rejected. */
bool stand_rules_hit(const stand_target_t *target, const float origin[3], float range, stand_effect_t *effect);
/* A flying body met `target` at `speed` (m/s, the contact's approach): a live breakable breaks
   at its break_speed, a standing mannequin falls at its fall_speed, anything else is NONE. The
   same rule judges on the owner (whether to claim) and on the host (what to apply). */
stand_effect_t stand_rules_impact(const stand_target_t *target, float speed);

/* The host's acceptance of a guest's impact claim beyond the seam's verdict: stand policy, not
   the seam's contract (a game sets its own). The host measures on its late world, so the claimed
   approach may exceed what it saw by a quarter plus 0.5 m/s, a hit event's approach against the
   sampled speeds of the states; the gap must lie within the reach the seam derives. */
#define STAND_IMPACT_SPEED_SCALE 1.25f
/* The oldest contact an impact claim may name, ticks before the record's own stamp: a claim waits
   behind its tick's seam records a tick or two; one this old names a contact the host's evidence
   (12 states) no longer holds. A contact after the stamp never happened. */
#define STAND_CONTACT_AGE_MAX_TICKS 120u
#define STAND_IMPACT_SPEED_SLACK 0.5f
bool stand_impact_accepts(float gap, float reach, float approach_max, float claimed_speed);

/* Claim keys already sent or judged: (seat, agent, number, target). One flight of one owner
   names one number, so a body bouncing on a target claims it once, and a new throw (a new
   number) claims a target that survived again. A ring: the oldest key goes first, by then its
   number is past the seam's grace. */
enum { STAND_CLAIM_KEYS_MAX = 128 };
typedef struct stand_claim_key {
    uint32_t agent, target;
    uint16_t number;
    uint8_t seat;
} stand_claim_key_t;
typedef struct stand_claim_keys {
    stand_claim_key_t keys[STAND_CLAIM_KEYS_MAX];
    uint32_t next, count;
} stand_claim_keys_t;
/* False when `key` is in the table; otherwise adds it and returns true. */
bool stand_claim_keys_add(stand_claim_keys_t *keys, stand_claim_key_t key);
bool stand_claim_keys_has(const stand_claim_keys_t *keys, stand_claim_key_t key);
/* Pose and velocity of debris piece `index` of a breaking body, drawn from the host's rng. */
void stand_rules_debris(stand_rng_t *rng, const body_sync_body_desc_t *breakable, const float position[3], const float rotation[4],
                        float out_position[3], float out_velocity[3]);
/* Pose of a content template placed relative to the breakable. */
void stand_rules_content(const body_sync_body_desc_t *content, const float position[3], const float rotation[4], float out_position[3],
                         float out_rotation[4]);

/* ---- the stand's own records ---- */

/* First byte of every reliable record the stand sends: whose record it is. */
enum { STAND_FAMILY_SEAM = 1, STAND_FAMILY_STAND = 2 };
enum { STAND_RECORD_MAX = 96 };

typedef enum stand_record_type {
    STAND_REC_HIT = 1, /* guest to host */
    STAND_REC_IMPACT,  /* guest to host */
    STAND_REC_VERDICT, /* host to the claimant */
    STAND_REC_SPAWN,   /* host to every guest */
    STAND_REC_REMOVE,  /* host to every guest */
} stand_record_type_t;

typedef struct stand_record {
    stand_record_type_t type;
    uint32_t target;   /* HIT, IMPACT, VERDICT, REMOVE, SPAWN: a body_sync_id_t */
    uint32_t body;     /* IMPACT: the flying body, the claim's agent */
    uint16_t number;   /* IMPACT: the agent's authority number at the contact (body_sync_contact_number) */
    uint16_t action;   /* HIT, VERDICT: the claimant's action index */
    uint8_t accepted;  /* VERDICT */
    uint8_t source;    /* SPAWN: template index */
    float origin[3];   /* HIT */
    float speed;       /* IMPACT */
    uint32_t tick;     /* IMPACT: the sender's tick of the contact, the one its states are stamped with */
    float position[3]; /* SPAWN */
    float rotation[4];
    float velocity[3];
    float angular[3];
} stand_record_t;

/* Writes the family byte and the record; returns the size, 0 when `capacity` is short. */
uint32_t stand_record_encode(const stand_record_t *record, uint8_t *buffer, uint32_t capacity);
/* Parses untrusted bytes after the family byte. `from_host` says who sent them: each side
   may send only its own types. False with `error` filled on anything out of range: an unknown
   type, a wrong size, a type of the other side, an id with a slot past the bodies or generation
   0, and per type its fields (an impact's agent that is its target, a speed not finite, negative
   or past the limits, a contact tick after `stamp`, the record's own, or more than
   STAND_CONTACT_AGE_MAX_TICKS before it). Never asserts. */
bool stand_record_decode(const uint8_t *data, uint32_t size, bool from_host, uint32_t stamp, const stand_limits_t *limits, stand_record_t *out,
                         char *error, size_t error_size);
body_sync_stand_kind_t stand_record_kind(stand_record_type_t type);

/* ---- a rest's end ---- */

/* A copy rests with its host body: slower than the run's rest (`slow`), drawn where its physics
   is (`shown`: no render offset still decaying) and drawn within `near_m` of its host body. */
bool stand_copy_rests(bool slow, bool shown, float host_gap_m, float near_m);
/* The owner's exact path, oldest first, moved by the seam's test of a move (sync_soft_owner_rests):
   at some tick it was more than `step_m` from where it was `ticks` before. */
bool stand_path_moved(const float (*path)[3], uint32_t count, float step_m, uint32_t ticks);
/* ...and its rotations, oldest first: at some tick it had turned more than `step_rad` from its
   rotation `ticks` before. */
bool stand_turns_moved(const float (*turns)[4], uint32_t count, float step_rad, uint32_t ticks);
/* A copy that does not rest while its host body rests ends the world's rest by this guest's doing,
   unless the host body's own path at the owner ticks the copy showed moved (`host_moved_at_play`):
   the seam takes a copy of an owner moving that fast out of the rest hold and pulls it after, so
   the copy's move is the host's physics (a host wake). The host body's speed now is no test: the
   copy shows it a playback delay late, and an owner creeping between the seam's creep bound and the
   run's rest speed is a move to the seam and a rest to the stand. */
bool stand_copy_breaks(bool host_rests, bool copy_rests, bool host_moved_at_play);

/* ---- a driven body's rest ---- */

/* A driven (kinematic) body has no motion of its own: its velocity is what the last drive asked
   of one step, and a copy of a still or creeping owner moves by the copy grid, up to a step (a
   cell's diagonal, 3.4 mm; 0.0195 rad) in one tick, 0.2 m/s and 1.2 rad/s, now and then. Its rest
   is judged by its mean speed and spin over the last STAND_DRIVE_TICKS ticks. Window: an owner
   creeping at c (in the rest hold at most the seam's creep bound, a step an eighth of a second,
   27 mm/s: a faster owner takes its copy out of the hold, stand_copy_breaks; nothing bounds a host
   body's own creep, which the stand has seen reach 48 mm/s) and one step of rounding move a copy c*T + 3.4 mm over
   T, under the rest speed (5 cm/s) for T > 0.15 s; 15 ticks (0.25 s) bound the path at 12.5 mm
   against 10.2 mm at the creep bound, 7.2 mm at the sweep's creep. A tick's move past
   STAND_DRIVE_STEP_M or STAND_DRIVE_STEP_RAD counts at its own speed: the rest hold lets a copy
   trail its owner by a step and close it in one tick while the owner's record moves a step more
   (twice a step); a centimetre's snap is no rounding. */
enum { STAND_DRIVE_TICKS = 15 };
#define STAND_DRIVE_STEP_M 0.0068f
#define STAND_DRIVE_STEP_RAD 0.039f

typedef struct stand_drive {
    float speed[STAND_DRIVE_TICKS], spin[STAND_DRIVE_TICKS];
    uint32_t next;
} stand_drive_t;
/* Notes one tick's speed (m/s) and spin (rad/s). */
void stand_drive_note(stand_drive_t *drive, float speed, float spin);
/* What the body moves at for its rest: the window's means, or the last tick's own speed or spin
   when that tick moved it past a step (`dt` the tick, seconds). */
void stand_drive_motion(const stand_drive_t *drive, float dt, float *speed, float *spin);

/* ---- metrics ---- */

enum { STAND_HIST_BINS = 422 };

/* Log-spaced bins from 1e-5 to 1e2 m, 60 per decade: quantiles within 4% and the same on every
   run, without keeping the samples. */
typedef struct stand_hist {
    uint32_t bins[STAND_HIST_BINS];
    uint64_t count;
    double sum;
    double max;
} stand_hist_t;
void stand_hist_add(stand_hist_t *hist, double value);
double stand_hist_quantile(const stand_hist_t *hist, double q);
double stand_hist_mean(const stand_hist_t *hist);

enum { STAND_DIR_TO_GUEST = 0, STAND_DIR_TO_HOST = 1 };
enum { STAND_CLOSED_MAX = 8 };

typedef struct stand_closed {
    uint8_t seat;
    uint32_t tick;
    uint32_t reason;
    const char *reason_name; /* a static string from the session's close names */
} stand_closed_t;

typedef struct stand_metrics {
    uint32_t guests;
    uint32_t ticks; /* ticks sampled */
    stand_hist_t divergence[BODY_SYNC_GUESTS_MAX];
    stand_hist_t jerk[BODY_SYNC_GUESTS_MAX];
    double rotation_max[BODY_SYNC_GUESTS_MAX];
    uint32_t jerk_over[BODY_SYNC_GUESTS_MAX];
    /* the tick being sampled */
    double tick_div_max[BODY_SYNC_GUESTS_MAX], tick_div_sum[BODY_SYNC_GUESTS_MAX], tick_jerk_max[BODY_SYNC_GUESTS_MAX];
    uint32_t tick_div_count[BODY_SYNC_GUESTS_MAX];
    float series_div_max[BODY_SYNC_GUESTS_MAX][BODY_SYNC_STAND_TICKS_MAX];
    float series_div_mean[BODY_SYNC_GUESTS_MAX][BODY_SYNC_STAND_TICKS_MAX];
    float series_jerk_max[BODY_SYNC_GUESTS_MAX][BODY_SYNC_STAND_TICKS_MAX];
    stand_hist_t lag[BODY_SYNC_GUESTS_MAX], path[BODY_SYNC_GUESTS_MAX], behind[BODY_SYNC_GUESTS_MAX];
    uint32_t steps[BODY_SYNC_GUESTS_MAX], offset_steps[BODY_SYNC_GUESTS_MAX], snaps[BODY_SYNC_GUESTS_MAX];
    double jump_max[BODY_SYNC_GUESTS_MAX];
    stand_hist_t aligned[BODY_SYNC_GUESTS_MAX], prediction[BODY_SYNC_GUESTS_MAX], flight_pause[BODY_SYNC_GUESTS_MAX];
    double local_correction[BODY_SYNC_GUESTS_MAX];
    double local_turn_deg[BODY_SYNC_GUESTS_MAX];
    uint32_t local_samples[BODY_SYNC_GUESTS_MAX];
    uint32_t lost_takes[BODY_SYNC_GUESTS_MAX];
    double lost_take_max[BODY_SYNC_GUESTS_MAX];
    stand_hist_t response[BODY_SYNC_GUESTS_MAX];
    double rest_jitter[BODY_SYNC_GUESTS_MAX], copy_speed[BODY_SYNC_GUESTS_MAX], pause_divergence[BODY_SYNC_GUESTS_MAX];
    uint32_t first_hit[BODY_SYNC_GUESTS_MAX]; /* tick + 1 of the first hit answered or missed; 0 none */
    uint32_t rest_wakes[BODY_SYNC_GUESTS_MAX], rest_breaks[BODY_SYNC_GUESTS_MAX], rest_ticks[BODY_SYNC_GUESTS_MAX], response_misses[BODY_SYNC_GUESTS_MAX];
    uint32_t rest_lag[BODY_SYNC_GUESTS_MAX], soft_places[BODY_SYNC_GUESTS_MAX], host_wakes[BODY_SYNC_GUESTS_MAX];
    uint32_t aligned_predicted[BODY_SYNC_GUESTS_MAX], pause_window[BODY_SYNC_GUESTS_MAX];
    double creep[BODY_SYNC_GUESTS_MAX];
    double carry_gap[BODY_SYNC_GUESTS_MAX];
    uint32_t carry_ticks[BODY_SYNC_GUESTS_MAX];
    uint32_t bytes_second[BODY_SYNC_GUESTS_MAX][2][BODY_SYNC_KINDS_ALL][BODY_SYNC_TIME_MAX_S];
    uint64_t bytes_total[BODY_SYNC_GUESTS_MAX][2][BODY_SYNC_KINDS_ALL];
    double link_rate_sum[BODY_SYNC_GUESTS_MAX][2];
    uint32_t link_rate_count[BODY_SYNC_GUESTS_MAX][2];
    uint32_t closed_count;
    stand_closed_t closed[STAND_CLOSED_MAX];
} stand_metrics_t;

void stand_metrics_reset(stand_metrics_t *metrics, uint32_t guests);
void stand_metrics_begin_tick(stand_metrics_t *metrics);
/* guest is 0-based (seat - 1). */
void stand_metrics_divergence(stand_metrics_t *metrics, uint32_t guest, double distance_m, double rotation_deg);
void stand_metrics_jerk(stand_metrics_t *metrics, uint32_t guest, double jerk_m);
/* One copy at one tick against its owner (body_sync_metrics.h): the lag-aligned distance, the
   path error and how many owner ticks it plays behind; with `moved`, the drawn and the owner's
   move since the last tick, and whether a render offset was giving a move back meanwhile. */
void stand_metrics_playback(stand_metrics_t *metrics, uint32_t guest, double lag_m, double path_m, double behind_ticks, bool moved,
                            double drawn_move_m, double owner_move_m, bool offset_active);
/* A predicted copy's drawn move that its own step does not explain: into jump.max. */
void stand_metrics_correction(stand_metrics_t *metrics, uint32_t guest, double correction_m);
/* One sample of the lag-aligned gap (body_sync_metrics.h), `predicted` of a copy the layer predicts. */
void stand_metrics_aligned(stand_metrics_t *metrics, uint32_t guest, double gap_m, bool predicted);
/* The most paused ticks one copy of this guest's had in a pause window so far. */
void stand_metrics_pause_window(stand_metrics_t *metrics, uint32_t guest, uint32_t ticks);
/* A prediction in a pause from rest against the host's answer at the guest's moment: reported only. */
void stand_metrics_prediction(stand_metrics_t *metrics, uint32_t guest, double gap_m);
/* A prediction in a pause with no shared moment against its owner at the playback tick: reported only. */
void stand_metrics_flight_pause(stand_metrics_t *metrics, uint32_t guest, double gap_m);
/* A body the guest simulates moved and turned by the seam, not by its own step: into
   local_correction.max and local_turn.max. */
void stand_metrics_local_correction(stand_metrics_t *metrics, uint32_t guest, double correction_m, double correction_deg);
/* A take of the guest's that lost, its drawn pose handed back `hand_back_m` from the winner's. */
void stand_metrics_lost_take(stand_metrics_t *metrics, uint32_t guest, double hand_back_m);
void stand_metrics_end_tick(stand_metrics_t *metrics);
/* The second layer's measures (body_sync_metrics.h "local response", "rest"). */
void stand_metrics_response(stand_metrics_t *metrics, uint32_t guest, double ticks);
void stand_metrics_response_miss(stand_metrics_t *metrics, uint32_t guest);
void stand_metrics_rest(stand_metrics_t *metrics, uint32_t guest, double jitter_m, bool wake);
/* A tick the host shows a body this guest's character carries, `gap_m` from its hold point. */
void stand_metrics_carry(stand_metrics_t *metrics, uint32_t guest, double gap_m);
/* A guest's world at rest stopped resting: a break of its own, or a host wake (`host`: only host
   bodies moved). */
void stand_metrics_rest_break(stand_metrics_t *metrics, uint32_t guest, bool host);
/* An awake host body's speed while the guest's world rests (m/s). */
void stand_metrics_creep(stand_metrics_t *metrics, uint32_t guest, double speed);
void stand_metrics_rest_lag(stand_metrics_t *metrics, uint32_t guest, uint32_t ticks);
void stand_metrics_soft_place(stand_metrics_t *metrics, uint32_t guest, uint32_t places);
void stand_metrics_copy_speed(stand_metrics_t *metrics, uint32_t guest, double speed);
void stand_metrics_layer_hit(stand_metrics_t *metrics, uint32_t guest, uint32_t tick, double pause_divergence);
void stand_metrics_bytes(stand_metrics_t *metrics, uint32_t guest, int direction, uint32_t kind, uint64_t bytes, uint32_t tick);
void stand_metrics_link(stand_metrics_t *metrics, uint32_t guest, int direction, double bytes_per_s);
void stand_metrics_closed(stand_metrics_t *metrics, uint8_t seat, uint32_t tick, uint32_t reason, const char *reason_name);
/* Fills the guests' summaries of the report from what was sampled. */
void stand_metrics_summarize(const stand_metrics_t *metrics, body_sync_report_t *report);

typedef enum stand_metric_unit {
    STAND_UNIT_LENGTH = 1,
    STAND_UNIT_ANGLE,
    STAND_UNIT_TICKS,
    STAND_UNIT_RATE,
    STAND_UNIT_COUNT,
} stand_metric_unit_t;
/* The unit of a metric name of body_sync_metrics.h; false for an unknown name. */
bool stand_metric_unit(const char *name, stand_metric_unit_t *unit);
/* The worst guest's value of a metric (the worst action for the delay metrics); false when
   unknown. A run without guests or without delays reads 0. */
bool stand_metric_value(const body_sync_report_t *report, const stand_metrics_t *metrics, const char *name, double *value, char *detail,
                        size_t detail_size);

/* ---- JSON ---- */

/* Writes the report as schema body_sync.metrics.v2. `metrics` adds the closed seats and, when
   `series`, the per-tick and per-second series. False when `capacity` is short; *size then
   holds the size needed. */
bool stand_json_write(const body_sync_report_t *report, const stand_metrics_t *metrics, bool series, char *buffer, size_t capacity,
                      size_t *size);

/* Names used in JSON and logs. */
const char *stand_verb_name(body_sync_verb_t verb);
const char *stand_expect_name(body_sync_expect_kind_t kind);

#endif
