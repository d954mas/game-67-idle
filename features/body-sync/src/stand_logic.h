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
/* Slack the host grants a claim measured on its own, older, world. */
#define STAND_CLAIM_SLACK_M 0.5f

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
} stand_schedule_t;

void stand_schedule_init(stand_schedule_t *schedule, const body_sync_scenario_t *scenario);
/* The action player starts at `tick`, or -1. A due timed action skips the untimed ones before
   it and cuts off the active one; an untimed one waits until the player is idle. At most one
   start per player per tick, so instant actions in a row take one tick each. */
int32_t stand_schedule_next(stand_schedule_t *schedule, uint8_t player, uint32_t tick);
void stand_schedule_end(stand_schedule_t *schedule, uint8_t player);
int32_t stand_schedule_active(const stand_schedule_t *schedule, uint8_t player);
/* Every player idle with nothing left to start, and every network item fired. */
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
/* An impact claim: a flying body hit a live breakable at `speed`. */
bool stand_rules_impact(const stand_target_t *target, float speed);
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
    uint32_t body;     /* IMPACT: the flying body */
    uint16_t action;   /* HIT, VERDICT: the claimant's action index */
    uint8_t accepted;  /* VERDICT */
    uint8_t source;    /* SPAWN: template index */
    float origin[3];   /* HIT */
    float speed;       /* IMPACT */
    float position[3]; /* SPAWN */
    float rotation[4];
    float velocity[3];
    float angular[3];
} stand_record_t;

/* Writes the family byte and the record; returns the size, 0 when `capacity` is short. */
uint32_t stand_record_encode(const stand_record_t *record, uint8_t *buffer, uint32_t capacity);
/* Parses untrusted bytes after the family byte. `from_host` says who sent them: each side
   may send only its own types. False with `error` filled on anything out of range. */
bool stand_record_decode(const uint8_t *data, uint32_t size, bool from_host, const stand_limits_t *limits, stand_record_t *out, char *error,
                         size_t error_size);
body_sync_stand_kind_t stand_record_kind(stand_record_type_t type);

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
void stand_metrics_end_tick(stand_metrics_t *metrics);
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

/* Writes the report as schema body_sync.metrics.v1. `metrics` adds the closed seats and, when
   `series`, the per-tick and per-second series. False when `capacity` is short; *size then
   holds the size needed. */
bool stand_json_write(const body_sync_report_t *report, const stand_metrics_t *metrics, bool series, char *buffer, size_t capacity,
                      size_t *size);

/* Names used in JSON and logs. */
const char *stand_verb_name(body_sync_verb_t verb);
const char *stand_expect_name(body_sync_expect_kind_t kind);

#endif
