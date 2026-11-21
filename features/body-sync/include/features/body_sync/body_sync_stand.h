#ifndef FEATURES_BODY_SYNC_BODY_SYNC_STAND_H
#define FEATURES_BODY_SYNC_BODY_SYNC_STAND_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "features/body_sync/body_sync_metrics.h"
#include "features/body_sync/body_sync_scenario.h"
#include "features/body_sync/body_sync_scene.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The network-physics stand: one host and up to three guests in one process, each with its own
   Box3D world built from one scene, its own body_sync seam and its own coop session, joined by
   a seeded net_link_sim. The stand is the game here: it owns the loop.

   Worlds step at 1/60 s with 4 sub-steps, gravity 12 m/s^2, on one thread. The network runs on
   virtual time and the host's random draws come from the scenario's seed, so one scenario with
   one seed on one build gives the same run, tick for tick.

   One stand tick, every endpoint in turn, the host first: the session's time and poll (state
   and seam records to the seam, the stand's records to its rules), the peer ticks, apply, the
   scheduled actions and the character, the host rules, the world step, collect, the seam's and
   the stand's sends, end_tick; then the network advances one tick and the metrics sample.

   Before tick 0 every guest connects and is seated; that phase is not measured, and a guest
   not seated in 10 virtual seconds refuses the run. Seat g belongs to guest g.

   Host rules (stand-ins for a game's):
   - Grab: a claim reaching the host is granted when nobody holds the body, the claimant is
     within reach plus 0.5 m on the host's world, and no earlier claim for it is unanswered;
     otherwise refused. First claim wins; the host's own player claims first in its tick.
   - Hit: a guest's punch sends a hit claim (target, tick, origin, facing). The host accepts it
     when the target lies within range plus 0.5 m of the claimed origin on its world; then a
     mannequin is knocked down (dynamic, rotation free, knock_speed away from the puncher), a
     breakable with PUNCH_BREAKS breaks, an item gets punch_push. A rejected hit is answered.
   - Impact: the endpoint that simulates a flying body judges its contacts; a contact with a
     breakable at break_speed or more sends an impact claim, and the host breaks it.
   - Break: the host removes the body and spawns its contents and debris with its random draws;
     guests get one remove and one spawn record per body, with ids, poses and velocities.
   - A knocked mannequin carries the knocked flag; a broken body never comes back. */

typedef struct body_sync_stand body_sync_stand_t;

enum {
    BODY_SYNC_ENDPOINTS_MAX = 1 + BODY_SYNC_GUESTS_MAX,
    /* Ticks a run may last, connect phase excluded: the per-tick series are sized for it. */
    BODY_SYNC_STAND_TICKS_MAX = BODY_SYNC_TIME_MAX_S * BODY_SYNC_TICK_HZ,
};

typedef struct body_sync_stand_config {
    /* Both are copied at create; the caller may free them after. */
    const body_sync_scene_t *scene;
    const body_sync_scenario_t *scenario;
    bool override_seed; /* run with `seed` instead of the scenario's, for sweeps */
    uint64_t seed;
    bool series; /* per-tick and per-second series in the JSON; the summary is always there */
} body_sync_stand_config_t;

/* Builds every endpoint's world, seam, session and the network, and connects the guests. NULL
   with `error` filled when the scene does not validate, the scenario's scene name differs from
   the scene's, a named body is missing, an action names a player the scene has no spawn point
   for, a guest is not seated in time, or memory is short. A scene player whose endpoint the
   scenario does not run (p2 with one guest) is not built. */
body_sync_stand_t *body_sync_stand_create(const body_sync_stand_config_t *config, body_sync_error_t *error);
void body_sync_stand_destroy(body_sync_stand_t *stand);

/* One tick of everything; false once the run has ended (the hard end, or rest after the last
   item ended, or the settle time spent). Ticking an ended stand does nothing. */
bool body_sync_stand_tick(body_sync_stand_t *stand);

/* Ticks to the end, then evaluates the checks. */
const body_sync_report_t *body_sync_stand_run(body_sync_stand_t *stand);

/* Evaluates the checks on the run so far and returns the report; the run may continue and a
   later call evaluates again. The pointer lives as long as the stand. */
const body_sync_report_t *body_sync_stand_report(body_sync_stand_t *stand);

/* The scenario tick about to run: 0 before the first body_sync_stand_tick. */
uint32_t body_sync_stand_now(const body_sync_stand_t *stand);
uint32_t body_sync_stand_endpoint_count(const body_sync_stand_t *stand);

/* A hash of one endpoint's world: for every live slot in order, its slot, generation, motion
   type, motion locks, the filter of its shape (a carried body's differs), shape, size, mass,
   and the bits of its transform, velocities and awake state. Equal hashes mean those values
   are equal to the bit; they do not promise equal sleep timers, contact caches or the
   solver's warm start, so two worlds with one hash may still part on a later step. Before the
   first tick every endpoint's hash is the same. 0 for an endpoint out of range. */
uint64_t body_sync_stand_world_hash(const body_sync_stand_t *stand, uint32_t endpoint);

/* ---- what a drawer reads ---- */

enum {
    BODY_SYNC_VIEW_STATIC = 1u << 0,
    BODY_SYNC_VIEW_COPY = 1u << 1,      /* another endpoint simulates it; drawn through the seam */
    BODY_SYNC_VIEW_CHARACTER = 1u << 2,
    BODY_SYNC_VIEW_HELD = 1u << 3,
    BODY_SYNC_VIEW_FLYING = 1u << 4,    /* thrown or punched, its hold not yet settled */
    BODY_SYNC_VIEW_ASLEEP = 1u << 5,
    BODY_SYNC_VIEW_KNOCKED = 1u << 6,
};

typedef struct body_sync_body_view {
    uint32_t id; /* body_sync_id_t */
    const char *name; /* the scene's name, "" for spawned bodies and unnamed ones */
    body_sync_shape_t shape;
    float size[3];
    float position[3]; /* the drawn pose: the physics pose with the seam's render offset */
    float rotation[4];
    uint32_t model_tag;
    body_sync_role_t role;
    uint32_t flags;    /* BODY_SYNC_VIEW_* */
    uint8_t authority; /* the seat that simulates it */
    uint8_t player;    /* for a character; BODY_SYNC_PLAYERS_MAX otherwise */
} body_sync_body_view_t;

/* The bodies of one endpoint's world, statics included, in slot order. Index 0..count-1 is
   valid until the next tick; ids say which body is which across ticks and endpoints. */
uint32_t body_sync_stand_body_count(const body_sync_stand_t *stand, uint32_t endpoint);
bool body_sync_stand_body(const body_sync_stand_t *stand, uint32_t endpoint, uint32_t index, body_sync_body_view_t *out);

/* ---- results ---- */

/* The run's metrics as JSON (schema in body_sync_metrics.h), after evaluating the checks.
   False when `capacity` is short; *size then says how much is needed. */
bool body_sync_stand_write_json(body_sync_stand_t *stand, char *buffer, size_t capacity, size_t *size);
bool body_sync_stand_save_json(body_sync_stand_t *stand, const char *path, body_sync_error_t *error);

/* The checks that end a run. A guest whose session ended fails agree_at_rest, no_stuck and
   effects_once with "gN left at tick T", unless the scenario says `expect left gN`; then it is
   left out of them.
   - agree_at_rest: once the last item has ended and every endpoint's world has been at rest for
     0.5 s (every player idle, every awake body under 0.05 m/s and 0.5 rad/s), every moving body
     is on both worlds under one id and within the tolerance on every guest; a body only one
     world has is an offender. The run ends at that rest. A run that ends otherwise reads its
     last tick: at a hard end the check passes when every world has been at rest for the last
     0.5 s and agrees, and fails otherwise; when settle was spent without rest it fails. A
     missing rest is named first.
   - no_stuck: on every endpoint no body is outside the scene's bounds, kinematic with no
     holder and no stand reason, held by a character that does not hold it, under the
     authority of a seat that has left, or (at the end) a copy whose pose has not moved for 2 s
     while the host's body moves; no claim is unanswered for 2 s.
   - effects_once: on the host every knocked body was knocked exactly once by exactly one
     accepted claim, and no break lost a spawn; every guest shows each knockdown once, has
     each broken body gone and each spawned id exactly once.
   - left: the guest's session ended.
   A failed check names its first offender in the report's detail. */

/* The command line, as a library call so a game's own target runs it with its scenes:
     body_sync_stand <scenario> [--seed N] [--out metrics.json] [--no-series] [--quiet]
     body_sync_stand --list-scenes
   Scenes are the catalog's and then `extra`'s; an extra scene of a catalog name replaces it.
   Prints one summary line per check. Returns 0 when every check passed, 1 when one failed,
   2 when the input was refused, the run could not start or the metrics could not be written. */
int body_sync_stand_main(int argc, char **argv, const body_sync_scene_entry_t *extra, uint32_t extra_count);

#ifdef __cplusplus
}
#endif

#endif
