#define _CRT_SECURE_NO_WARNINGS
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "features/body_sync/body_sync_stand.h"
#include "features/coop_net/coop_session.h"
#include "features/net_link/net_link_sim.h"
#include "stand_logic.h"
#include "stand_testing.h"
#include "stand_world.h"
#include "sync_playback.h"
#include "sync_soft.h"

_Static_assert(STAND_BODIES_MAX == BODY_SYNC_BODIES_MAX, "stand_logic.h repeats the slot count");
_Static_assert(BODY_SYNC_SEAM_KINDS == BODY_SYNC_TRAFFIC_COUNT, "body_sync_metrics.h repeats the seam's kind count");

enum {
    QUEUE_MAX = 256,
    /* A guest's claims the host holds for one tick: one seat's flood fills only its own. */
    SEAT_CLAIMS_MAX = 64,
    EFFECTS_MAX = 256,
    EFFECT_SPAWNS_MAX = STAND_BREAK_SPAWNS_MAX,
    PACKET_MAX = 2048,
    CATCH_UP_MAX = 8,         /* ticks a stalled endpoint runs in one frame when it comes back */
    REST_TICKS = 30,          /* 0.5 s of stillness counts as rest */
    LAG_TICKS = 60,           /* the layer's longest pause: a copy touched within it may still move */
    STUCK_TICKS = 120,        /* 2 s */
    CONNECT_TICKS_MAX = 600,  /* 10 s for each guest to be seated */
    KNOCK_FLAG = 1u << 0,     /* seam flag bit the stand gives a knocked mannequin */
    /* An owner's poses kept per body for the playback measures: the deepest a copy plays
       behind (a second before it jumps, the delay, one way) plus the path window. */
    RING_TICKS = 128,
    PATH_TICKS = 30,
    /* The window of pause_window.max: two of the layer's longest pause chains. */
    PAUSE_WINDOW = 2 * SYNC_SOFT_PAUSE_MAX_TICKS,
};
_Static_assert(PAUSE_WINDOW <= RING_TICKS, "the pause window is read from the drawn ring");
_Static_assert(BODY_SYNC_ACTIVITY_PUNCH <= 255, "a character's activity is one data byte");
#define REST_SPEED 0.05f
#define REST_OFFSET_M 0.001f
#define REST_OFFSET_RAD 0.002f /* about a tenth of a degree */
#define REST_MOVE_M 1e-4 /* a tenth of a millimetre: no eye sees it, above a pose's float noise */
#define REST_STILL REST_SPEED /* a body this slow rests though awake (a pile against a character), as for the run's end */
#define RESPONSE_M 0.01f /* the host's move that makes a hit real */
/* The copy's answer: its drawn pose starts to move. A soft hit moves a body by millimetres a
   tick; a copy that waits for the host does not move at all. */
#define ANSWER_M 0.001f
/* A hit is watched 60 ticks for the host's move; the copy answers within the layer's pause (24
   ticks) or it missed: moved later, it was the pull, not the hit. */
enum { RESPONSE_TICKS = 60, RESPONSE_WINDOW = 24 };
#define MOVED_M 0.05f
#define OWNER_JUMP_M 0.5f /* an owner's move in one tick past any throw: it was set there */

typedef struct out_queue {
    uint8_t data[QUEUE_MAX][STAND_RECORD_MAX];
    uint8_t size[QUEUE_MAX];
    uint8_t kind[QUEUE_MAX];
    uint32_t head, count;
} out_queue_t;

typedef struct pending_claim {
    uint8_t seat;
    stand_record_t record;
} pending_claim_t;

/* One body's positions on the endpoint that owned it, by that endpoint's tick. */
typedef struct owner_ring {
    body_sync_id_t id;
    uint32_t tag[RING_TICKS]; /* tick + 1 of the entry; 0 empty */
    float position[RING_TICKS][3];
} owner_ring_t;

/* A copy's last playback sample, for its move between two ticks. */
typedef struct copy_track {
    body_sync_id_t id;
    bool valid;
    /* A predicted copy: its physics pose after the last step and after this apply, and the render
       offset then; what the seam moved it beyond its own step is its visible correction. */
    bool self_valid;
    uint32_t ticks_at; /* the endpoint's ticks run then: measured across exactly one */
    float stepped[3], applied[3], offset_at[3];
    /* A body this guest simulates, likewise (sample_local). */
    bool local_valid;
    body_sync_id_t local_id;
    uint32_t local_ticks_at;
    bool local_now;       /* simulated by this guest at the last sample */
    bool applied_copy;    /* a copy at this tick's apply: a claim refused there and taken again in the
                             collect simulated nothing in between, so its move is a copy's */
    float acted[3], acted_rot[4]; /* its pose once the seam and the actions (a claim's take-over) are done */
    float local_stepped[3], local_offset[3];
    float local_stepped_rot[4], local_offset_rot[4];
    bool local_own_offset; /* the seam set its render offset while this side simulated it */
    uint8_t owner;
    bool offset; /* a render offset was set */
    double played; /* owner tick */
    float drawn[3];
    float owner_at[3];
} copy_track_t;

typedef struct endpoint {
    bool built;
    bool active; /* seated and its session alive */
    uint8_t index; /* endpoint 0 is the host, g its seat */
    stand_world_t *world;
    body_sync_t *sync;
    net_link_t *link;
    coop_session_t *session;
    stand_character_t character; /* the player this endpoint controls */
    stand_character_t others[BODY_SYNC_GUESTS_MAX]; /* the reference run's host: players 1.., local too */
    uint32_t ticks_run;
    uint32_t rate_mhz; /* steps per second of virtual time, millihertz */
    owner_ring_t *rings; /* STAND_BODIES_MAX */
    /* What this endpoint drew of every body after each step, by session tick, and whether it
       drew its own answer then (a predicted copy in its pause, and how that pause began): the
       lag-aligned gap's pictures. */
    owner_ring_t *drawn_rings;
    uint8_t *drawn_paused; /* STAND_BODIES_MAX x RING_TICKS, PAUSE_* */
    float (*drawn_turns)[4]; /* STAND_BODIES_MAX x RING_TICKS: the drawn rotations */
    bool ring_any;
    uint32_t ring_newest; /* the session tick of its last recorded step */
    copy_track_t tracks[STAND_BODIES_MAX];
    uint32_t stall_until, deaf_until;
    out_queue_t queues[BODY_SYNC_ENDPOINTS_MAX];
    uint16_t view[STAND_BODIES_MAX];
    uint32_t view_count;
    /* sampled per slot */
    uint32_t hist_id[STAND_BODIES_MAX];
    uint8_t hist_n[STAND_BODIES_MAX];
    float prev_pos[STAND_BODIES_MAX][3];
    float prev_delta[STAND_BODIES_MAX][3];
    uint32_t frozen[STAND_BODIES_MAX];
    uint32_t pending_since[STAND_BODIES_MAX];
    stand_claim_keys_t claimed; /* impact claims this guest sent */
    uint32_t knock_id[STAND_BODIES_MAX];
    uint8_t knock_seen[STAND_BODIES_MAX];
    uint8_t knock_count[STAND_BODIES_MAX];
    uint32_t spawn_id[STAND_BODIES_MAX];
    /* The second layer's measures: hits not yet answered, and each copy's rest. */
    struct {
        uint32_t slot, id, tick;
        float drawn[3], host[3];
        bool host_moved, moved, counted;
        uint32_t answer;
    } hits[64];
    uint32_t hit_count;
    uint32_t touched_tick[STAND_BODIES_MAX]; /* stand tick + 1 a body it simulates hit this copy */
    uint32_t world_rest_run;
    uint16_t pause_window[STAND_BODIES_MAX]; /* a copy's paused ticks of the last PAUSE_WINDOW drawn */
    uint32_t pause_since[STAND_BODIES_MAX];  /* session tick its prediction was last taken back */
    bool pause_reset[STAND_BODIES_MAX];      /* placed since: the window starts again */
    uint32_t pause_window_max;
    uint32_t local_tick[STAND_BODIES_MAX]; /* stand tick + 1 this guest last simulated the body */
    bool was_resting[STAND_BODIES_MAX];
    uint32_t rest_lag[STAND_BODIES_MAX];   /* ticks moving while its owner rests, untouched */
    uint32_t soft_places[STAND_BODIES_MAX]; /* placements of a predicted copy (its offset jumped) */
    float offset_prev[STAND_BODIES_MAX];
    bool host_awake[STAND_BODIES_MAX];
    bool standing; /* every character of this guest slower than a mover: what it touches trembles, if it moves */
    bool passed[STAND_BODIES_MAX]; /* this guest's character is inside it now, counted once */
    uint32_t lost_seen[STAND_BODIES_MAX]; /* body_sync_lost_take's count already measured */
    uint32_t host_flip[STAND_BODIES_MAX]; /* tick its host body last fell asleep or woke */
    float layer_prev[STAND_BODIES_MAX][3]; /* drawn at the last sample, for the rest measures */
    float rest_here[STAND_BODIES_MAX][3];  /* ...and where its physics lay */
    float rest_here_q[STAND_BODIES_MAX][4];
    stand_drive_t drives[STAND_BODIES_MAX]; /* every body's last speeds: a driven body's rest */
    uint32_t drive_id[STAND_BODIES_MAX];
    uint8_t spawn_count[STAND_BODIES_MAX];
    uint32_t claim_stale;
} endpoint_t;

typedef struct effect {
    stand_effect_t kind;
    uint32_t target;
    uint32_t spawn_count;
    uint32_t spawns[EFFECT_SPAWNS_MAX];
    uint32_t dropped; /* spawns the host could not make: the effect did not happen whole */
} effect_t;

typedef struct action_track {
    bool started;
    bool ended;
    bool awaiting;       /* a guest's grab or hit waits for the host's answer */
    bool measuring;      /* delays are being watched */
    uint32_t start_tick; /* stand tick */
    uint32_t ref_id;
    bool ref_set;
    float ref_pos[BODY_SYNC_ENDPOINTS_MAX][3];
    uint16_t answer_number;
    bool answered_on_host;
    bool shows;    /* moved its character, sent a claim or took effect */
    bool answered; /* a guest claim the host answers */
    uint8_t seen_mask;
    body_sync_outcome_t outcome;
    int32_t host_ticks, echo_ticks, seen_ticks;
    int32_t own_ticks; /* a throw: until the actor's own world draws the body moved */
} action_track_t;

struct body_sync_stand {
    body_sync_scene_t scene;
    body_sync_scenario_t scenario;
    bool series;
    float light_mass;
    uint32_t plant_shift; /* stand_testing_plant_shift */
    uint8_t plant_keep_broken; /* stand_testing_plant_keep_broken: the endpoint, 0 none */
    float plant_drift;    /* stand_testing_plant_drift */
    float plant_lead;     /* stand_testing_plant_lead */
    uint32_t plant_jolt_tick; /* stand_testing_plant_jolt; 0 none */
    float plant_jolt_m;
    bool plant_jolt_local;
    uint32_t plant_jolt_every, plant_jolt_count; /* stand_testing_plant_jitter; 0 one jolt */
    uint32_t plant_offset_tick; /* stand_testing_plant_offset; 0 none */
    float plant_offset_by, plant_offset_turn_by;
    uint32_t plant_offset_slot; /* the body it moved, from that tick on */
    body_sync_id_t plant_offset_id;
    float plant_offset_m, plant_offset_rad; /* along x, and about y */
    uint32_t plant_pause_tick, plant_pause_ticks, plant_pause_slot; /* stand_testing_plant_pause; slot + 1, 0 none */
    uint32_t plant_creep_tick, plant_creep_ticks, plant_creep_slot; /* stand_testing_plant_creep; slot + 1, 0 none */
    float plant_creep_m, plant_creep_turn; /* turn: radians a tick, stand_testing_plant_turn */
    bool plant_creep_copy; /* the creep moves guest 1's copy, held as stand_testing_plant_wall holds one, not the host body */
    uint8_t authority; /* body_sync_authority_mode_t of every seam */
    uint64_t seed;
    uint32_t endpoints;
    net_link_sim_t *sim;
    endpoint_t ep[BODY_SYNC_ENDPOINTS_MAX];
    stand_schedule_t schedule;
    stand_rng_t rng;
    net_link_conditions_t conditions[BODY_SYNC_ENDPOINTS_MAX][2];
    uint32_t cut_until[BODY_SYNC_ENDPOINTS_MAX];
    bool left[BODY_SYNC_ENDPOINTS_MAX];
    uint32_t left_tick[BODY_SYNC_ENDPOINTS_MAX];
    stand_limits_t limits;
    pending_claim_t claims[BODY_SYNC_ENDPOINTS_MAX][SEAT_CLAIMS_MAX];
    uint32_t claim_count[BODY_SYNC_ENDPOINTS_MAX];
    /* The host's guest claims (hits and impacts), per seat: every one that parsed lands in exactly
       one of overflow (its seat's tick queue full), repeat (an impact key judged before), rejected
       (the seam's gate or the stand's limits or rules), idle (accepted, nothing to do) and
       effects; format counts the seat's stand records the parser refused. */
    stand_claim_keys_t judged[BODY_SYNC_ENDPOINTS_MAX];
    uint32_t claims_in[BODY_SYNC_ENDPOINTS_MAX], claims_overflow[BODY_SYNC_ENDPOINTS_MAX], claims_repeat[BODY_SYNC_ENDPOINTS_MAX];
    uint32_t claims_rejected[BODY_SYNC_ENDPOINTS_MAX], claims_idle[BODY_SYNC_ENDPOINTS_MAX], claim_effects[BODY_SYNC_ENDPOINTS_MAX];
    uint32_t claims_format[BODY_SYNC_ENDPOINTS_MAX];
    uint32_t claims_ended[BODY_SYNC_ENDPOINTS_MAX];
    float claim_reach_scale, claim_approach_scale; /* 1 and 1; stand_testing_plant_claim_limits */
    uint32_t plant_declared_endpoint, plant_declared_ticks; /* stand_testing_plant_declared_velocity; 0 ticks none */
    body_sync_id_t plant_declared_body;
    float plant_declared_add[3];
    /* A recall or removal of a body a guest simulated, per seat: the host's drops of that seat's
       bytes from the move (base) until the guest stopped simulating the body and the slowest
       one-way trip more (late_drops). */
    struct {
        bool open, stopped;
        body_sync_id_t id;
        uint32_t base, stopped_tick;
    } late[BODY_SYNC_ENDPOINTS_MAX];
    uint32_t late_drops[BODY_SYNC_ENDPOINTS_MAX];
    effect_t effects[EFFECTS_MAX];
    uint32_t effect_count;
    action_track_t actions[BODY_SYNC_SCENARIO_ACTIONS_MAX];
    uint64_t seam_prev[BODY_SYNC_GUESTS_MAX][2][BODY_SYNC_TRAFFIC_COUNT];
    uint32_t now;
    bool ended;
    bool items_done;
    uint32_t done_tick;
    uint32_t flush_over[BODY_SYNC_ENDPOINTS_MAX]; /* per guest, both directions */
    uint32_t rest_run;
    uint32_t rest_tick;
    bool started;
    float start_position[STAND_BODIES_MAX][3]; /* the host's, at the first tick: `expect moved` */
    /* The host's ids at the first tick: `expect effect` names a scene body whose slot a break frees
       for its spawns. */
    uint32_t start_id[STAND_BODIES_MAX];
    /* `expect pace`: a reference run is the host's world alone with every player local and no
       network, the players other than `ref_walker` acting `ref_shift` ticks late; per check, the
       walk's progress (its start distance less the nearest it came) by tick since it started. */
    bool local;
    uint32_t ref_shift;
    uint8_t ref_walker;
    bool plant_pace_on_time; /* stand_testing_plant_pace_on_time */
    float *pace[BODY_SYNC_SCENARIO_EXPECTS_MAX];
    uint32_t pace_len[BODY_SYNC_SCENARIO_EXPECTS_MAX];
    bool pace_checked[BODY_SYNC_SCENARIO_EXPECTS_MAX]; /* the ended run's verdict, kept: each costs up to lag + 1 reference runs */
    body_sync_check_metrics_t pace_check[BODY_SYNC_SCENARIO_EXPECTS_MAX];
    bool pace_closed[BODY_SYNC_SCENARIO_EXPECTS_MAX]; /* its walk ended: the last sample is in */
    float pace_start[BODY_SYNC_SCENARIO_EXPECTS_MAX], pace_best[BODY_SYNC_SCENARIO_EXPECTS_MAX];
    float one_way_max_ms;  /* the slowest one-way path the run set, jitter and reordering included */
    uint32_t frozen_max; /* ticks: the longest cut, stall or deafness the run set */
    uint32_t plant_wall_endpoint, plant_wall_count, plant_wall_slots[8]; /* stand_testing_plant_wall */
    stand_metrics_t metrics;
    body_sync_report_t report;
    uint8_t packet[PACKET_MAX];
};

static void error_text(body_sync_error_t *error, uint32_t line, const char *text) {
    if (error) {
        error->line = line;
        snprintf(error->text, sizeof(error->text), "%s", text);
    }
}

/* ---- small helpers ---- */

static bool is_host_ep(const endpoint_t *ep) { return ep->index == BODY_SYNC_ENDPOINT_HOST; }

static uint32_t character_slot(const body_sync_stand_t *st, uint32_t player) { return st->scene.body_count + player; }

/* The character `player` drives on `ep`: its own, or on the reference run's host any player's. */
static stand_character_t *player_character(body_sync_stand_t *st, endpoint_t *ep, uint8_t player) {
    if (player == ep->index) {
        return &ep->character;
    }
    return st->local && is_host_ep(ep) && player >= 1u && player <= st->scenario.guests ? &ep->others[player - 1u] : NULL;
}

static body_sync_id_t body_id_by_name(const body_sync_stand_t *st, const endpoint_t *ep, const char *name) {
    uint32_t index = body_sync_scene_find_body(&st->scene, name);
    return index == UINT32_MAX ? BODY_SYNC_ID_NONE : stand_world_id(ep->world, index);
}

static bool queue_record(body_sync_stand_t *st, endpoint_t *ep, uint8_t seat, const stand_record_t *record) {
    out_queue_t *q = &ep->queues[seat];
    if (q->count >= QUEUE_MAX) {
        ep->claim_stale++;
        return false;
    }
    uint32_t at = (q->head + q->count) % QUEUE_MAX;
    q->size[at] = (uint8_t)stand_record_encode(record, q->data[at], STAND_RECORD_MAX);
    q->kind[at] = (uint8_t)stand_record_kind(record->type);
    q->count++;
    (void)st;
    return true;
}

static void broadcast(body_sync_stand_t *st, const stand_record_t *record) {
    endpoint_t *host = &st->ep[0];
    for (uint32_t g = 1; g < st->endpoints; g++) {
        if (st->ep[g].active && !st->left[g]) {
            queue_record(st, host, (uint8_t)g, record);
        }
    }
}

static void pose_of(const endpoint_t *ep, uint32_t slot, float position[3], float rotation[4]) {
    stand_world_pose(ep->world, slot, position, rotation);
}

/* The drawn pose: physics plus the seam's render offset. */
static void drawn_pose(const endpoint_t *ep, uint32_t slot, float position[3], float rotation[4]) {
    stand_world_pose(ep->world, slot, position, rotation);
    float offset[3], turn[4];
    body_sync_id_t id = stand_world_id(ep->world, slot);
    if (body_sync_render_offset(ep->sync, id, offset, turn)) {
        stand_quat_mul(turn, rotation, rotation);
        for (int i = 0; i < 3; i++) {
            position[i] += offset[i];
        }
    }
}

static void rebuild_view(endpoint_t *ep) {
    ep->view_count = 0;
    for (uint32_t slot = 0; slot < STAND_BODIES_MAX; slot++) {
        if (ep->world->slots[slot].live) {
            ep->view[ep->view_count++] = (uint16_t)slot;
        }
    }
}

static stand_target_t target_of(const body_sync_stand_t *st, const endpoint_t *ep, body_sync_id_t id) {
    stand_target_t t;
    memset(&t, 0, sizeof(t));
    stand_slot_t *s = stand_world_slot(ep->world, id);
    if (!s) {
        return t;
    }
    t.live = true;
    t.character = s->character;
    t.desc = s->character ? NULL : &s->desc;
    t.knocked = s->knocked || (body_sync_flags(ep->sync, id) & KNOCK_FLAG) != 0;
    body_sync_authority_t a;
    if (body_sync_authority(ep->sync, id, &a)) {
        t.host_simulates = a.seat == BODY_SYNC_SEAT_HOST;
        t.held = a.holder != BODY_SYNC_ID_NONE;
    } else {
        t.host_simulates = true;
    }
    float rot[4];
    pose_of(ep, body_sync_id_slot(id), t.position, rot);
    (void)st;
    return t;
}

/* ---- host effects ---- */

static effect_t *new_effect(body_sync_stand_t *st, stand_effect_t kind, uint32_t target) {
    if (st->effect_count >= EFFECTS_MAX) {
        return NULL;
    }
    effect_t *e = &st->effects[st->effect_count++];
    memset(e, 0, sizeof(*e));
    e->kind = kind;
    e->target = target;
    return e;
}

static void spawn_on_host(body_sync_stand_t *st, effect_t *effect, uint8_t source, const float position[3], const float rotation[4],
                          const float velocity[3]) {
    endpoint_t *host = &st->ep[0];
    uint32_t slot = stand_world_free_slot(host->world);
    uint16_t generation = slot == UINT32_MAX ? 0 : (uint16_t)(host->world->slots[slot].generation + 1u);
    generation = generation == 0 ? 1 : generation;
    body_sync_id_t id = slot == UINT32_MAX ? BODY_SYNC_ID_NONE : body_sync_id_make(slot, generation);
    const float angular[3] = {0.0f, 0.0f, 0.0f};
    if (!id || source >= st->scene.template_count ||
        !stand_world_spawn(host->world, id, &st->scene.templates[source], position, rotation, velocity, angular)) {
        /* The validator keeps the worst case within the free slots; a failure still shows. */
        if (effect) {
            effect->dropped++;
        }
        return;
    }
    body_sync_bind_t bind = {.body = host->world->slots[slot].body, .body_class = BODY_SYNC_CLASS_LIGHT, .authority = BODY_SYNC_SEAT_HOST};
    body_sync_bind(host->sync, id, &bind);
    if (effect && effect->spawn_count < EFFECT_SPAWNS_MAX) {
        effect->spawns[effect->spawn_count++] = id;
    } else if (effect) {
        effect->dropped++;
    }
    stand_record_t r = {.type = STAND_REC_SPAWN, .target = id, .source = source};
    memcpy(r.position, position, sizeof(r.position));
    memcpy(r.rotation, rotation, sizeof(r.rotation));
    memcpy(r.velocity, velocity, sizeof(r.velocity));
    broadcast(st, &r);
}

static void drop_if_held(endpoint_t *ep, body_sync_id_t id) {
    for (uint32_t p = 0; p <= BODY_SYNC_GUESTS_MAX; p++) {
        stand_character_t *c = p == 0 ? &ep->character : &ep->others[p - 1u];
        if (c->held == id) {
            c->held = BODY_SYNC_ID_NONE;
            c->held_heavy = false;
        }
    }
}

static void break_on_host(body_sync_stand_t *st, body_sync_id_t id) {
    endpoint_t *host = &st->ep[0];
    stand_slot_t *s = stand_world_slot(host->world, id);
    if (!s) {
        return;
    }
    body_sync_body_desc_t desc = s->desc;
    float position[3], rotation[4];
    pose_of(host, body_sync_id_slot(id), position, rotation);
    effect_t *effect = new_effect(st, STAND_EFFECT_BREAK, id);
    drop_if_held(host, id);
    body_sync_unbind(host->sync, id);
    stand_world_remove(host->world, id);
    stand_record_t removal = {.type = STAND_REC_REMOVE, .target = id};
    broadcast(st, &removal);
    for (uint32_t i = 0; i < desc.contents_count; i++) {
        uint8_t source = (uint8_t)(desc.contents_first + i);
        float at[3], turn[4];
        stand_rules_content(&st->scene.templates[source], position, rotation, at, turn);
        const float still[3] = {0.0f, 0.0f, 0.0f};
        spawn_on_host(st, effect, source, at, turn, still);
    }
    for (uint32_t i = 0; i < desc.debris_count; i++) {
        float at[3], velocity[3];
        stand_rules_debris(&st->rng, &desc, position, rotation, at, velocity);
        spawn_on_host(st, effect, desc.debris_template, at, rotation, velocity);
    }
}

/* What an effect did on the host: refused, accepted with nothing to do (no effect, a push of a
   body the host does not simulate), or done. */
typedef enum applied {
    APPLIED_REFUSED = 0,
    APPLIED_IDLE,
    APPLIED_DONE,
} applied_t;

static applied_t apply_effect(body_sync_stand_t *st, stand_effect_t effect, body_sync_id_t target, const float origin[3]) {
    endpoint_t *host = &st->ep[0];
    stand_slot_t *s = stand_world_slot(host->world, target);
    if (!s) {
        return APPLIED_REFUSED;
    }
    const body_sync_character_tuning_t *t = &st->scene.character;
    switch (effect) {
    case STAND_EFFECT_KNOCK:
        if (!body_sync_is_local(host->sync, target)) {
            return APPLIED_REFUSED;
        }
        stand_world_knock(host->world, body_sync_id_slot(target), origin, t->knock_speed);
        body_sync_set_flags(host->sync, target, (uint8_t)(body_sync_flags(host->sync, target) | KNOCK_FLAG));
        new_effect(st, STAND_EFFECT_KNOCK, target);
        return APPLIED_DONE;
    case STAND_EFFECT_BREAK:
        break_on_host(st, target);
        return APPLIED_DONE;
    case STAND_EFFECT_PUSH:
        if (!body_sync_is_local(host->sync, target)) {
            return APPLIED_IDLE;
        }
        stand_world_push(host->world, body_sync_id_slot(target), origin, t->punch_push);
        return APPLIED_DONE;
    case STAND_EFFECT_NONE:
        return APPLIED_IDLE;
    }
    return APPLIED_REFUSED;
}

/* ---- actions ---- */

static action_track_t *track_of(body_sync_stand_t *st, int32_t index) {
    return index >= 0 && (uint32_t)index < st->scenario.action_count ? &st->actions[index] : NULL;
}

static void set_outcome(body_sync_stand_t *st, int32_t index, body_sync_outcome_t outcome) {
    action_track_t *a = track_of(st, index);
    if (a) {
        a->outcome = outcome;
    }
}

static void begin_measure(body_sync_stand_t *st, int32_t index, body_sync_id_t ref) {
    action_track_t *a = track_of(st, index);
    if (!a) {
        return;
    }
    a->measuring = true;
    a->ref_id = ref;
    a->ref_set = false;
    a->seen_mask = 0;
    a->host_ticks = a->echo_ticks = a->seen_ticks = a->own_ticks = -1;
    if (st->scenario.actions[index].player == BODY_SYNC_ENDPOINT_HOST) {
        a->host_ticks = 0;
        a->echo_ticks = 0;
    }
}

static void finish_action(body_sync_stand_t *st, endpoint_t *ep, int32_t index, body_sync_outcome_t outcome) {
    action_track_t *a = track_of(st, index);
    if (a && !a->ended) {
        a->ended = true;
        if (outcome != BODY_SYNC_OUTCOME_PENDING) {
            a->outcome = outcome;
        }
    }
    uint8_t player = st->scenario.actions[index].player;
    (void)ep;
    if (stand_schedule_active(&st->schedule, player) == index) {
        stand_schedule_end(&st->schedule, player);
    }
}

static void target_point(body_sync_stand_t *st, endpoint_t *ep, const body_sync_target_t *target, float out[3], body_sync_id_t *id) {
    *id = BODY_SYNC_ID_NONE;
    if (target->is_body) {
        *id = body_id_by_name(st, ep, target->body);
        if (*id) {
            float rot[4];
            pose_of(ep, body_sync_id_slot(*id), out, rot);
            return;
        }
    }
    memcpy(out, target->point, sizeof(float) * 3);
}

static uint32_t host_drops(const body_sync_stand_t *st, uint8_t seat) {
    body_sync_traffic_t t;
    return body_sync_traffic(st->ep[0].sync, seat, &t) ? t.stale + t.rejected : 0u;
}

static void close_late(body_sync_stand_t *st, uint8_t seat) {
    if (st->late[seat].open) {
        st->late[seat].open = false;
        st->late_drops[seat] += host_drops(st, seat) - st->late[seat].base;
    }
}

static void open_late(body_sync_stand_t *st, body_sync_id_t id) {
    body_sync_authority_t a;
    if (!body_sync_authority(st->ep[0].sync, id, &a) || a.seat == BODY_SYNC_SEAT_HOST || a.seat >= st->endpoints) {
        return;
    }
    close_late(st, a.seat);
    st->late[a.seat].open = true;
    st->late[a.seat].stopped = false;
    st->late[a.seat].id = id;
    st->late[a.seat].base = host_drops(st, a.seat);
}

/* Packets the old owner sent before it stopped simulating the body reach the host within the
   slowest one-way trip, a cut, stall or deafness included. */
static void watch_late(body_sync_stand_t *st) {
    uint32_t way = (uint32_t)ceilf(st->one_way_max_ms * (float)BODY_SYNC_TICK_HZ / 1000.0f) + st->frozen_max + 2u;
    for (uint8_t g = 1; g < st->endpoints; g++) {
        endpoint_t *ep = &st->ep[g];
        if (!st->late[g].open) {
            continue;
        }
        body_sync_id_t id = st->late[g].id;
        if (!st->late[g].stopped && (!ep->active || !stand_world_slot(ep->world, id) || !body_sync_is_local(ep->sync, id))) {
            st->late[g].stopped = true;
            st->late[g].stopped_tick = st->now;
        }
        if (st->late[g].stopped && st->now >= st->late[g].stopped_tick + way) {
            close_late(st, g);
        }
    }
}

/* p0's recall and removal: the host's own rule over a body whoever owns it. A recall is the
   forced move to the host, then a placement at rest; the seam's number moves, so the old owner's
   late states and claims are dropped. A removal is a break with nothing left behind. */
static void host_rule(body_sync_stand_t *st, endpoint_t *ep, int32_t index) {
    const body_sync_action_t *a = &st->scenario.actions[index];
    action_track_t *track = &st->actions[index];
    uint32_t scene_index = body_sync_scene_find_body(&st->scene, a->target.body);
    body_sync_id_t id = body_id_by_name(st, ep, a->target.body);
    stand_slot_t *s = id ? stand_world_slot(ep->world, id) : NULL;
    if (!s || s->character) {
        finish_action(st, ep, index, BODY_SYNC_OUTCOME_FAILED);
        return;
    }
    if (!st->local) {
        open_late(st, id);
    }
    if (a->verb == BODY_SYNC_VERB_REMOVE) {
        drop_if_held(ep, id);
        body_sync_unbind(ep->sync, id);
        stand_world_remove(ep->world, id);
        stand_record_t removal = {.type = STAND_REC_REMOVE, .target = id};
        broadcast(st, &removal);
    } else {
        if (body_sync_assign(ep->sync, id, BODY_SYNC_SEAT_HOST, BODY_SYNC_HOLD_UNTIL_RELEASE) != BODY_SYNC_OK) {
            finish_action(st, ep, index, BODY_SYNC_OUTCOME_FAILED);
            return;
        }
        /* A carrier on the host lets go too: the recall takes it out of every hand. */
        drop_if_held(ep, id);
        body_sync_set_holder(ep->sync, id, BODY_SYNC_ID_NONE);
        const body_sync_body_desc_t *home = &st->scene.bodies[scene_index];
        const float *at = a->has_home ? a->home : home->position;
        b3Body_SetType(s->body, s->desc.motion == BODY_SYNC_MOTION_KINEMATIC ? b3_kinematicBody : b3_dynamicBody);
        stand_world_set_held(ep->world, body_sync_id_slot(id), false, 0);
        b3Body_SetTransform(s->body, (b3Vec3){at[0], at[1], at[2]}, (b3Quat){{home->rotation[0], home->rotation[1], home->rotation[2]}, home->rotation[3]});
        b3Body_SetLinearVelocity(s->body, (b3Vec3){0.0f, 0.0f, 0.0f});
        b3Body_SetAngularVelocity(s->body, (b3Vec3){0.0f, 0.0f, 0.0f});
        b3Body_SetAwake(s->body, true);
        body_sync_authority_t auth;
        body_sync_authority(ep->sync, id, &auth);
        track->answer_number = auth.number;
    }
    begin_measure(st, index, id);
    track->shows = true;
    track->answered_on_host = true;
    finish_action(st, ep, index, BODY_SYNC_OUTCOME_DONE);
}

static void start_action(body_sync_stand_t *st, endpoint_t *ep, int32_t index) {
    const body_sync_action_t *a = &st->scenario.actions[index];
    action_track_t *track = &st->actions[index];
    stand_character_t *c = player_character(st, ep, a->player);
    track->started = true;
    track->start_tick = st->now;
    body_sync_id_t me = stand_world_id(ep->world, c->slot);
    float point[3];
    body_sync_id_t id;
    switch (a->verb) {
    case BODY_SYNC_VERB_WALK:
        target_point(st, ep, &a->target, point, &id);
        if (a->target.is_body && !id) {
            finish_action(st, ep, index, BODY_SYNC_OUTCOME_FAILED);
            return;
        }
        stand_character_walk(c, ep->world, point[0], point[2], id, a->speed);
        begin_measure(st, index, me);
        return;
    case BODY_SYNC_VERB_WAIT:
        stand_character_wait(c, a->wait_ticks);
        if (a->wait_ticks == 0) {
            finish_action(st, ep, index, BODY_SYNC_OUTCOME_DONE);
        }
        return;
    case BODY_SYNC_VERB_PUNCH:
        id = a->target.is_body ? body_id_by_name(st, ep, a->target.body) : BODY_SYNC_ID_NONE;
        stand_character_punch(c, ep->world, id);
        return;
    case BODY_SYNC_VERB_GRAB: {
        stand_character_stop(c, ep->world);
        id = body_id_by_name(st, ep, a->target.body);
        stand_slot_t *s = id ? stand_world_slot(ep->world, id) : NULL;
        if (!s || c->held || !(s->desc.flags & BODY_SYNC_BODY_GRABBABLE) || s->character ||
            !stand_character_in_reach(c, ep->world, body_sync_id_slot(id))) {
            finish_action(st, ep, index, BODY_SYNC_OUTCOME_FAILED);
            return;
        }
        if (is_host_ep(ep)) {
            float mine[3];
            stand_character_position(c, ep->world, mine);
            stand_target_t t = target_of(st, ep, id);
            if (!stand_rules_grab(&t, mine, st->scene.character.reach)) {
                finish_action(st, ep, index, BODY_SYNC_OUTCOME_REFUSED);
                return;
            }
        }
        if (body_sync_claim(ep->sync, id, BODY_SYNC_HOLD_UNTIL_RELEASE) != BODY_SYNC_OK) {
            finish_action(st, ep, index, BODY_SYNC_OUTCOME_FAILED);
            return;
        }
        stand_character_attach(c, ep->world, id);
        body_sync_set_holder(ep->sync, id, me);
        begin_measure(st, index, id);
        track->shows = true;
        track->answered = !is_host_ep(ep);
        if (is_host_ep(ep)) {
            track->host_ticks = 0;
            track->echo_ticks = 0;
            body_sync_authority_t auth;
            body_sync_authority(ep->sync, id, &auth);
            track->answer_number = auth.number;
            track->answered_on_host = true;
            finish_action(st, ep, index, BODY_SYNC_OUTCOME_GRANTED);
        } else {
            track->awaiting = true;
            finish_action(st, ep, index, BODY_SYNC_OUTCOME_PENDING);
        }
        return;
    }
    case BODY_SYNC_VERB_THROW:
    case BODY_SYNC_VERB_DROP: {
        if (!c->held) {
            finish_action(st, ep, index, BODY_SYNC_OUTCOME_FAILED);
            return;
        }
        body_sync_id_t held = c->held;
        if (a->verb == BODY_SYNC_VERB_THROW) {
            target_point(st, ep, &a->target, point, &id);
            float from[3], rot[4], velocity[3];
            pose_of(ep, body_sync_id_slot(held), from, rot);
            stand_ballistic(from, point, a->speed > 0.0f ? a->speed : st->scene.character.throw_speed, velocity);
            float dx = point[0] - from[0], dz = point[2] - from[2];
            if (dx * dx + dz * dz > 1e-6f) {
                c->yaw = atan2f(dx, dz);
            }
            stand_character_release(c, ep->world, velocity);
            stand_character_threw(c);
        } else {
            stand_character_release(c, ep->world, NULL);
        }
        body_sync_set_holder(ep->sync, held, BODY_SYNC_ID_NONE);
        body_sync_claim(ep->sync, held, BODY_SYNC_HOLD_UNTIL_SETTLED);
        begin_measure(st, index, held);
        track->shows = true;
        finish_action(st, ep, index, BODY_SYNC_OUTCOME_DONE);
        return;
    }
    case BODY_SYNC_VERB_RECALL:
    case BODY_SYNC_VERB_REMOVE:
        host_rule(st, ep, index);
        return;
    }
}

static void punch_landed(body_sync_stand_t *st, endpoint_t *ep, int32_t index) {
    stand_character_t *c = player_character(st, ep, st->scenario.actions[index].player);
    body_sync_id_t target = stand_character_punch_target(c, ep->world);
    if (!target) {
        set_outcome(st, index, BODY_SYNC_OUTCOME_MISSED);
        return;
    }
    float origin[3];
    stand_character_position(c, ep->world, origin);
    begin_measure(st, index, target);
    action_track_t *track = track_of(st, index);
    if (is_host_ep(ep)) {
        stand_target_t t = target_of(st, ep, target);
        stand_effect_t effect;
        if (stand_rules_hit(&t, origin, st->scene.character.punch_range, &effect) && apply_effect(st, effect, target, origin) != APPLIED_REFUSED) {
            set_outcome(st, index, BODY_SYNC_OUTCOME_HIT);
            if (track) {
                track->shows = true;
                track->host_ticks = 0;
                track->echo_ticks = 0;
                track->answered_on_host = true;
            }
        } else {
            set_outcome(st, index, BODY_SYNC_OUTCOME_REJECTED);
            if (track) {
                track->measuring = false;
            }
        }
        return;
    }
    stand_record_t r = {.type = STAND_REC_HIT, .target = target, .action = (uint16_t)index};
    memcpy(r.origin, origin, sizeof(r.origin));
    queue_record(st, ep, BODY_SYNC_SEAT_HOST, &r);
    if (track) {
        track->awaiting = true;
        track->shows = true;
        track->answered = true;
    }
}

static void act_player(body_sync_stand_t *st, endpoint_t *ep, uint8_t player, uint32_t tick) {
    stand_character_t *c = player_character(st, ep, player);
    if (!c || !c->present || !body_sync_is_local(ep->sync, stand_world_id(ep->world, c->slot))) {
        return;
    }
    int32_t previous = stand_schedule_active(&st->schedule, player);
    int32_t index = stand_schedule_next(&st->schedule, player, tick);
    if (index >= 0) {
        if (previous >= 0 && previous != index) {
            action_track_t *cut = track_of(st, previous);
            if (cut && !cut->ended) {
                cut->ended = true;
                if (cut->outcome == BODY_SYNC_OUTCOME_PENDING && !cut->awaiting) {
                    cut->outcome = BODY_SYNC_OUTCOME_FAILED;
                }
            }
            stand_character_stop(c, ep->world);
        }
        start_action(st, ep, index);
    }
}

static void act(body_sync_stand_t *st, endpoint_t *ep, uint32_t tick) {
    for (uint8_t p = 0; p < BODY_SYNC_ENDPOINTS_MAX; p++) {
        bool late = st->local && p != st->ref_walker;
        if (late && tick < st->ref_shift) {
            continue;
        }
        act_player(st, ep, p, late ? tick - st->ref_shift : tick);
    }
}

static void character_tick_player(body_sync_stand_t *st, endpoint_t *ep, uint8_t player) {
    stand_character_t *c = player_character(st, ep, player);
    if (!c || !c->present || !body_sync_is_local(ep->sync, stand_world_id(ep->world, c->slot))) {
        return;
    }
    stand_char_status_t status = stand_character_tick(c, ep->world);
    body_sync_id_t me = stand_world_id(ep->world, c->slot);
    uint8_t activity = (uint8_t)stand_character_activity(c);
    body_sync_set_data(ep->sync, me, &activity, 1);
    int32_t index = stand_schedule_active(&st->schedule, player);
    if (index < 0) {
        return;
    }
    const body_sync_action_t *a = &st->scenario.actions[index];
    if (status == STAND_CHAR_LANDED && a->verb == BODY_SYNC_VERB_PUNCH) {
        punch_landed(st, ep, index);
    } else if (status == STAND_CHAR_DONE) {
        action_track_t *track = track_of(st, index);
        finish_action(st, ep, index, a->verb == BODY_SYNC_VERB_PUNCH && track ? track->outcome : BODY_SYNC_OUTCOME_DONE);
    } else if (status == STAND_CHAR_FAILED) {
        finish_action(st, ep, index, BODY_SYNC_OUTCOME_FAILED);
    }
}

static void character_tick(body_sync_stand_t *st, endpoint_t *ep) {
    for (uint8_t p = 0; p < BODY_SYNC_ENDPOINTS_MAX; p++) {
        character_tick_player(st, ep, p);
    }
}

/* ---- host decisions ---- */

static void answer_claims(body_sync_stand_t *st) {
    endpoint_t *host = &st->ep[0];
    body_sync_claim_event_t claim;
    while (body_sync_poll_claim(host->sync, &claim)) {
        stand_target_t t = target_of(st, host, claim.id);
        float claimant[3] = {0.0f, -1e6f, 0.0f};
        uint32_t slot = character_slot(st, claim.seat);
        if (claim.seat < st->scene.player_count && host->world->slots[slot].live) {
            stand_character_position(&(stand_character_t){.slot = slot}, host->world, claimant);
        }
        body_sync_authority_t now;
        body_sync_authority(host->sync, claim.id, &now);
        bool granted = stand_rules_grab(&t, claimant, st->scene.character.reach) && (now.seat == BODY_SYNC_SEAT_HOST || now.seat == claim.seat);
        body_sync_answer(host->sync, &claim, granted);
        body_sync_authority(host->sync, claim.id, &now);
        for (uint32_t i = 0; i < st->scenario.action_count; i++) {
            action_track_t *a = &st->actions[i];
            if (a->awaiting && !a->answered_on_host && a->ref_id == claim.id && st->scenario.actions[i].verb == BODY_SYNC_VERB_GRAB &&
                st->scenario.actions[i].player == claim.seat) {
                a->answered_on_host = true;
                a->answer_number = now.number;
                a->host_ticks = (int32_t)(st->now - a->start_tick);
                a->seen_mask |= 1u;
                break;
            }
        }
    }
}

/* Where a knock by `agent` comes from on the host: the agent's pose there, or beside the target. */
static void impact_origin(endpoint_t *host, body_sync_id_t agent, const stand_target_t *t, float origin[3]) {
    float rot[4];
    if (stand_world_slot(host->world, agent)) {
        pose_of(host, body_sync_id_slot(agent), origin, rot);
        return;
    }
    memcpy(origin, t->position, sizeof(t->position));
    origin[0] -= 1.0f;
}

static bool apply_impact(body_sync_stand_t *st, stand_effect_t effect, body_sync_id_t agent, body_sync_id_t target, const stand_target_t *t) {
    float origin[3];
    impact_origin(&st->ep[0], agent, t, origin);
    return apply_effect(st, effect, target, origin) == APPLIED_DONE;
}

/* A guest's impact claim: once per key, through the seam's gate and the stand's limits, then the
   rule on the host's own target. The target's state dedupes across seats: a broken body is gone,
   a knocked one carries the flag, so a second seat's OK for the same contact applies nothing.
   A key enters its seat's ring before its verdict, so a refused claim is not judged again while
   the ring holds it; once the ring forgets it, a retry meets the same gate on what the host itself
   took of that body, which by then has aged past the claim's grace. */
static void judge_impact(body_sync_stand_t *st, uint8_t seat, const stand_record_t *r, const stand_target_t *t) {
    endpoint_t *host = &st->ep[0];
    if (!stand_claim_keys_add(&st->judged[seat], (stand_claim_key_t){.agent = r->body, .target = r->target, .number = r->number, .seat = seat})) {
        st->claims_repeat[seat]++;
        return;
    }
    body_sync_contact_t c;
    body_sync_contact_verdict_t verdict = body_sync_check_contact(host->sync, seat, r->body, r->number, r->target, r->tick, &c);
    if (verdict == BODY_SYNC_CONTACT_OK && c.ended) {
        st->claims_ended[seat]++;
    }
    if (verdict != BODY_SYNC_CONTACT_OK || !stand_impact_accepts(c.gap, c.reach * st->claim_reach_scale, c.approach_max * st->claim_approach_scale, r->speed)) {
        st->claims_rejected[seat]++;
        return;
    }
    if (apply_impact(st, stand_rules_impact(t, r->speed), r->body, r->target, t)) {
        st->claim_effects[seat]++;
    } else {
        st->claims_idle[seat]++;
    }
}

static void judge_hit(body_sync_stand_t *st, uint8_t seat, const stand_record_t *r, const stand_target_t *t) {
    endpoint_t *host = &st->ep[0];
    stand_effect_t effect;
    applied_t applied = stand_rules_hit(t, r->origin, st->scene.character.punch_range, &effect) ? apply_effect(st, effect, r->target, r->origin)
                                                                                                 : APPLIED_REFUSED;
    bool accepted = applied != APPLIED_REFUSED;
    stand_record_t verdict = {.type = STAND_REC_VERDICT, .target = r->target, .action = r->action, .accepted = accepted ? 1 : 0};
    if (seat < st->endpoints && st->ep[seat].active) {
        queue_record(st, host, seat, &verdict);
    }
    action_track_t *track = track_of(st, r->action);
    if (track && accepted) {
        track->answered_on_host = true;
    }
    if (applied == APPLIED_DONE) {
        st->claim_effects[seat]++;
    } else if (accepted) {
        st->claims_idle[seat]++;
    } else {
        st->claims_rejected[seat]++;
    }
}

static void answer_stand_claims(body_sync_stand_t *st) {
    endpoint_t *host = &st->ep[0];
    for (uint8_t seat = 1; seat < st->endpoints; seat++) {
        for (uint32_t i = 0; i < st->claim_count[seat]; i++) {
            const stand_record_t *r = &st->claims[seat][i].record;
            stand_target_t t = target_of(st, host, r->target);
            if (r->type == STAND_REC_HIT) {
                judge_hit(st, seat, r, &t);
            } else {
                judge_impact(st, seat, r, &t);
            }
        }
        st->claim_count[seat] = 0;
    }
}

/* The owner of a flying body judges its contacts with the host's own rule: the host applies its
   bodies' at once, a guest claims under the number its body flies with. A guest whose body names
   no number (a claim not yet granted) sends nothing: an effect missed, never a false one. */
static void impacts(body_sync_stand_t *st, endpoint_t *ep) {
    for (uint32_t i = 0; i < ep->world->hit_count; i++) {
        const stand_hit_t *h = &ep->world->hits[i];
        for (int side = 0; side < 2; side++) {
            uint32_t flying = side ? h->slot_b : h->slot_a;
            uint32_t other = side ? h->slot_a : h->slot_b;
            body_sync_id_t fid = stand_world_id(ep->world, flying);
            body_sync_id_t tid = stand_world_id(ep->world, other);
            body_sync_authority_t a;
            if (!fid || !tid || !body_sync_is_local(ep->sync, fid) || !body_sync_authority(ep->sync, fid, &a) ||
                (a.hold != BODY_SYNC_HOLD_UNTIL_SETTLED && a.hold != BODY_SYNC_HOLD_UNTIL_REST)) {
                continue;
            }
            stand_target_t t = target_of(st, ep, tid);
            stand_effect_t effect = stand_rules_impact(&t, h->speed);
            if (effect == STAND_EFFECT_NONE) {
                continue;
            }
            if (is_host_ep(ep)) {
                apply_impact(st, effect, fid, tid, &t);
                continue;
            }
            uint16_t number = 0;
            if (!body_sync_contact_number(ep->sync, fid, &number)) {
                continue;
            }
            /* Marked sent only once queued: a claim the full queue dropped goes again at the next hit. */
            stand_claim_key_t key = {.agent = fid, .target = tid, .number = number, .seat = ep->index};
            stand_record_t r = {.type = STAND_REC_IMPACT, .target = tid, .body = fid, .number = number, .speed = h->speed,
                                .tick = coop_session_tick(ep->session)};
            if (!stand_claim_keys_has(&ep->claimed, key) && queue_record(st, ep, BODY_SYNC_SEAT_HOST, &r)) {
                stand_claim_keys_add(&ep->claimed, key);
            }
        }
    }
}

/* ---- network ---- */

static void mark_left(body_sync_stand_t *st, uint8_t seat) {
    if (seat > 0 && seat < BODY_SYNC_ENDPOINTS_MAX && !st->left[seat]) {
        st->left[seat] = true;
        st->left_tick[seat] = st->now;
    }
}

/* The stand's rule for a player who left: what its character carried falls where it is. */
static void drop_carried_of(body_sync_stand_t *st, uint8_t seat) {
    endpoint_t *host = &st->ep[0];
    body_sync_id_t carrier = stand_world_id(host->world, character_slot(st, seat));
    for (uint32_t slot = 0; carrier && slot < STAND_BODIES_MAX; slot++) {
        body_sync_id_t id = stand_world_id(host->world, slot);
        body_sync_authority_t a;
        if (id && body_sync_authority(host->sync, id, &a) && a.holder == carrier && body_sync_is_local(host->sync, id)) {
            body_sync_set_holder(host->sync, id, BODY_SYNC_ID_NONE);
            body_sync_release(host->sync, id);
            b3Body_SetType(host->world->slots[slot].body, b3_dynamicBody);
            stand_world_set_held(host->world, slot, false, 0);
        }
    }
}

static void close_peer(body_sync_stand_t *st, endpoint_t *ep, uint8_t seat) {
    if (is_host_ep(ep)) {
        coop_session_kick(ep->session, seat, COOP_CLOSE_GAME_FIRST);
    } else {
        ep->active = false;
        mark_left(st, ep->index);
        stand_metrics_closed(&st->metrics, ep->index, st->now, COOP_CLOSE_FORMAT, coop_close_name(COOP_CLOSE_FORMAT));
    }
}

static void stand_record_in(body_sync_stand_t *st, endpoint_t *ep, uint8_t seat, const stand_record_t *r) {
    if (is_host_ep(ep)) {
        if (seat == BODY_SYNC_SEAT_HOST || seat >= BODY_SYNC_ENDPOINTS_MAX) {
            return;
        }
        st->claims_in[seat]++;
        if (st->claim_count[seat] < SEAT_CLAIMS_MAX) {
            st->claims[seat][st->claim_count[seat]++] = (pending_claim_t){.seat = seat, .record = *r};
        } else {
            st->claims_overflow[seat]++;
        }
        return;
    }
    switch (r->type) {
    case STAND_REC_VERDICT: {
        action_track_t *track = track_of(st, r->action);
        if (track && track->awaiting && st->scenario.actions[r->action].player == ep->index) {
            track->awaiting = false;
            track->outcome = r->accepted ? BODY_SYNC_OUTCOME_HIT : BODY_SYNC_OUTCOME_REJECTED;
            /* The echo is the answer showing on the actor, whether the delays are still watched. */
            track->echo_ticks = (int32_t)(st->now - track->start_tick);
            if (!r->accepted) {
                track->measuring = false;
            }
        }
        break;
    }
    case STAND_REC_SPAWN: {
        uint32_t slot = body_sync_id_slot(r->target);
        if (r->source >= st->scene.template_count ||
            !stand_world_spawn(ep->world, r->target, &st->scene.templates[r->source], r->position, r->rotation, r->velocity, r->angular)) {
            ep->claim_stale++;
            break;
        }
        body_sync_bind_t bind = {.body = ep->world->slots[slot].body, .body_class = BODY_SYNC_CLASS_LIGHT, .authority = BODY_SYNC_SEAT_HOST};
        body_sync_bind(ep->sync, r->target, &bind);
        if (ep->spawn_id[slot] != r->target) {
            ep->spawn_id[slot] = r->target;
            ep->spawn_count[slot] = 0;
        }
        ep->spawn_count[slot]++;
        break;
    }
    case STAND_REC_REMOVE:
        if (st->plant_keep_broken != 0 && st->plant_keep_broken == ep->index) {
            break;
        }
        drop_if_held(ep, r->target);
        body_sync_unbind(ep->sync, r->target);
        stand_world_remove(ep->world, r->target);
        break;
    default:
        break;
    }
}

/* A stand record's bytes after the family byte. A guest sends only claims: one the parser
   refuses is counted and dropped, it costs that claim and not the seat. A host's record that
   does not parse leaves the guest's world unknowable: false, and the session closes. */
static bool stand_bytes_in(body_sync_stand_t *st, endpoint_t *ep, uint8_t seat, uint32_t stamp, const uint8_t *data, uint32_t size) {
    stand_record_t r;
    char why[96];
    if (!stand_record_decode(data, size, seat == BODY_SYNC_SEAT_HOST, stamp, &st->limits, &r, why, sizeof(why))) {
        if (is_host_ep(ep) && seat < BODY_SYNC_ENDPOINTS_MAX) {
            st->claims_format[seat]++;
            return true;
        }
        return false;
    }
    stand_record_in(st, ep, seat, &r);
    return true;
}

static void poll_events(body_sync_stand_t *st, endpoint_t *ep) {
    coop_event_t ev;
    while (ep->active && coop_session_poll(ep->session, &ev)) {
        switch (ev.type) {
        case COOP_EVENT_WELCOME:
            body_sync_seat_joined(ep->sync, BODY_SYNC_SEAT_HOST);
            break;
        case COOP_EVENT_SEAT_JOINED:
            if (is_host_ep(ep)) {
                body_sync_seat_joined(ep->sync, ev.seat);
            }
            break;
        case COOP_EVENT_SEAT_LEFT:
            if (is_host_ep(ep) && ev.seat < BODY_SYNC_ENDPOINTS_MAX) {
                body_sync_seat_left(ep->sync, ev.seat);
                drop_carried_of(st, ev.seat);
                mark_left(st, ev.seat);
                stand_metrics_closed(&st->metrics, ev.seat, st->now, ev.reason, coop_close_name(ev.reason));
            }
            break;
        case COOP_EVENT_CLOSED:
            ep->active = false;
            mark_left(st, ep->index);
            stand_metrics_closed(&st->metrics, ep->index, st->now, ev.reason, coop_close_name(ev.reason));
            break;
        case COOP_EVENT_STATE:
            if (body_sync_receive_state(ep->sync, ev.seat, ev.tick + st->plant_shift, ev.data, ev.size) == BODY_SYNC_FORMAT) {
                close_peer(st, ep, ev.seat);
            }
            break;
        case COOP_EVENT_RECORD: {
            bool ok = ev.size >= 1;
            if (ok && ev.data[0] == STAND_FAMILY_SEAM) {
                ok = body_sync_receive_record(ep->sync, ev.seat, ev.tick, ev.data + 1, ev.size - 1) != BODY_SYNC_FORMAT;
            } else if (ok && ev.data[0] == STAND_FAMILY_STAND) {
                ok = stand_bytes_in(st, ep, ev.seat, ev.tick, ev.data + 1, ev.size - 1);
            } else {
                ok = false;
            }
            if (!ok) {
                close_peer(st, ep, ev.seat);
            }
            break;
        }
        case COOP_EVENT_BASELINE:
            break;
        }
    }
}

static int guest_of(const endpoint_t *ep, uint8_t seat) { return is_host_ep(ep) ? (int)seat - 1 : (int)ep->index - 1; }

static void send_all(body_sync_stand_t *st, endpoint_t *ep) {
    uint32_t cap_state = coop_session_state_max(ep->session);
    uint32_t cap_record = coop_session_record_max(ep->session);
    cap_state = cap_state > PACKET_MAX ? PACKET_MAX : cap_state;
    cap_record = cap_record > PACKET_MAX ? PACKET_MAX : cap_record;
    for (uint8_t seat = 0; seat < st->endpoints; seat++) {
        if (seat == ep->index || (!is_host_ep(ep) && seat != BODY_SYNC_SEAT_HOST) || !coop_session_seated(ep->session, seat)) {
            continue;
        }
        int guest = guest_of(ep, seat);
        int direction = is_host_ep(ep) ? STAND_DIR_TO_GUEST : STAND_DIR_TO_HOST;
        uint32_t size = 0, flushed = 0;
        while (body_sync_write_state(ep->sync, seat, st->packet, cap_state, &size)) {
            uint16_t number = 0;
            flushed += size;
            if (coop_session_send_state(ep->session, seat, st->packet, size, &number) == COOP_SEND_OK) {
                body_sync_state_sent(ep->sync, seat, number);
            }
        }
        uint16_t numbers[64];
        uint32_t n = body_sync_unresolved(ep->sync, seat, numbers, 64);
        for (uint32_t i = 0; i < n; i++) {
            coop_packet_fate_t fate = coop_session_packet_fate(ep->session, seat, numbers[i]);
            if (fate != COOP_PACKET_PENDING) {
                body_sync_resolve(ep->sync, seat, numbers[i], fate == COOP_PACKET_ARRIVED);
            }
        }
        bool seam_left = false;
        while (cap_record > 1 && body_sync_peek_record(ep->sync, seat, st->packet + 1, cap_record - 1, &size)) {
            st->packet[0] = STAND_FAMILY_SEAM;
            if (coop_session_send_record(ep->session, seat, st->packet, size + 1) != COOP_SEND_OK) {
                seam_left = true;
                break;
            }
            body_sync_pop_record(ep->sync, seat);
            flushed += size + 1;
            if (guest >= 0) {
                stand_metrics_bytes(&st->metrics, (uint32_t)guest, direction, BODY_SYNC_TRAFFIC_AUTHORITY, 1, st->now);
            }
        }
        /* The stand's records wait behind the seam's: a claim must not overtake the release or
           take it follows, even when it alone would fit. */
        out_queue_t *q = &ep->queues[seat];
        while (!seam_left && q->count > 0) {
            if (coop_session_send_record(ep->session, seat, q->data[q->head], q->size[q->head]) != COOP_SEND_OK) {
                break;
            }
            flushed += q->size[q->head];
            if (guest >= 0) {
                stand_metrics_bytes(&st->metrics, (uint32_t)guest, direction, BODY_SYNC_SEAM_KINDS + q->kind[q->head], q->size[q->head], st->now);
            }
            q->head = (q->head + 1) % QUEUE_MAX;
            q->count--;
        }
        if (guest >= 0 && flushed > cap_state) {
            st->flush_over[guest]++;
        }
    }
}

/* ---- one endpoint tick ---- */

static void after_apply(body_sync_stand_t *st, endpoint_t *ep) {
    stand_world_t *w = ep->world;
    for (uint32_t slot = 0; slot < STAND_BODIES_MAX; slot++) {
        stand_slot_t *s = &w->slots[slot];
        if (!s->live || s->character || s->desc.motion == BODY_SYNC_MOTION_STATIC) {
            continue;
        }
        body_sync_authority_t a;
        body_sync_id_t id = stand_world_id(w, slot);
        if (body_sync_authority(ep->sync, id, &a)) {
            /* The seam keeps the character bits on a layer copy: its own character pushes it or meets it. */
            bool light_copy = !body_sync_is_local(ep->sync, id) && !(s->desc.flags & BODY_SYNC_BODY_HEAVY) && !body_sync_is_layer_copy(ep->sync, id);
            stand_world_set_held(w, slot, a.holder != BODY_SYNC_ID_NONE, light_copy ? (uint64_t)STAND_CAT_CHARACTER : 0u);
        }
        if ((body_sync_flags(ep->sync, stand_world_id(w, slot)) & KNOCK_FLAG) != 0) {
            s->knocked = true;
        }
        float rot[4];
        pose_of(ep, slot, ep->tracks[slot].applied, rot);
        ep->tracks[slot].applied_copy = !body_sync_is_local(ep->sync, id);
    }
    stand_character_t *c = &ep->character;
    if (c->held && !body_sync_is_local(ep->sync, c->held)) {
        c->held = BODY_SYNC_ID_NONE;
        c->held_heavy = false;
    }
    (void)st;
}

/* The poses this endpoint simulated this tick, after its step, under the session tick k its
   state packets carry: the units of the copies' playback ticks. */
static void record_owner(endpoint_t *ep, uint32_t k) {
    ep->ring_any = true;
    ep->ring_newest = k;
    for (uint32_t slot = 0; slot < STAND_BODIES_MAX; slot++) {
        stand_slot_t *s = &ep->world->slots[slot];
        if (!s->live || s->desc.motion == BODY_SYNC_MOTION_STATIC) {
            continue;
        }
        body_sync_id_t id = stand_world_id(ep->world, slot);
        owner_ring_t *r = &ep->rings[slot];
        if (r->id != id) {
            memset(r, 0, sizeof(*r));
            r->id = id;
        }
        if (!body_sync_is_local(ep->sync, id)) {
            continue;
        }
        float rot[4];
        pose_of(ep, slot, r->position[k % RING_TICKS], rot);
        r->tag[k % RING_TICKS] = k + 1u;
    }
}

static bool owner_at(const owner_ring_t *r, double played, float out[3]);
static bool moving_at(const owner_ring_t *r, double played);

/* A predicted copy's pause, as the lag-aligned gap reads it (sample_aligned). */
enum {
    PAUSE_NONE = 0,
    PAUSE_FROM_REST, /* its owner rested at the tick the copy showed when the pause began */
    PAUSE_IN_FLIGHT, /* its owner moved it then, a third seat owned it, or the stand cannot tell */
};

/* At a pause's first tick: whether the owner's own picture at the tick the copy plays moves. A
   guest's copy of another guest's body plays a relayed clock the stand cannot map to that owner's
   ticks, and each side meets that owner's motion at its own stage, so the host's answer at the
   guest's moment is no picture of what this copy shows: it compares at the playback tick. */
static uint8_t pause_mark(const body_sync_stand_t *st, const endpoint_t *ep, uint32_t slot, body_sync_id_t id) {
    body_sync_authority_t a;
    double played = 0.0;
    if (!body_sync_authority(ep->sync, id, &a) || a.seat >= st->endpoints || !st->ep[a.seat].drawn_rings || !body_sync_playback(ep->sync, id, &played) ||
        (!is_host_ep(ep) && a.seat != BODY_SYNC_SEAT_HOST)) {
        return PAUSE_IN_FLIGHT;
    }
    const owner_ring_t *r = &st->ep[a.seat].drawn_rings[slot];
    float at[3];
    if (r->id != id || !owner_at(r, played, at) || !owner_at(r, played - 1.0, at)) {
        return PAUSE_IN_FLIGHT;
    }
    return moving_at(r, played) ? PAUSE_IN_FLIGHT : PAUSE_FROM_REST;
}

/* What this endpoint draws of every body after its step, under the session tick k. */
static void record_drawn(const body_sync_stand_t *st, endpoint_t *ep, uint32_t k) {
    if (!ep->drawn_rings || !ep->drawn_paused || !ep->drawn_turns) {
        return;
    }
    for (uint32_t slot = 0; slot < STAND_BODIES_MAX; slot++) {
        stand_slot_t *s = &ep->world->slots[slot];
        if (!s->live || s->desc.motion == BODY_SYNC_MOTION_STATIC) {
            continue;
        }
        body_sync_id_t id = stand_world_id(ep->world, slot);
        owner_ring_t *r = &ep->drawn_rings[slot];
        if (r->id != id) {
            memset(r, 0, sizeof(*r));
            r->id = id;
            ep->pause_reset[slot] = true;
        }
        bool planted = ep->index == 1u && st->plant_pause_slot == slot + 1u && st->now - st->plant_pause_tick < st->plant_pause_ticks;
        /* The window counts one prediction's pauses since it was last taken back: its start (a copy
           held or driven before), a snap or a stalled pull's placement; each starts a new chain. */
        if (ep->pause_reset[slot] || (!body_sync_is_predicted(ep->sync, id) && !planted)) {
            ep->pause_window[slot] = 0;
            ep->pause_since[slot] = k;
            ep->pause_reset[slot] = false;
        }
        /* The paused tick that leaves the window, while the ring still holds it. */
        if (k >= PAUSE_WINDOW && k - PAUSE_WINDOW >= ep->pause_since[slot] && r->tag[(k - PAUSE_WINDOW) % RING_TICKS] == k - PAUSE_WINDOW + 1u &&
            ep->drawn_paused[(size_t)slot * RING_TICKS + (k - PAUSE_WINDOW) % RING_TICKS] && ep->pause_window[slot] > 0) {
            ep->pause_window[slot]--;
        }
        float rot[4], *at = r->position[k % RING_TICKS];
        drawn_pose(ep, slot, at, rot);
        memcpy(ep->drawn_turns[(size_t)slot * RING_TICKS + k % RING_TICKS], rot, sizeof(rot));
        if (!is_host_ep(ep) && st->plant_drift != 0.0f && body_sync_is_local(ep->sync, id) && !s->character) {
            at[0] += st->plant_drift;
        }
        if (!is_host_ep(ep) && body_sync_is_predicted(ep->sync, id) && body_sync_is_pull_paused(ep->sync, id)) {
            at[0] += st->plant_lead;
        }
        bool went_on = r->tag[(k - 1u) % RING_TICKS] == k; /* the tick before is recorded */
        r->tag[k % RING_TICKS] = k + 1u;
        uint8_t *paused = &ep->drawn_paused[(size_t)slot * RING_TICKS];
        uint8_t mark = 0;
        if (body_sync_is_pull_paused(ep->sync, id) || planted) {
            mark = went_on ? paused[(k - 1u) % RING_TICKS] : 0u;
            mark = mark ? mark : pause_mark(st, ep, slot, id);
        }
        paused[k % RING_TICKS] = mark;
        if (mark) {
            ep->pause_window[slot]++;
            ep->pause_window_max = ep->pause_window[slot] > ep->pause_window_max ? ep->pause_window[slot] : ep->pause_window_max;
        }
    }
}

/* A copy that moves while it touches one this guest's body touched, or one of her own, is shoved
   through the pile by her: her doing as much as a direct touch (the rest lag's bound covers a
   copy moving on after a hit, not one she keeps pushing). Only moving copies pass it on, so a pile
   at rest that she leans on is no shove. */
static void note_shoves(body_sync_stand_t *st, endpoint_t *ep) {
    if (is_host_ep(ep)) {
        return;
    }
    for (bool grew = true; grew;) {
        grew = false;
        for (uint32_t slot = 0; slot < STAND_BODIES_MAX; slot++) {
            const stand_slot_t *c = &ep->world->slots[slot];
            body_sync_id_t id = stand_world_id(ep->world, slot);
            if (!c->live || !id || c->character || c->desc.motion == BODY_SYNC_MOTION_STATIC || body_sync_is_local(ep->sync, id) ||
                ep->touched_tick[slot] == st->now + 1u || stand_world_speed(ep->world, slot) < REST_STILL) {
                continue;
            }
            b3ContactData contacts[16];
            int n = b3Body_GetContactData(c->body, contacts, 16);
            for (int i = 0; i < n && ep->touched_tick[slot] != st->now + 1u; i++) {
                if (contacts[i].manifoldCount == 0 || contacts[i].manifolds[0].pointCount == 0 || !b3Shape_IsValid(contacts[i].shapeIdA) ||
                    !b3Shape_IsValid(contacts[i].shapeIdB)) {
                    continue;
                }
                b3BodyId ba = b3Shape_GetBody(contacts[i].shapeIdA), bb = b3Shape_GetBody(contacts[i].shapeIdB);
                uintptr_t tag = (uintptr_t)b3Body_GetUserData(B3_ID_EQUALS(ba, c->body) ? bb : ba);
                if (tag == 0 || tag > STAND_BODIES_MAX) {
                    continue;
                }
                uint32_t other = (uint32_t)(tag - 1u);
                body_sync_id_t oid = stand_world_id(ep->world, other);
                if (ep->touched_tick[other] == st->now + 1u || (oid && body_sync_is_local(ep->sync, oid))) {
                    ep->touched_tick[slot] = st->now + 1u;
                    grew = true;
                }
            }
        }
    }
}

/* A guest's step hit a LIGHT copy with a body it simulates: the hit waits for its response. */
static void note_hits(body_sync_stand_t *st, endpoint_t *ep) {
    if (is_host_ep(ep)) {
        return;
    }
    for (uint32_t i = 0; i < ep->world->hit_count; i++) {
        const stand_hit_t *h = &ep->world->hits[i];
        for (int side = 0; side < 2; side++) {
            uint32_t mine = side ? h->slot_b : h->slot_a, copy = side ? h->slot_a : h->slot_b;
            const stand_slot_t *c = &ep->world->slots[copy];
            body_sync_id_t mid = stand_world_id(ep->world, mine), cid = stand_world_id(ep->world, copy);
            if (!mid || !cid || !body_sync_is_local(ep->sync, mid) || body_sync_is_local(ep->sync, cid) || c->character ||
                c->desc.motion == BODY_SYNC_MOTION_STATIC || (c->desc.flags & BODY_SYNC_BODY_HEAVY)) {
                continue;
            }
            ep->touched_tick[copy] = st->now + 1u;
            if (h->speed <= 0.0f) {
                continue; /* a graze: it touches, it does not hit */
            }
            bool pending = false;
            for (uint32_t k = 0; k < ep->hit_count; k++) {
                pending = pending || ep->hits[k].slot == copy;
            }
            const stand_slot_t *hs = stand_world_slot(st->ep[0].world, cid);
            if (pending || ep->hit_count >= 64 || !hs || ep->hist_id[copy] != cid) {
                continue;
            }
            float rot[4];
            uint32_t k = ep->hit_count++;
            ep->hits[k].slot = copy;
            ep->hits[k].id = cid;
            ep->hits[k].tick = st->now;
            ep->hits[k].host_moved = ep->hits[k].moved = ep->hits[k].counted = false;
            ep->hits[k].answer = 0;
            memcpy(ep->hits[k].drawn, ep->prev_pos[copy], sizeof(ep->hits[k].drawn)); /* drawn before the step */
            pose_of(&st->ep[0], copy, ep->hits[k].host, rot);
        }
    }
}

/* stand_testing_plant_wall: the copy stays where it is, driven, whatever the seam or a push does. */
static void plant_wall(body_sync_stand_t *st, endpoint_t *ep) {
    for (uint32_t k = 0; k < st->plant_wall_count && ep->index == st->plant_wall_endpoint; k++) {
        uint32_t slot = st->plant_wall_slots[k];
        stand_slot_t *s = &ep->world->slots[slot];
        if (!s->live || body_sync_is_local(ep->sync, stand_world_id(ep->world, slot))) {
            continue;
        }
        b3Body_SetType(s->body, b3_kinematicBody);
        b3Body_SetLinearVelocity(s->body, (b3Vec3){0.0f, 0.0f, 0.0f});
        b3Body_SetAngularVelocity(s->body, (b3Vec3){0.0f, 0.0f, 0.0f});
    }
}

static void endpoint_tick(body_sync_stand_t *st, endpoint_t *ep) {
    uint32_t k = ep->ticks_run;
    coop_session_set_time(ep->session, net_link_sim_time(st->sim));
    if (st->now >= ep->deaf_until) {
        poll_events(st, ep);
    }
    if (!ep->active) {
        return;
    }
    for (uint8_t seat = 0; seat < st->endpoints; seat++) {
        if (seat != ep->index) {
            body_sync_set_peer_tick(ep->sync, seat, coop_session_peer_tick(ep->session, seat));
        }
    }
    /* The seam counts in the tick its state packets carry, so a claim's tick, a local body's
       playback tick and a copy's owner tick share one unit with the owner rings. */
    uint32_t seam_tick = coop_session_tick(ep->session);
    body_sync_apply(ep->sync, seam_tick);
    after_apply(st, ep);
    plant_wall(st, ep);
    act(st, ep, k);
    for (uint32_t slot = 0; slot < STAND_BODIES_MAX; slot++) {
        stand_slot_t *s = &ep->world->slots[slot];
        if (s->live && !s->character && s->desc.motion != BODY_SYNC_MOTION_STATIC) {
            pose_of(ep, slot, ep->tracks[slot].acted, ep->tracks[slot].acted_rot);
        }
    }
    if (is_host_ep(ep)) {
        answer_claims(st);
        answer_stand_claims(st);
        stand_world_walk_routes(ep->world, ep->sync);
    }
    character_tick(st, ep);
    stand_world_step(ep->world);
    note_hits(st, ep);
    note_shoves(st, ep);
    record_owner(ep, coop_session_tick(ep->session)); /* the tick its state packets carry */
    record_drawn(st, ep, coop_session_tick(ep->session));
    impacts(st, ep);
    /* stand_testing_plant_declared_velocity: what collect reads, and then the body's own again. */
    b3BodyId lied = b3_nullBodyId;
    b3Vec3 true_velocity = {0.0f, 0.0f, 0.0f};
    if (st->plant_declared_ticks > 0 && ep->index == st->plant_declared_endpoint && stand_world_slot(ep->world, st->plant_declared_body)) {
        lied = ep->world->slots[body_sync_id_slot(st->plant_declared_body)].body;
        true_velocity = b3Body_GetLinearVelocity(lied);
        const float *add = st->plant_declared_add;
        b3Body_SetLinearVelocity(lied, (b3Vec3){true_velocity.x + add[0], true_velocity.y + add[1], true_velocity.z + add[2]});
        st->plant_declared_ticks--;
    }
    body_sync_collect(ep->sync, seam_tick);
    if (B3_IS_NON_NULL(lied)) {
        b3Body_SetLinearVelocity(lied, true_velocity);
    }
    send_all(st, ep);
    coop_session_end_tick(ep->session);
    ep->ticks_run++;
}

/* ---- network items ---- */

static void apply_conditions(body_sync_stand_t *st, uint8_t guest) {
    endpoint_t *ep = &st->ep[guest];
    if (!ep->link) {
        return;
    }
    net_link_sim_set_conditions(st->sim, ep->link, NET_LINK_SIM_TO_HOST, &st->conditions[guest][0]);
    net_link_sim_set_conditions(st->sim, ep->link, NET_LINK_SIM_TO_GUEST, &st->conditions[guest][1]);
}

static void fire_net(body_sync_stand_t *st, const body_sync_net_event_t *e) {
    if (st->local || e->endpoint >= st->endpoints) {
        return;
    }
    if (e->kind != BODY_SYNC_NET_CONDITIONS && e->duration_ticks > st->frozen_max) {
        st->frozen_max = e->duration_ticks;
    }
    switch (e->kind) {
    case BODY_SYNC_NET_CONDITIONS:
        for (int dir = 0; dir < 2; dir++) {
            if (!(e->directions & (dir == 0 ? BODY_SYNC_TO_HOST : BODY_SYNC_TO_GUEST))) {
                continue;
            }
            net_link_conditions_t *c = &st->conditions[e->endpoint][dir];
            if (e->fields & BODY_SYNC_FIELD_DELAY) c->delay_ms = e->conditions.delay_ms;
            if (e->fields & BODY_SYNC_FIELD_JITTER) c->jitter_ms = e->conditions.jitter_ms;
            if (e->fields & BODY_SYNC_FIELD_LOSS) c->loss = e->conditions.loss;
            if (e->fields & BODY_SYNC_FIELD_DUPLICATE) c->duplicate = e->conditions.duplicate;
            if (e->fields & BODY_SYNC_FIELD_REORDER) c->reorder = e->conditions.reorder;
            if (e->fields & BODY_SYNC_FIELD_REORDER_DELAY) c->reorder_ms = e->conditions.reorder_ms;
            if (e->fields & BODY_SYNC_FIELD_RATE) c->rate_bytes_per_s = e->conditions.rate_bytes_per_s;
            float way = c->delay_ms + c->jitter_ms + (c->reorder > 0.0f ? c->reorder_ms : 0.0f);
            st->one_way_max_ms = way > st->one_way_max_ms ? way : st->one_way_max_ms;
        }
        apply_conditions(st, e->endpoint);
        break;
    case BODY_SYNC_NET_CUT:
        if (st->ep[e->endpoint].link) {
            net_link_sim_set_cut(st->sim, st->ep[e->endpoint].link, true);
            st->cut_until[e->endpoint] = e->tick + e->duration_ticks;
        }
        break;
    case BODY_SYNC_NET_STALL:
        st->ep[e->endpoint].stall_until = e->tick + e->duration_ticks;
        break;
    case BODY_SYNC_NET_DEAF:
        st->ep[e->endpoint].deaf_until = e->tick + e->duration_ticks;
        break;
    }
}

/* ---- sampling ---- */

static bool moved(const float a[3], const float b[3], float by) { return stand_distance3(a, b) >= by; }

static void track_actions(body_sync_stand_t *st) {
    for (uint32_t i = 0; i < st->scenario.action_count; i++) {
        action_track_t *a = &st->actions[i];
        const body_sync_action_t *act_desc = &st->scenario.actions[i];
        uint8_t actor = act_desc->player;
        if (!a->started) {
            continue;
        }
        endpoint_t *actor_ep = &st->ep[actor];
        /* A guest's grab is answered by the host's assignment as the actor's seam sees it. */
        if (a->awaiting && act_desc->verb == BODY_SYNC_VERB_GRAB && actor_ep->active) {
            body_sync_authority_t auth;
            if (!body_sync_authority(actor_ep->sync, a->ref_id, &auth)) {
                a->awaiting = false;
                a->outcome = BODY_SYNC_OUTCOME_REFUSED;
            } else if (auth.seat == actor) {
                a->awaiting = false;
                a->outcome = BODY_SYNC_OUTCOME_GRANTED;
                a->echo_ticks = (int32_t)(st->now - a->start_tick);
            } else if (!body_sync_is_local(actor_ep->sync, a->ref_id)) {
                a->awaiting = false;
                a->outcome = BODY_SYNC_OUTCOME_REFUSED;
                a->echo_ticks = (int32_t)(st->now - a->start_tick);
            }
        }
        if (!a->measuring) {
            continue;
        }
        if (!a->ref_set) {
            for (uint32_t e = 0; e < st->endpoints; e++) {
                stand_slot_t *s = st->ep[e].built ? stand_world_slot(st->ep[e].world, a->ref_id) : NULL;
                if (s) {
                    float rot[4];
                    drawn_pose(&st->ep[e], body_sync_id_slot(a->ref_id), a->ref_pos[e], rot);
                }
            }
            a->ref_set = true;
            continue;
        }
        int32_t elapsed = (int32_t)(st->now - a->start_tick);
        if (act_desc->verb == BODY_SYNC_VERB_THROW && a->own_ticks < 0 && actor_ep->built && stand_world_slot(actor_ep->world, a->ref_id)) {
            float at[3], rot[4];
            drawn_pose(actor_ep, body_sync_id_slot(a->ref_id), at, rot);
            if (moved(at, a->ref_pos[actor], MOVED_M)) {
                a->own_ticks = elapsed;
            }
        }
        /* A walk counts for the delays only once its own character has moved. */
        if (act_desc->verb == BODY_SYNC_VERB_WALK && !a->shows && actor_ep->built && stand_world_slot(actor_ep->world, a->ref_id)) {
            float at[3], rot[4];
            drawn_pose(actor_ep, body_sync_id_slot(a->ref_id), at, rot);
            a->shows = moved(at, a->ref_pos[actor], MOVED_M);
        }
        uint8_t all = 0;
        for (uint32_t e = 0; e < st->endpoints; e++) {
            if (e != actor && st->ep[e].active) {
                all |= (uint8_t)(1u << e);
            }
        }
        for (uint32_t e = 0; e < st->endpoints; e++) {
            endpoint_t *ep = &st->ep[e];
            if (e == actor || !ep->active || (a->seen_mask & (1u << e))) {
                continue;
            }
            bool shown = false;
            stand_slot_t *s = stand_world_slot(ep->world, a->ref_id);
            switch (act_desc->verb) {
            case BODY_SYNC_VERB_WALK:
            case BODY_SYNC_VERB_THROW:
            case BODY_SYNC_VERB_DROP:
                if (s) {
                    float at[3], rot[4];
                    drawn_pose(ep, body_sync_id_slot(a->ref_id), at, rot);
                    shown = moved(at, a->ref_pos[e], MOVED_M);
                }
                break;
            case BODY_SYNC_VERB_GRAB:
            case BODY_SYNC_VERB_RECALL: {
                body_sync_authority_t auth;
                shown = a->answered_on_host && body_sync_authority(ep->sync, a->ref_id, &auth) &&
                        (int16_t)(uint16_t)(auth.number - a->answer_number) >= 0;
                break;
            }
            case BODY_SYNC_VERB_REMOVE:
                shown = !s;
                break;
            case BODY_SYNC_VERB_PUNCH:
                if (a->outcome == BODY_SYNC_OUTCOME_HIT || is_host_ep(ep)) {
                    if (!s) {
                        shown = true;
                    } else if (s->knocked || (body_sync_flags(ep->sync, a->ref_id) & KNOCK_FLAG)) {
                        shown = true;
                    } else {
                        float at[3], rot[4];
                        drawn_pose(ep, body_sync_id_slot(a->ref_id), at, rot);
                        shown = moved(at, a->ref_pos[e], MOVED_M);
                    }
                }
                break;
            case BODY_SYNC_VERB_WAIT:
                break;
            }
            if (shown) {
                a->seen_mask |= (uint8_t)(1u << e);
                if (e == BODY_SYNC_ENDPOINT_HOST && a->host_ticks < 0) {
                    a->host_ticks = elapsed;
                }
            }
        }
        if ((a->seen_mask & all) == all && all != 0) {
            a->seen_ticks = elapsed;
            a->measuring = false;
        } else if (all == 0) {
            a->measuring = false;
        }
        if (elapsed > 10 * BODY_SYNC_TICK_HZ) {
            a->measuring = false;
        }
    }
}

static float delta_length(const float a[3], const float b[3]) { return stand_distance3(a, b); }

static bool ring_at(const owner_ring_t *r, int64_t k, float out[3]) {
    if (k < 0 || r->tag[(uint64_t)k % RING_TICKS] != (uint32_t)k + 1u) {
        return false;
    }
    memcpy(out, r->position[(uint64_t)k % RING_TICKS], sizeof(float) * 3);
    return true;
}

/* The owner's position at owner tick `played`, between its two ticks. */
static bool owner_at(const owner_ring_t *r, double played, float out[3]) {
    double base = floor(played);
    float a[3], b[3];
    if (!ring_at(r, (int64_t)base, a)) {
        return false;
    }
    float f = (float)(played - base);
    if (f <= 0.0f) {
        memcpy(out, a, sizeof(a));
        return true;
    }
    if (!ring_at(r, (int64_t)base + 1, b)) {
        return false;
    }
    for (int i = 0; i < 3; i++) {
        out[i] = a[i] + (b[i] - a[i]) * f;
    }
    return true;
}

static double segment_distance(const float p[3], const float a[3], const float b[3]) {
    double ab[3], ap[3], dot = 0.0, len = 0.0;
    for (int i = 0; i < 3; i++) {
        ab[i] = (double)b[i] - (double)a[i];
        ap[i] = (double)p[i] - (double)a[i];
        dot += ab[i] * ap[i];
        len += ab[i] * ab[i];
    }
    double u = len > 0.0 ? dot / len : 0.0;
    u = u < 0.0 ? 0.0 : u > 1.0 ? 1.0 : u;
    double d = 0.0;
    for (int i = 0; i < 3; i++) {
        double e = ap[i] - ab[i] * u;
        d += e * e;
    }
    return sqrt(d);
}

/* The distance from `at` to the owner's path over [played - PATH_TICKS, played + PATH_TICKS]. */
static double path_error(const owner_ring_t *r, double played, const float at[3]) {
    int64_t mid = (int64_t)floor(played);
    double best = (double)INFINITY;
    float prev[3], cur[3];
    bool have = false;
    for (int64_t k = mid - PATH_TICKS; k <= mid + PATH_TICKS; k++) {
        if (!ring_at(r, k, cur)) {
            have = false;
            continue;
        }
        double d = have ? segment_distance(at, prev, cur) : segment_distance(at, cur, cur);
        best = d < best ? d : best;
        memcpy(prev, cur, sizeof(cur));
        have = true;
    }
    return best;
}

/* The owner's move over owner ticks [from, to]. Across a tick where the owner itself jumped
   (a grab sets a body in the hands) the interpolated move understates it: the move is then
   that of the whole ticks around the interval, so a copy that shows the owner's own jump at
   another fraction of a tick is not counted as one. */
static double owner_move(const owner_ring_t *r, double from, double to, const float a[3], const float b[3]) {
    double d = (double)stand_distance3(a, b);
    int64_t k0 = (int64_t)floor(from), k1 = (int64_t)ceil(to);
    float first[3], prev[3], cur[3];
    bool have = false, jumped = false;
    for (int64_t k = k0; k <= k1; k++) {
        if (!ring_at(r, k, cur)) {
            continue;
        }
        if (!have) {
            memcpy(first, cur, sizeof(cur));
        }
        jumped = jumped || (have && stand_distance3(prev, cur) > OWNER_JUMP_M);
        memcpy(prev, cur, sizeof(cur));
        have = true;
    }
    double whole = have ? (double)stand_distance3(first, prev) : 0.0;
    return jumped && whole > d ? whole : d;
}

/* The second layer's measures on each guest: hits answered, copies at rest, copy speeds. */
/* A light copy this guest's own character walks into without touching it (driven, its character
   bits off: she passes through) is a hit the layer missed: counted with the misses, once per
   entry. Into means her capsule, PASS_M thinner, overlaps the copy's shapes with no contact
   between the bodies. */
#define PASS_M 0.03f
typedef struct passing {
    body_sync_stand_t *st;
    endpoint_t *ep;
    b3BodyId her;
    bool inside[STAND_BODIES_MAX];
} passing_t;

static bool in_contact(b3BodyId a, b3BodyId b) {
    b3ContactData contacts[32];
    int n = b3Body_GetContactData(a, contacts, 32);
    for (int i = 0; i < n; i++) {
        if (b3Shape_IsValid(contacts[i].shapeIdA) && b3Shape_IsValid(contacts[i].shapeIdB) &&
            (B3_ID_EQUALS(b3Shape_GetBody(contacts[i].shapeIdA), b) || B3_ID_EQUALS(b3Shape_GetBody(contacts[i].shapeIdB), b))) {
            return true;
        }
    }
    return false;
}

static bool passing_shape(b3ShapeId shape, void *context) {
    passing_t *ps = context;
    b3BodyId body = b3Shape_GetBody(shape);
    for (uint32_t slot = 0; slot < STAND_BODIES_MAX; slot++) {
        stand_slot_t *s = &ps->ep->world->slots[slot];
        if (s->live && B3_ID_EQUALS(s->body, body)) {
            body_sync_id_t id = stand_world_id(ps->ep->world, slot);
            ps->inside[slot] = !s->character && s->desc.motion != BODY_SYNC_MOTION_STATIC && !(s->desc.flags & BODY_SYNC_BODY_HEAVY) &&
                               s->desc.mass <= ps->st->light_mass && !body_sync_is_local(ps->ep->sync, id) &&
                               !body_sync_is_layer_copy(ps->ep->sync, id) && !in_contact(ps->her, body);
            break;
        }
    }
    return true;
}

static void passed_through(body_sync_stand_t *st, endpoint_t *ep, uint32_t gi) {
    if (st->light_mass <= 0.0f) {
        return;
    }
    static passing_t ps;
    memset(ps.inside, 0, sizeof(ps.inside));
    ps.st = st;
    ps.ep = ep;
    for (uint32_t c = 0; c < STAND_BODIES_MAX; c++) {
        stand_slot_t *ch = &ep->world->slots[c];
        b3ShapeId shape;
        if (!ch->live || !ch->character || !body_sync_is_local(ep->sync, stand_world_id(ep->world, c)) || b3Body_GetShapes(ch->body, &shape, 1) != 1 ||
            b3Shape_GetType(shape) != b3_capsuleShape) {
            continue;
        }
        b3Capsule cap = b3Shape_GetCapsule(shape); /* a character never tips: its local axis is the world's */
        const b3Vec3 points[2] = {cap.center1, cap.center2};
        b3ShapeProxy proxy = {.points = points, .count = 2, .radius = cap.radius - PASS_M};
        ps.her = ch->body;
        b3World_OverlapShape(ep->world->world, b3Body_GetPosition(ch->body), &proxy, b3DefaultQueryFilter(), passing_shape, &ps);
    }
    for (uint32_t slot = 0; slot < STAND_BODIES_MAX; slot++) {
        if (ps.inside[slot] && !ep->passed[slot]) {
            stand_metrics_response_miss(&st->metrics, gi);
        }
        ep->passed[slot] = ps.inside[slot];
    }
}

/* Every body's speeds this tick, for a driven body's rest (stand_drive_t). */
static void note_drives(body_sync_stand_t *st) {
    for (uint32_t e = 0; e < st->endpoints; e++) {
        endpoint_t *ep = &st->ep[e];
        if (!ep->built || !ep->active) {
            continue;
        }
        for (uint32_t slot = 0; slot < STAND_BODIES_MAX; slot++) {
            const stand_slot_t *s = &ep->world->slots[slot];
            if (!s->live || s->desc.motion == BODY_SYNC_MOTION_STATIC) {
                continue;
            }
            body_sync_id_t id = stand_world_id(ep->world, slot);
            if (ep->drive_id[slot] != id) {
                memset(&ep->drives[slot], 0, sizeof(ep->drives[slot]));
                ep->drive_id[slot] = id;
            }
            bool awake = b3Body_IsAwake(s->body);
            b3Vec3 w = b3Body_GetAngularVelocity(s->body);
            stand_drive_note(&ep->drives[slot], awake ? stand_world_speed(ep->world, slot) : 0.0f, awake ? sqrtf(w.x * w.x + w.y * w.y + w.z * w.z) : 0.0f);
        }
    }
}

/* What a body moves at for the rest measures: nothing asleep, its velocities, or a driven body's
   over its window (stand_drive_motion): a kinematic copy steps on the copy grid. */
static void rest_motion(const endpoint_t *ep, uint32_t slot, float *speed, float *spin) {
    const stand_slot_t *s = &ep->world->slots[slot];
    if (!b3Body_IsAwake(s->body)) {
        *speed = *spin = 0.0f;
    } else if (b3Body_GetType(s->body) == b3_kinematicBody) {
        stand_drive_motion(&ep->drives[slot], STAND_DT, speed, spin);
    } else {
        b3Vec3 w = b3Body_GetAngularVelocity(s->body);
        *speed = stand_world_speed(ep->world, slot);
        *spin = sqrtf(w.x * w.x + w.y * w.y + w.z * w.z);
    }
}

static float rest_speed(const endpoint_t *ep, uint32_t slot) {
    float speed, spin;
    rest_motion(ep, slot, &speed, &spin);
    return speed;
}

/* The host body's own path over the owner ticks a copy showed in the last REST_TICKS (the pull
   after the seam lets a copy out of the rest hold, and the copy's rest window), as the seam's
   test of a move reads it: a copy-grid step, in place or in turn, left within
   SYNC_SOFT_CREEP_TICKS. */
static bool host_moved_at_play(const endpoint_t *host, const endpoint_t *ep, uint32_t slot, body_sync_id_t id) {
    double played = 0.0;
    const owner_ring_t *r = host->drawn_rings ? &host->drawn_rings[slot] : NULL;
    if (!r || !host->drawn_turns || r->id != id || !body_sync_playback(ep->sync, id, &played)) {
        return false;
    }
    float path[REST_TICKS + SYNC_SOFT_CREEP_TICKS][3], turns[REST_TICKS + SYNC_SOFT_CREEP_TICKS][4];
    uint32_t count = 0;
    int64_t newest = (int64_t)floor(played);
    for (int64_t k = newest - (int64_t)(REST_TICKS + SYNC_SOFT_CREEP_TICKS) + 1; k <= newest; k++) {
        if (ring_at(r, k, path[count])) {
            memcpy(turns[count], host->drawn_turns[(size_t)slot * RING_TICKS + (uint64_t)k % RING_TICKS], sizeof(turns[count]));
            count++;
        } else {
            count = 0; /* a gap in the ring: only a run of consecutive ticks is a path */
        }
    }
    return stand_path_moved((const float (*)[3])path, count, SYNC_SOFT_HOLD_M, SYNC_SOFT_CREEP_TICKS) ||
           stand_turns_moved((const float (*)[4])turns, count, SYNC_SOFT_HOLD_RAD, SYNC_SOFT_CREEP_TICKS);
}

/* An awake host body (a pile against a character, which never sleeps) still enough that its copy
   at `at`, `at_q` has nothing to follow: over the window host_moved_at_play reads, ending at the
   copy's playback, the host body never left half a copy-grid step of `at` nor half the hold's turn
   of `at_q`. A record rounds each axis by at most half a cell, and its rotation by at most
   net_bits_quat_error(9) (0.0097 rad, under half SYNC_SOFT_HOLD_RAD), so every record of that
   window decodes within a step (SYNC_SOFT_HOLD_M, SYNC_SOFT_HOLD_RAD) of the copy's pose, and the
   rest hold (drive_copy) keeps the copy
   where it is: any move of it is the seam's, not the host's. A host body that turns further takes
   its copy one step of the hold, its rounded position with it, as one that moves further does. */
static bool host_held_at_play(const endpoint_t *host, const endpoint_t *ep, uint32_t slot, body_sync_id_t id, const float at[3], const float at_q[4]) {
    double played = 0.0;
    const owner_ring_t *r = host->drawn_rings ? &host->drawn_rings[slot] : NULL;
    if (!r || !host->drawn_turns || r->id != id || !body_sync_playback(ep->sync, id, &played)) {
        return false;
    }
    int64_t newest = (int64_t)ceil(played);
    for (int64_t k = newest - (int64_t)(REST_TICKS + SYNC_SOFT_CREEP_TICKS) + 1; k <= newest; k++) {
        float p[3], turn[3];
        if (!ring_at(r, k, p) || stand_distance3(p, at) > 0.5f * SYNC_SOFT_HOLD_M) {
            return false;
        }
        sync_turn_between(at_q, host->drawn_turns[(size_t)slot * RING_TICKS + (uint64_t)k % RING_TICKS], turn);
        if (sqrtf(turn[0] * turn[0] + turn[1] * turn[1] + turn[2] * turn[2]) > 0.5f * SYNC_SOFT_HOLD_RAD) {
            return false;
        }
    }
    return true;
}

static void sample_layer(body_sync_stand_t *st) {
    endpoint_t *host = &st->ep[0];
    for (uint32_t g = 1; g < st->endpoints; g++) {
        endpoint_t *ep = &st->ep[g];
        if (!ep->built || !ep->active) {
            continue;
        }
        uint32_t gi = g - 1u;
        stand_metrics_pause_window(&st->metrics, gi, ep->pause_window_max);
        for (uint32_t k = 0; k < ep->hit_count;) {
            float drawn[3], at[3], rot[4];
            stand_slot_t *gs = stand_world_slot(ep->world, ep->hits[k].id), *hs = stand_world_slot(host->world, ep->hits[k].id);
            bool done = !gs || !hs || st->now - ep->hits[k].tick >= RESPONSE_TICKS;
            if (!done) {
                pose_of(host, ep->hits[k].slot, at, rot);
                /* A host that only comes to where this guest already drew the copy at the hit (an
                   earlier push of it shown, then taken) moved it nowhere this guest did not show. */
                ep->hits[k].host_moved = ep->hits[k].host_moved ||
                                         (stand_distance3(at, ep->hits[k].host) >= RESPONSE_M && stand_distance3(at, ep->hits[k].drawn) >= RESPONSE_M);
                drawn_pose(ep, ep->hits[k].slot, drawn, rot);
                /* Moved here before the host did: decided once the host moves, or dropped. */
                if (stand_distance3(drawn, ep->hits[k].drawn) >= ANSWER_M && !ep->hits[k].moved && st->now - ep->hits[k].tick <= RESPONSE_WINDOW) {
                    ep->hits[k].moved = true;
                    ep->hits[k].answer = st->now - ep->hits[k].tick;
                }
                if (ep->hits[k].moved && ep->hits[k].host_moved && !ep->hits[k].counted) {
                    stand_metrics_response(&st->metrics, gi, (double)ep->hits[k].answer);
                    ep->hits[k].counted = true;
                }
                /* Kept to the pause's end, where the gap to the host is what the pull starts from. */
                if (st->now - ep->hits[k].tick == RESPONSE_WINDOW && ep->hits[k].host_moved) {
                    stand_metrics_layer_hit(&st->metrics, gi, ep->hits[k].tick, (double)stand_distance3(drawn, at));
                }
                done = ep->hits[k].counted && st->now - ep->hits[k].tick >= RESPONSE_WINDOW;
            } else if (ep->hits[k].host_moved && !ep->hits[k].counted && gs && hs) {
                stand_metrics_response_miss(&st->metrics, gi); /* the host moved it, this guest did not in the window */
            }
            if (done) {
                ep->hits[k] = ep->hits[--ep->hit_count];
            } else {
                k++;
            }
        }
        /* The rest measures watch a world at rest that agrees: every copy and its host body
           resting (asleep, or slower than REST_STILL: a pile against a character never sleeps;
           a driven body over its window, rest_motion),
           within 1 cm and drawn where it is, for REST_TICKS; a copy this guest's body touches rests
           too. Its moves count as jitter while she stands still (a pile trembling against her),
           not while she pushes.
           A move inside it is jitter, a copy starting to move a wake. */
        ep->standing = true;
        for (uint32_t c = 0; c < STAND_BODIES_MAX; c++) {
            stand_slot_t *ch = &ep->world->slots[c];
            if (ch->live && ch->character && body_sync_is_local(ep->sync, stand_world_id(ep->world, c)) && stand_world_speed(ep->world, c) >= 0.25f) { /* the layer's mover speed */
                ep->standing = false;
            }
        }
        /* She walking is no rest: what she sets moving as she leaves a pile is her doing. A rest
           that ends only because host bodies moved (a creeping pile slipping against a character)
           is the host's physics, a host wake; one this guest's copies or bodies end is a break. */
        bool was_rest = ep->world_rest_run >= REST_TICKS && ep->standing, now_rest = ep->standing;
        bool guest_moved = !ep->standing;
        for (uint32_t slot = 0; slot < STAND_BODIES_MAX; slot++) {
            stand_slot_t *s = &ep->world->slots[slot];
            body_sync_id_t id = stand_world_id(ep->world, slot);
            stand_slot_t *hs = s->live ? stand_world_slot(host->world, id) : NULL;
            if (!s->live || !hs || s->desc.motion == BODY_SYNC_MOTION_STATIC || s->character) {
                ep->was_resting[slot] = false;
                ep->rest_lag[slot] = 0;
                continue;
            }
            if (body_sync_is_local(ep->sync, id)) {
                ep->local_tick[slot] = st->now + 1u; /* this guest's own action: no rest of the layer's */
                ep->was_resting[slot] = false;
                ep->rest_lag[slot] = 0;
                /* ...and while it moves (her throw in flight), no rest of her world either: what it
                   hits is her doing, not a break of a rest. */
                bool still = !b3Body_IsAwake(s->body) || stand_world_speed(ep->world, slot) < REST_STILL;
                now_rest = now_rest && still;
                guest_moved = guest_moved || !still;
                continue;
            }
            stand_metrics_copy_speed(&st->metrics, gi, (double)stand_world_speed(ep->world, slot));
            bool asleep = rest_speed(ep, slot) < REST_STILL;
            bool host_asleep = rest_speed(host, slot) < REST_STILL;
            if (b3Body_IsAwake(hs->body) != ep->host_awake[slot]) {
                ep->host_awake[slot] = b3Body_IsAwake(hs->body);
                ep->host_flip[slot] = st->now;
            }
            /* The host body falling asleep sends its exact rest pose, which replaces the rounded one
               a copy of a still, awake owner stood on: one move a rest, once it has landed. */
            bool host_steady = st->now - ep->host_flip[slot] >= LAG_TICKS;
            bool touched = ep->touched_tick[slot] == st->now + 1u || (ep->local_tick[slot] && st->now + 1u - ep->local_tick[slot] < REST_TICKS);
            bool near = ep->touched_tick[slot] + LAG_TICKS > st->now + 1u || (ep->local_tick[slot] && st->now + 1u - ep->local_tick[slot] < LAG_TICKS);
            ep->rest_lag[slot] = host_asleep && !asleep && !near ? ep->rest_lag[slot] + 1u : 0u;
            stand_metrics_rest_lag(&st->metrics, gi, ep->rest_lag[slot]);
            float offset[3], spin[4];
            float offset_now = body_sync_render_offset(ep->sync, id, offset, spin) ? sqrtf(offset[0] * offset[0] + offset[1] * offset[1] + offset[2] * offset[2]) : 0.0f;
            if (body_sync_is_predicted(ep->sync, id) && offset_now > ep->offset_prev[slot] + (float)REST_MOVE_M * 10.0f) {
                stand_metrics_soft_place(&st->metrics, gi, ++ep->soft_places[slot]);
                ep->pause_reset[slot] = true;
            }
            ep->offset_prev[slot] = offset_now;
            float drawn[3], rot[4], host_at[3], here[3], here_q[4];
            drawn_pose(ep, slot, drawn, rot);
            pose_of(host, slot, host_at, rot);
            pose_of(ep, slot, here, here_q);
            /* A render offset still decaying is a correction's tail, not a rest. */
            bool shown = stand_distance3(drawn, here) <= (float)REST_MOVE_M;
            bool host_rests = host_asleep && host_steady, copy_rests = stand_copy_rests(asleep, shown, stand_distance3(drawn, host_at), RESPONSE_M);
            now_rest = now_rest && host_rests && copy_rests;
            guest_moved = guest_moved || stand_copy_breaks(host_rests, copy_rests, !copy_rests && host_moved_at_play(host, ep, slot, id));
            if (was_rest && host_asleep && b3Body_IsAwake(hs->body)) {
                stand_metrics_creep(&st->metrics, gi, (double)rest_speed(host, slot));
            }
            /* A body asleep on the host has an exact pose to hold, and so has one kept awake (against
               a character) that stays within half a step of its copy, in place and in turn
               (host_held_at_play); one that creeps further is followed on the copy grid, its steps
               the host's own creep. */
            if (was_rest && (!touched || ep->standing) && (!b3Body_IsAwake(hs->body) || host_held_at_play(host, ep, slot, id, ep->rest_here[slot], ep->rest_here_q[slot]))) {
                double move = (double)stand_distance3(drawn, ep->layer_prev[slot]);
                move = move > REST_MOVE_M ? move : 0.0; /* float noise of a pose read back */
                stand_metrics_rest(&st->metrics, gi, move, ep->was_resting[slot] && !asleep && host_asleep);
            }
            ep->was_resting[slot] = asleep;
            memcpy(ep->layer_prev[slot], drawn, sizeof(drawn));
            memcpy(ep->rest_here[slot], here, sizeof(here));
            memcpy(ep->rest_here_q[slot], here_q, sizeof(here_q));
        }
        if (st->items_done && ep->world_rest_run >= REST_TICKS && !now_rest) {
            stand_metrics_rest_break(&st->metrics, gi, !guest_moved);
        }
        ep->world_rest_run = now_rest ? ep->world_rest_run + 1u : 0u;
        passed_through(st, ep, gi);
    }
}

/* A predicted copy's visible correction this tick: the seam's move at the apply (a placement)
   plus the change of its render offset, which together are the drawn move its own step does not
   explain. True when the copy is predicted (and so not measured against its owner's playback). */
static bool sample_predicted(body_sync_stand_t *st, endpoint_t *ep, uint32_t e, uint32_t slot, body_sync_id_t id) {
    copy_track_t *t = &ep->tracks[slot];
    stand_slot_t *s = &ep->world->slots[slot];
    body_sync_authority_t a;
    bool predicted = s->live && s->desc.motion != BODY_SYNC_MOTION_STATIC && !body_sync_is_local(ep->sync, id) && body_sync_is_predicted(ep->sync, id) &&
                     body_sync_authority(ep->sync, id, &a) && a.seat < st->endpoints && (e == 0) != (a.seat == 0);
    if (!predicted) {
        t->self_valid = false;
        return false;
    }
    float here[3], rot[4], offset[3], turn[4];
    pose_of(ep, slot, here, rot);
    body_sync_render_offset(ep->sync, id, offset, turn);
    if (t->self_valid && t->id == id && ep->ticks_run == t->ticks_at + 1u) {
        float c[3];
        for (int i = 0; i < 3; i++) {
            c[i] = t->applied[i] - t->stepped[i] + offset[i] - t->offset_at[i];
        }
        stand_metrics_correction(&st->metrics, e == 0 ? (uint32_t)a.seat - 1u : e - 1u, (double)sqrtf(c[0] * c[0] + c[1] * c[1] + c[2] * c[2]));
    }
    t->id = id;
    t->valid = false;
    t->self_valid = true;
    t->ticks_at = ep->ticks_run;
    memcpy(t->stepped, here, sizeof(here));
    memcpy(t->offset_at, offset, sizeof(offset));
    return true;
}

/* Whether a drawn ring holds a moving body at owner tick `played`: it moved by more than a pose's
   float noise over the tick before. At rest the pictures agree by the rest records; counting them
   would only dilute the quantiles. */
static bool moving_at(const owner_ring_t *r, double played) {
    float a[3], b[3];
    return owner_at(r, played, a) && owner_at(r, played - 1.0, b) && (double)stand_distance3(a, b) > REST_MOVE_M;
}

static void note_lost_takes(body_sync_stand_t *st) {
    for (uint32_t g = 1; g < st->endpoints; g++) {
        endpoint_t *ep = &st->ep[g];
        if (!ep->built || !ep->active) {
            continue;
        }
        for (uint32_t slot = 0; slot < STAND_BODIES_MAX; slot++) {
            uint32_t count = 0;
            float hand_back = 0.0f;
            body_sync_id_t id = ep->world->slots[slot].live ? stand_world_id(ep->world, slot) : BODY_SYNC_ID_NONE;
            if (id) {
                (void)body_sync_lost_take(ep->sync, id, &count, &hand_back);
            }
            for (uint32_t k = ep->lost_seen[slot]; k < count; k++) {
                stand_metrics_lost_take(&st->metrics, g - 1u, (double)hand_back);
            }
            ep->lost_seen[slot] = count;
        }
    }
}

/* The lag-aligned gap: a copy E plays (on a guest the host's bodies, on the host a guest's),
   predicted or not, against what its owner O drew at the owner tick E shows; its physics stays
   within 2 m of that pose and a tick of the owner's travel (README, the lag-aligned gap) and its
   render offset adds at most 2 m. Bodies relayed between
   guests play on a clock the stand cannot map back and are left out. A predicted copy in its pause
   is this guest's own answer to its touch, drawn ahead of the tick it plays, so it leaves the gap
   and is reported only. The prediction gap: a pause that began with its owner at rest, against
   the host's picture when the host shows that guest's moment (its playback of the guest's
   character reaches the guest's tick); both simulate one contact from one rest pose, so past it
   they differ by chaos. A pause begun on a moving owner shares no such moment: the flight-pause
   gap measures it at the playback tick, its lead included. */
/* Host: every body a guest's character carries, against that character's hold point as the host
   shows both. The guest sets the body there each tick before its step, so they part by one
   tick of the walk and the copy grid. */
static void sample_carry(body_sync_stand_t *st) {
    endpoint_t *host = &st->ep[0];
    const body_sync_character_tuning_t *t = &st->scene.character;
    for (uint32_t g = 1; g < st->endpoints; g++) {
        uint32_t c = character_slot(st, g);
        body_sync_id_t character = stand_world_id(host->world, c);
        if (!st->ep[g].active || !character || !host->world->slots[c].live) {
            continue;
        }
        float at[3], rot[4];
        pose_of(host, c, at, rot);
        /* The facing yaw's forward, (0, 0, 1) turned by the character's rotation. */
        const float forward[3] = {2.0f * (rot[0] * rot[2] + rot[3] * rot[1]), 0.0f, 1.0f - 2.0f * (rot[0] * rot[0] + rot[1] * rot[1])};
        const float hold[3] = {at[0] + forward[0] * t->hold_forward, at[1] + t->hold_up, at[2] + forward[2] * t->hold_forward};
        for (uint32_t slot = 0; slot < STAND_BODIES_MAX; slot++) {
            body_sync_id_t id = stand_world_id(host->world, slot);
            body_sync_authority_t a;
            if (slot == c || !host->world->slots[slot].live || !id || !body_sync_authority(host->sync, id, &a) || a.holder != character) {
                continue;
            }
            float p[3], q[4];
            pose_of(host, slot, p, q);
            stand_metrics_carry(&st->metrics, g - 1u, (double)stand_distance3(p, hold));
        }
    }
}

static void sample_aligned(body_sync_stand_t *st) {
    endpoint_t *host = &st->ep[0];
    for (uint32_t g = 1; g < st->endpoints; g++) {
        endpoint_t *guest = &st->ep[g];
        if (!guest->built || !guest->active) {
            continue;
        }
        double shown = 0.0;
        body_sync_id_t character = stand_world_id(host->world, character_slot(st, g));
        bool guest_time = character && body_sync_playback(host->sync, character, &shown);
        for (uint32_t slot = 0; slot < STAND_BODIES_MAX; slot++) {
            for (int side = 0; side < 2; side++) {
                endpoint_t *e = side ? guest : host, *o = side ? host : guest;
                stand_slot_t *s = &e->world->slots[slot];
                body_sync_id_t id = stand_world_id(e->world, slot);
                body_sync_authority_t a;
                double played = 0.0;
                if (!s->live || s->desc.motion == BODY_SYNC_MOTION_STATIC || body_sync_is_local(e->sync, id) || !body_sync_authority(e->sync, id, &a) ||
                    a.seat != o->index || !body_sync_playback(e->sync, id, &played)) {
                    continue;
                }
                const owner_ring_t *r = &o->drawn_rings[slot];
                float drawn[3], rot[4], truth[3];
                if (r->id != id || !owner_at(r, played, truth) || !moving_at(r, played)) {
                    continue;
                }
                /* A lost take's glide is the loser's picture handed back to the winner's, measured
                   at the hand-back (lost_takes); the copy itself is what is compared meanwhile. */
                uint32_t lost_count;
                float hand_back;
                if (body_sync_lost_take(e->sync, id, &lost_count, &hand_back)) {
                    pose_of(e, slot, drawn, rot);
                } else {
                    drawn_pose(e, slot, drawn, rot);
                }
                if (body_sync_is_predicted(e->sync, id) && body_sync_is_pull_paused(e->sync, id)) {
                    drawn[0] += st->plant_lead;
                    uint8_t pause = e->drawn_paused ? e->drawn_paused[(size_t)slot * RING_TICKS + e->ring_newest % RING_TICKS] : PAUSE_IN_FLIGHT;
                    if (pause != PAUSE_FROM_REST) {
                        stand_metrics_flight_pause(&st->metrics, g - 1u, (double)stand_distance3(drawn, truth));
                    }
                    continue;
                }
                stand_metrics_aligned(&st->metrics, g - 1u, (double)stand_distance3(drawn, truth), body_sync_is_predicted(e->sync, id));
            }
            /* A guest's prediction of the host's body in its pause, at the moment the host shows. */
            const owner_ring_t *r = &guest->drawn_rings[slot];
            stand_slot_t *hs = &host->world->slots[slot];
            body_sync_id_t id = stand_world_id(host->world, slot);
            int64_t k = (int64_t)floor(shown + 0.5);
            if (!guest_time || !hs->live || hs->desc.motion == BODY_SYNC_MOTION_STATIC || !body_sync_is_local(host->sync, id) || r->id != id || k < 0 ||
                guest->drawn_paused[(size_t)slot * RING_TICKS + (uint64_t)k % RING_TICKS] != PAUSE_FROM_REST) {
                continue;
            }
            float seen[3], here[3], rot[4];
            const owner_ring_t *hr = &host->drawn_rings[slot];
            if (!owner_at(r, shown, seen) || hr->id != id || !ring_at(hr, (int64_t)host->ring_newest, here) ||
                !(moving_at(r, shown) || moving_at(hr, (double)host->ring_newest))) {
                continue;
            }
            (void)rot;
            stand_metrics_prediction(&st->metrics, g - 1u, (double)stand_distance3(here, seen));
        }
    }
}

static void plant_jolt(body_sync_stand_t *st);

/* A body a guest simulates: the seam's move of it at the apply or at a claim (a take-over's
   placement), measured once the tick's actions are done and before the game moves what it holds,
   plus what
   its render offset did beyond the seam's own decay, replayed on last tick's offset. Measured from
   the tick it turns local, against its pose and offset of the tick before (a copy's then): a
   take-over that keeps the drawn pose reads 0. An offset left from its time as a copy only
   decays, the copy's correction ending; once the seam sets one while the guest simulates the
   body, every change of it counts, the decay too (what the guest sees glide). */
static void sample_local(body_sync_stand_t *st) {
    for (uint32_t e = 1; e < st->endpoints; e++) {
        endpoint_t *ep = &st->ep[e];
        if (!ep->built || !ep->active) {
            continue;
        }
        for (uint32_t slot = 0; slot < STAND_BODIES_MAX; slot++) {
            copy_track_t *t = &ep->tracks[slot];
            stand_slot_t *s = &ep->world->slots[slot];
            body_sync_id_t id = stand_world_id(ep->world, slot);
            /* after_apply skips characters: the seam never moves one it simulates. */
            if (!s->live || s->character || s->desc.motion == BODY_SYNC_MOTION_STATIC) {
                t->local_valid = false;
                continue;
            }
            bool local = body_sync_is_local(ep->sync, id);
            float here[3], rot[4], offset[3] = {0.0f, 0.0f, 0.0f}, turn[4] = {0.0f, 0.0f, 0.0f, 1.0f};
            pose_of(ep, slot, here, rot);
            if (!body_sync_render_offset(ep->sync, id, offset, turn)) {
                offset[0] = offset[1] = offset[2] = 0.0f;
                turn[0] = turn[1] = turn[2] = 0.0f;
                turn[3] = 1.0f;
            }
            if (e == 1u && st->plant_offset_slot == slot && st->plant_offset_id == id) {
                const float about_y[4] = {0.0f, sinf(0.5f * st->plant_offset_rad), 0.0f, cosf(0.5f * st->plant_offset_rad)};
                offset[0] += st->plant_offset_m;
                stand_quat_mul(about_y, turn, turn);
            }
            bool continues = t->local_valid && t->local_id == id && ep->ticks_run == t->local_ticks_at + 1u && !t->applied_copy;
            t->local_own_offset = continues && t->local_now && t->local_own_offset;
            if (local && continues) {
                sync_offset_t law = {.position = {t->local_offset[0], t->local_offset[1], t->local_offset[2]},
                                     .rotation = {t->local_offset_rot[0], t->local_offset_rot[1], t->local_offset_rot[2], t->local_offset_rot[3]}};
                sync_offset_decay(&law);
                float r[3], c[3];
                for (int i = 0; i < 3; i++) {
                    r[i] = offset[i] - law.position[i];
                    t->local_own_offset = t->local_own_offset || fabsf(r[i]) > 1e-5f;
                }
                for (int i = 0; i < 4; i++) {
                    t->local_own_offset = t->local_own_offset || fabsf(turn[i] - law.rotation[i]) > 1e-5f;
                }
                for (int i = 0; i < 3; i++) {
                    c[i] = t->acted[i] - t->local_stepped[i] + (t->local_own_offset ? offset[i] - t->local_offset[i] : r[i]);
                }
                /* The turn likewise: the offset's turn beyond the law (or every change of one the seam
                   set), after the seam's turn at the apply; drawn = offset turn * physics turn. */
                const float *expected = t->local_own_offset ? t->local_offset_rot : law.rotation;
                float back[4] = {-expected[0], -expected[1], -expected[2], expected[3]}, unturn[4] = {-t->local_stepped_rot[0], -t->local_stepped_rot[1], -t->local_stepped_rot[2], t->local_stepped_rot[3]};
                float by_offset[4], by_seam[4], both[4];
                const float none[4] = {0.0f, 0.0f, 0.0f, 1.0f};
                stand_quat_mul(turn, back, by_offset);
                stand_quat_mul(t->acted_rot, unturn, by_seam);
                stand_quat_mul(by_offset, by_seam, both);
                stand_metrics_local_correction(&st->metrics, e - 1u, (double)sqrtf(c[0] * c[0] + c[1] * c[1] + c[2] * c[2]), (double)stand_quat_angle_deg(both, none));
            }
            t->local_valid = true;
            t->local_now = local;
            t->local_id = id;
            t->local_ticks_at = ep->ticks_run;
            memcpy(t->local_stepped, here, sizeof(here));
            memcpy(t->local_offset, offset, sizeof(offset));
            memcpy(t->local_stepped_rot, rot, sizeof(rot));
            memcpy(t->local_offset_rot, turn, sizeof(turn));
        }
    }
}

/* Every copy that its owner sends to directly, against that owner's own path. */
static void sample_playback(body_sync_stand_t *st) {
    for (uint32_t e = 0; e < st->endpoints; e++) {
        endpoint_t *ep = &st->ep[e];
        if (!ep->built || !ep->active) {
            continue;
        }
        for (uint32_t slot = 0; slot < STAND_BODIES_MAX; slot++) {
            copy_track_t *t = &ep->tracks[slot];
            stand_slot_t *s = &ep->world->slots[slot];
            body_sync_id_t id = stand_world_id(ep->world, slot);
            body_sync_authority_t a;
            double played = 0.0;
            /* A predicted copy (the second layer) is not its playback: its own step explains its
               move, and what the seam did to it besides (a placement, an offset's release) is
               its correction. */
            if (sample_predicted(st, ep, e, slot, id)) {
                continue;
            }
            bool counts = s->live && s->desc.motion != BODY_SYNC_MOTION_STATIC && !body_sync_is_local(ep->sync, id) && !body_sync_is_predicted(ep->sync, id) &&
                          body_sync_authority(ep->sync, id, &a) && a.seat < st->endpoints && (e == 0) != (a.seat == 0) &&
                          st->ep[a.seat].built && body_sync_playback(ep->sync, id, &played);
            float drawn[3], rot[4], truth[3];
            const owner_ring_t *ring = counts ? &st->ep[a.seat].rings[slot] : NULL;
            if (!counts || ring->id != id || !owner_at(ring, played, truth)) {
                t->valid = false;
                continue;
            }
            drawn_pose(ep, slot, drawn, rot);
            float offset[3], turn[4];
            body_sync_render_offset(ep->sync, id, offset, turn);
            bool offset_now = offset[0] * offset[0] + offset[1] * offset[1] + offset[2] * offset[2] > REST_OFFSET_M * REST_OFFSET_M;
            uint32_t gi = e == 0 ? (uint32_t)a.seat - 1u : e - 1u;
            bool moved = t->valid && t->id == id && t->owner == a.seat && played >= t->played;
            const endpoint_t *owner = &st->ep[a.seat];
            double behind = owner->ring_any ? (double)owner->ring_newest - played : 0.0;
            stand_metrics_playback(&st->metrics, gi, (double)stand_distance3(drawn, truth), path_error(ring, played, drawn), behind, moved,
                                   moved ? (double)stand_distance3(drawn, t->drawn) : 0.0, moved ? owner_move(ring, t->played, played, t->owner_at, truth) : 0.0,
                                   moved && (offset_now || t->offset));
            *t = (copy_track_t){.id = id, .valid = true, .owner = a.seat, .offset = offset_now, .played = played};
            memcpy(t->drawn, drawn, sizeof(drawn));
            memcpy(t->owner_at, truth, sizeof(truth));
        }
    }
}

static void sample(body_sync_stand_t *st) {
    stand_metrics_begin_tick(&st->metrics);
    note_drives(st);
    endpoint_t *host = &st->ep[0];
    if (!st->started) {
        st->started = true;
        for (uint32_t slot = 0; slot < STAND_BODIES_MAX; slot++) {
            float rot[4];
            if (host->world->slots[slot].live) {
                pose_of(host, slot, st->start_position[slot], rot);
            }
            st->start_id[slot] = stand_world_id(host->world, slot);
        }
    }
    /* The host's own motion this tick, so a frozen copy can be told from a body at rest. */
    for (uint32_t slot = 0; slot < STAND_BODIES_MAX; slot++) {
        stand_slot_t *s = &host->world->slots[slot];
        if (!s->live || s->desc.motion == BODY_SYNC_MOTION_STATIC) {
            continue;
        }
        body_sync_id_t id = stand_world_id(host->world, slot);
        float p[3], r[4];
        pose_of(host, slot, p, r);
        if (host->hist_id[slot] == id && host->hist_n[slot] > 0) {
            for (int i = 0; i < 3; i++) {
                host->prev_delta[slot][i] = p[i] - host->prev_pos[slot][i];
            }
        } else {
            memset(host->prev_delta[slot], 0, sizeof(host->prev_delta[slot]));
            host->hist_id[slot] = id;
            host->hist_n[slot] = 1;
        }
        memcpy(host->prev_pos[slot], p, sizeof(p));
    }
    for (uint32_t g = 1; g < st->endpoints; g++) {
        endpoint_t *ep = &st->ep[g];
        if (!ep->built || !ep->active) {
            continue;
        }
        uint32_t gi = g - 1;
        for (uint32_t slot = 0; slot < STAND_BODIES_MAX; slot++) {
            stand_slot_t *hs = &host->world->slots[slot];
            if (!hs->live || hs->desc.motion == BODY_SYNC_MOTION_STATIC) {
                continue;
            }
            body_sync_id_t id = stand_world_id(host->world, slot);
            if (!stand_world_slot(ep->world, id)) {
                continue;
            }
            float hp[3], hr[4], gp[3], gr[4];
            pose_of(host, slot, hp, hr);
            drawn_pose(ep, slot, gp, gr);
            stand_metrics_divergence(&st->metrics, gi, (double)stand_distance3(hp, gp), (double)stand_quat_angle_deg(hr, gr));
            bool copy = !body_sync_is_local(ep->sync, id);
            if (ep->hist_id[slot] != id) {
                ep->hist_id[slot] = id;
                ep->hist_n[slot] = 0;
                ep->frozen[slot] = 0;
            }
            float d[3] = {gp[0] - ep->prev_pos[slot][0], gp[1] - ep->prev_pos[slot][1], gp[2] - ep->prev_pos[slot][2]};
            if (ep->hist_n[slot] >= 1) {
                const float zero[3] = {0.0f, 0.0f, 0.0f};
                float moved_now = delta_length(d, zero), moved_before = delta_length(ep->prev_delta[slot], zero);
                if (copy && ep->hist_n[slot] >= 2 && (moved_now > 1e-6f || moved_before > 1e-6f)) {
                    stand_metrics_jerk(&st->metrics, gi, (double)delta_length(d, ep->prev_delta[slot]));
                }
                bool host_moved = delta_length(host->prev_delta[slot], zero) > 1e-3f;
                ep->frozen[slot] = copy && moved_now < 1e-5f && host_moved ? ep->frozen[slot] + 1 : 0;
            }
            memcpy(ep->prev_delta[slot], d, sizeof(d));
            memcpy(ep->prev_pos[slot], gp, sizeof(gp));
            ep->hist_n[slot] = ep->hist_n[slot] >= 2 ? 2 : (uint8_t)(ep->hist_n[slot] + 1);
        }
    }
    sample_layer(st);
    sample_playback(st);
    note_lost_takes(st);
    sample_aligned(st);
    sample_carry(st);
    sample_local(st);
    stand_metrics_end_tick(&st->metrics);

    for (uint32_t g = 1; g < st->endpoints; g++) {
        endpoint_t *ep = &st->ep[g];
        uint32_t gi = g - 1;
        body_sync_traffic_t down, up;
        if (body_sync_traffic(host->sync, (uint8_t)g, &down)) {
            for (uint32_t k = 0; k < BODY_SYNC_TRAFFIC_COUNT; k++) {
                stand_metrics_bytes(&st->metrics, gi, STAND_DIR_TO_GUEST, k, down.sent_bytes[k] - st->seam_prev[gi][0][k], st->now);
                st->seam_prev[gi][0][k] = down.sent_bytes[k];
            }
        }
        if (ep->built && body_sync_traffic(ep->sync, BODY_SYNC_SEAT_HOST, &up)) {
            for (uint32_t k = 0; k < BODY_SYNC_TRAFFIC_COUNT; k++) {
                stand_metrics_bytes(&st->metrics, gi, STAND_DIR_TO_HOST, k, up.sent_bytes[k] - st->seam_prev[gi][1][k], st->now);
                st->seam_prev[gi][1][k] = up.sent_bytes[k];
            }
        }
        net_link_stats_t stats;
        if (coop_session_seated(host->session, (uint8_t)g) && net_link_stats(host->link, coop_session_peer(host->session, (uint8_t)g), &stats)) {
            stand_metrics_link(&st->metrics, gi, STAND_DIR_TO_GUEST, (double)stats.out_bytes_per_s);
        }
        if (ep->active && net_link_stats(ep->link, coop_session_peer(ep->session, BODY_SYNC_SEAT_HOST), &stats)) {
            stand_metrics_link(&st->metrics, gi, STAND_DIR_TO_HOST, (double)stats.out_bytes_per_s);
        }
    }

    for (uint32_t e = 0; e < st->endpoints; e++) {
        endpoint_t *ep = &st->ep[e];
        if (!ep->built) {
            continue;
        }
        for (uint32_t slot = 0; slot < STAND_BODIES_MAX; slot++) {
            stand_slot_t *s = &ep->world->slots[slot];
            /* The rules never knock a character, and a character's flag bits are its owner's
               game state, so they say nothing about a knockdown. */
            if (!s->live || s->character) {
                continue;
            }
            body_sync_id_t id = stand_world_id(ep->world, slot);
            /* A knockdown shown is a rising edge of the knocked bit as this endpoint reads it, so
               one shown twice counts twice; the host counts the knockdowns it applied. */
            bool knocked = e == 0 ? s->knocked : (body_sync_flags(ep->sync, id) & KNOCK_FLAG) != 0;
            if (ep->knock_id[slot] != id) {
                ep->knock_id[slot] = id;
                ep->knock_count[slot] = 0;
                ep->knock_seen[slot] = 0;
            }
            if (knocked && !ep->knock_seen[slot]) {
                ep->knock_count[slot]++;
            }
            ep->knock_seen[slot] = knocked ? 1 : 0;
            body_sync_authority_t a;
            bool pending = e != 0 && body_sync_authority(ep->sync, id, &a) && body_sync_is_local(ep->sync, id) && a.seat != e;
            ep->pending_since[slot] = pending ? (ep->pending_since[slot] ? ep->pending_since[slot] : st->now + 1) : 0;
        }
    }
}

static bool world_at_rest(const endpoint_t *ep) {
    for (uint32_t slot = 0; slot < STAND_BODIES_MAX; slot++) {
        const stand_slot_t *s = &ep->world->slots[slot];
        if (!s->live || s->desc.motion == BODY_SYNC_MOTION_STATIC) {
            continue;
        }
        float speed, spin;
        rest_motion(ep, slot, &speed, &spin);
        if (speed >= REST_SPEED || spin >= 0.5f) {
            return false;
        }
    }
    return true;
}

/* A copy whose render offset still slides or turns it is still moving on screen. */
static bool drawn_at_rest(const endpoint_t *ep) {
    for (uint32_t slot = 0; slot < STAND_BODIES_MAX; slot++) {
        const stand_slot_t *s = &ep->world->slots[slot];
        float offset[3], turn[4];
        if (!s->live || s->desc.motion == BODY_SYNC_MOTION_STATIC || !body_sync_render_offset(ep->sync, stand_world_id(ep->world, slot), offset, turn)) {
            continue;
        }
        float w = fabsf(turn[3]) < 1.0f ? fabsf(turn[3]) : 1.0f;
        if (offset[0] * offset[0] + offset[1] * offset[1] + offset[2] * offset[2] > REST_OFFSET_M * REST_OFFSET_M || 2.0f * acosf(w) > REST_OFFSET_RAD) {
            return false;
        }
    }
    return true;
}

/* Every player idle and every endpoint's world still, drawn poses included: a guest still
   simulating its own throw is not rest, though the host's copy of it may stand frozen. */
static bool all_at_rest(const body_sync_stand_t *st) {
    for (uint32_t p = 0; p < BODY_SYNC_PLAYERS_MAX; p++) {
        if (stand_schedule_active(&st->schedule, (uint8_t)p) >= 0) {
            return false;
        }
    }
    for (uint32_t e = 0; e < st->endpoints; e++) {
        if (st->ep[e].built && st->ep[e].active && (!world_at_rest(&st->ep[e]) || !drawn_at_rest(&st->ep[e]))) {
            return false;
        }
    }
    return true;
}

/* Ticks an endpoint has run once the stand has run `stand_ticks`: its own rate, on virtual time. */
static uint32_t due(const endpoint_t *ep, uint32_t stand_ticks) {
    return ep->rate_mhz == 0 ? stand_ticks : (uint32_t)(((uint64_t)stand_ticks * ep->rate_mhz) / (BODY_SYNC_TICK_HZ * 1000ull));
}

static bool items_over(const body_sync_stand_t *st) {
    if (!stand_schedule_done(&st->schedule)) {
        return false;
    }
    for (uint32_t e = 0; e < st->endpoints; e++) {
        const endpoint_t *ep = &st->ep[e];
        if (st->now < ep->stall_until || st->now < ep->deaf_until || st->cut_until[e] != 0 || (ep->active && ep->ticks_run < due(ep, st->now))) {
            return false;
        }
    }
    for (uint32_t i = 0; i < st->scenario.action_count; i++) {
        if (st->actions[i].awaiting) {
            return false;
        }
    }
    return true;
}

/* Each `expect pace` walk's progress this tick, on the world that simulates its character: its
   start distance to the point less the nearest it has come, from the tick it started through the
   tick it ended. */
static int32_t label_index(const body_sync_stand_t *st, const char *label);

static void record_pace(body_sync_stand_t *st) {
    for (uint32_t i = 0; i < st->scenario.expect_count; i++) {
        const body_sync_expect_t *x = &st->scenario.expects[i];
        int32_t index = x->kind == BODY_SYNC_EXPECT_PACE ? label_index(st, x->labels[0]) : -1;
        if (index < 0 || !st->pace[i] || st->pace_closed[i] || !st->actions[index].started) {
            continue;
        }
        const action_track_t *t = &st->actions[index];
        const body_sync_action_t *a = &st->scenario.actions[index];
        endpoint_t *ep = st->local ? &st->ep[0] : &st->ep[a->player];
        stand_character_t *c = ep->built ? player_character(st, ep, a->player) : NULL;
        uint32_t k = st->now - t->start_tick;
        if (!c || !c->present || k >= BODY_SYNC_STAND_TICKS_MAX) {
            st->pace_closed[i] = true;
            continue;
        }
        float at[3];
        stand_character_position(c, ep->world, at);
        float dx = a->target.point[0] - at[0], dz = a->target.point[2] - at[2], d = sqrtf(dx * dx + dz * dz);
        if (st->pace_len[i] == 0) {
            st->pace_start[i] = st->pace_best[i] = d;
        }
        st->pace_best[i] = d < st->pace_best[i] ? d : st->pace_best[i];
        for (uint32_t j = st->pace_len[i]; j <= k; j++) {
            st->pace[i][j] = st->pace_start[i] - st->pace_best[i];
        }
        st->pace_len[i] = k + 1u;
        st->pace_closed[i] = t->ended;
    }
}

bool body_sync_stand_tick(body_sync_stand_t *st) {
    if (!st || st->ended) {
        return false;
    }
    uint32_t t = st->now;
    const body_sync_net_event_t *e;
    while ((e = stand_schedule_net(&st->schedule, t)) != NULL) {
        fire_net(st, e);
    }
    for (uint32_t g = 1; g < st->endpoints; g++) {
        if (st->cut_until[g] != 0 && t >= st->cut_until[g]) {
            net_link_sim_set_cut(st->sim, st->ep[g].link, false);
            st->cut_until[g] = 0;
        }
    }
    net_link_sim_advance(st->sim, 1.0 / (double)BODY_SYNC_TICK_HZ);
    for (uint32_t i = 0; i < st->endpoints; i++) {
        endpoint_t *ep = &st->ep[i];
        if (!ep->active || t < ep->stall_until) {
            continue;
        }
        for (uint32_t n = 0; n < CATCH_UP_MAX && ep->active && ep->ticks_run < due(ep, t + 1u); n++) {
            endpoint_tick(st, ep);
        }
    }
    for (uint32_t i = 0; i < st->endpoints; i++) {
        rebuild_view(&st->ep[i]);
    }
    if (!st->local) {
        sample(st);
        plant_jolt(st);
        track_actions(st);
        watch_late(st);
    }
    record_pace(st);
    st->rest_run = all_at_rest(st) ? st->rest_run + 1 : 0;
    st->now++;
    if (!st->items_done && items_over(st)) {
        st->items_done = true;
        st->done_tick = st->now;
        /* Rest counts only once nothing more is scheduled: a copy frozen by a cut looks still. */
        st->rest_run = 0;
    }
    if (st->scenario.has_end) {
        st->ended = st->now >= st->scenario.end_tick;
    } else if (st->items_done) {
        if (st->rest_run >= REST_TICKS) {
            st->ended = true;
            st->rest_tick = t;
        } else if (st->now - st->done_tick >= st->scenario.settle_ticks) {
            st->ended = true;
        }
    }
    if (st->now >= BODY_SYNC_STAND_TICKS_MAX) {
        st->ended = true;
    }
    return !st->ended;
}

/* ---- checks ---- */

static void check_detail(body_sync_check_metrics_t *c, const char *text) {
    if (!c->detail[0]) {
        snprintf(c->detail, sizeof(c->detail), "%s", text);
    }
}

static const char *body_label(const body_sync_stand_t *st, uint32_t slot, char *buffer, size_t size) {
    if (slot < st->scene.body_count && st->scene.bodies[slot].name[0]) {
        return st->scene.bodies[slot].name;
    }
    if (slot >= st->scene.body_count && slot < st->scene.body_count + st->scene.player_count) {
        snprintf(buffer, size, "p%u", slot - st->scene.body_count);
    } else {
        snprintf(buffer, size, "slot %u", slot);
    }
    return buffer;
}

static bool expects_left(const body_sync_stand_t *st, uint32_t g) {
    for (uint32_t i = 0; i < st->scenario.expect_count; i++) {
        if (st->scenario.expects[i].kind == BODY_SYNC_EXPECT_LEFT && st->scenario.expects[i].endpoint == g) {
            return true;
        }
    }
    return false;
}

/* A guest that left is out of the checks that compare it: by the scenario's word (expect left),
   or as an offender, since a check that skips it silently passes on nothing. */
static bool guest_out(const body_sync_stand_t *st, uint32_t g, body_sync_check_metrics_t *c, uint32_t *offenders) {
    if (g == 0 || !st->left[g]) {
        return false;
    }
    if (!expects_left(st, g)) {
        char text[BODY_SYNC_ERROR_TEXT_MAX];
        snprintf(text, sizeof(text), "g%u left at tick %u", g, st->left_tick[g]);
        check_detail(c, text);
        (*offenders)++;
    }
    return true;
}

static bool moving_slot(const stand_world_t *w, uint32_t slot) {
    return w->slots[slot].live && w->slots[slot].desc.motion != BODY_SYNC_MOTION_STATIC;
}

static void check_agree(body_sync_stand_t *st, const body_sync_expect_t *x, body_sync_check_metrics_t *c) {
    c->limit = (double)x->tolerance_m;
    bool rest = st->rest_run >= REST_TICKS;
    double worst = 0.0;
    uint32_t offenders = 0;
    char text[BODY_SYNC_ERROR_TEXT_MAX], name[32];
    if (!rest) {
        check_detail(c, "the run ended before every world came to rest");
    }
    endpoint_t *host = &st->ep[0];
    for (uint32_t g = 1; g < st->endpoints; g++) {
        endpoint_t *ep = &st->ep[g];
        if (!ep->built || guest_out(st, g, c, &offenders)) {
            continue;
        }
        for (uint32_t slot = 0; slot < STAND_BODIES_MAX; slot++) {
            bool on_host = moving_slot(host->world, slot), on_guest = moving_slot(ep->world, slot);
            body_sync_id_t hid = stand_world_id(host->world, slot), gid = stand_world_id(ep->world, slot);
            if (!on_host && !on_guest) {
                continue;
            }
            /* A body one world has and the other lacks is the plainest disagreement of all. */
            if (!on_host || !on_guest || hid != gid) {
                offenders++;
                snprintf(text, sizeof(text), on_host ? "g%u lacks %s" : "only g%u has %s", g, body_label(st, slot, name, sizeof(name)));
                check_detail(c, text);
                continue;
            }
            float hp[3], hr[4], gp[3], gr[4];
            pose_of(host, slot, hp, hr);
            drawn_pose(ep, slot, gp, gr);
            double d = (double)stand_distance3(hp, gp), r = (double)stand_quat_angle_deg(hr, gr);
            if (d > (double)x->tolerance_m || (x->tolerance_deg >= 0.0f && r > (double)x->tolerance_deg)) {
                offenders++;
                if (d > (double)x->tolerance_m) {
                    snprintf(text, sizeof(text), "g%u %s off by %.3g m", g, body_label(st, slot, name, sizeof(name)), d);
                } else {
                    snprintf(text, sizeof(text), "g%u %s turned %.3g deg", g, body_label(st, slot, name, sizeof(name)), r);
                }
                check_detail(c, text);
            }
            worst = d > worst ? d : worst;
        }
    }
    c->value = worst;
    c->passed = rest && offenders == 0;
}

static void check_stuck(body_sync_stand_t *st, body_sync_check_metrics_t *c) {
    uint32_t offenders = 0;
    char text[BODY_SYNC_ERROR_TEXT_MAX], name[32];
    for (uint32_t e = 0; e < st->endpoints; e++) {
        endpoint_t *ep = &st->ep[e];
        if (!ep->built || guest_out(st, e, c, &offenders)) {
            continue;
        }
        for (uint32_t slot = 0; slot < STAND_BODIES_MAX; slot++) {
            stand_slot_t *s = &ep->world->slots[slot];
            if (!s->live || s->desc.motion == BODY_SYNC_MOTION_STATIC) {
                continue;
            }
            body_sync_id_t id = stand_world_id(ep->world, slot);
            float p[3], r[4];
            pose_of(ep, slot, p, r);
            const char *why = NULL;
            for (int i = 0; i < 3 && !why; i++) {
                if (p[i] < st->scene.bounds_min[i] || p[i] > st->scene.bounds_max[i]) {
                    why = "outside the bounds";
                }
            }
            body_sync_authority_t a;
            bool bound = body_sync_authority(ep->sync, id, &a);
            bool mine = bound && body_sync_is_local(ep->sync, id);
            if (!why && mine && b3Body_GetType(s->body) == b3_kinematicBody && !(s->desc.route != BODY_SYNC_ROUTE_NONE && !s->knocked) &&
                ep->character.held != id) {
                why = "kinematic with no holder";
            }
            if (!why && mine && a.holder != BODY_SYNC_ID_NONE && ep->character.held != id) {
                why = "names a holder that does not hold it";
            }
            if (!why && e == 0 && bound && a.seat != BODY_SYNC_SEAT_HOST && a.seat < BODY_SYNC_ENDPOINTS_MAX && st->left[a.seat]) {
                why = "under a seat that has left";
            }
            if (!why && e != 0 && ep->frozen[slot] >= STUCK_TICKS) {
                why = "a copy frozen while the host's body moves";
            }
            if (!why && e != 0 && ep->pending_since[slot] && st->now - ep->pending_since[slot] >= STUCK_TICKS) {
                why = "a claim unanswered for 2 s";
            }
            if (why) {
                offenders++;
                snprintf(text, sizeof(text), "endpoint %u %s: %s", e, body_label(st, slot, name, sizeof(name)), why);
                check_detail(c, text);
            }
        }
    }
    c->value = (double)offenders;
    c->limit = 0.0;
    c->passed = offenders == 0;
}

static uint32_t knock_effects_on(const body_sync_stand_t *st, uint32_t target) {
    uint32_t n = 0;
    for (uint32_t i = 0; i < st->effect_count; i++) {
        n += st->effects[i].kind == STAND_EFFECT_KNOCK && st->effects[i].target == target ? 1u : 0u;
    }
    return n;
}

static void check_effects(body_sync_stand_t *st, body_sync_check_metrics_t *c) {
    uint32_t offenders = 0;
    char text[BODY_SYNC_ERROR_TEXT_MAX];
    bool out[BODY_SYNC_ENDPOINTS_MAX] = {false};
    for (uint32_t e = 1; e < st->endpoints; e++) {
        out[e] = guest_out(st, e, c, &offenders);
    }
    endpoint_t *host = &st->ep[0];
    /* On the host: every body knocked down exactly once, by exactly one accepted claim. */
    for (uint32_t slot = 0; slot < STAND_BODIES_MAX; slot++) {
        const stand_slot_t *s = &host->world->slots[slot];
        body_sync_id_t id = stand_world_id(host->world, slot);
        if (s->live && (s->knock_events > 0 || knock_effects_on(st, id) > 0) && (s->knock_events != 1 || knock_effects_on(st, id) != 1)) {
            offenders++;
            snprintf(text, sizeof(text), "host: slot %u knocked %u times by %u effects", slot, s->knock_events, knock_effects_on(st, id));
            check_detail(c, text);
        }
    }
    for (uint32_t i = 0; i < st->effect_count; i++) {
        const effect_t *f = &st->effects[i];
        uint32_t slot = body_sync_id_slot(f->target);
        if (f->dropped > 0) {
            offenders++;
            snprintf(text, sizeof(text), "the break of slot %u lost %u of its spawns", slot, f->dropped);
            check_detail(c, text);
        }
        for (uint32_t e = 1; e < st->endpoints; e++) {
            endpoint_t *ep = &st->ep[e];
            if (!ep->built || out[e]) {
                continue;
            }
            if (f->kind == STAND_EFFECT_KNOCK && stand_world_slot(ep->world, f->target) &&
                !(ep->knock_id[slot] == f->target && ep->knock_count[slot] == 1)) {
                offenders++;
                snprintf(text, sizeof(text), "g%u: knockdown of slot %u shown %u times", e, slot,
                         ep->knock_id[slot] == f->target ? (unsigned)ep->knock_count[slot] : 0u);
                check_detail(c, text);
            }
            if (f->kind == STAND_EFFECT_BREAK) {
                if (stand_world_slot(ep->world, f->target)) {
                    offenders++;
                    snprintf(text, sizeof(text), "g%u: broken slot %u still there", e, slot);
                    check_detail(c, text);
                }
                for (uint32_t k = 0; k < f->spawn_count; k++) {
                    uint32_t s = body_sync_id_slot(f->spawns[k]);
                    bool once = stand_world_slot(ep->world, f->spawns[k]) && ep->spawn_id[s] == f->spawns[k] && ep->spawn_count[s] == 1;
                    if (!once) {
                        offenders++;
                        snprintf(text, sizeof(text), "g%u: spawned slot %u not there exactly once", e, s);
                        check_detail(c, text);
                    }
                }
            }
        }
    }
    for (uint32_t e = 1; e < st->endpoints; e++) {
        endpoint_t *ep = &st->ep[e];
        for (uint32_t slot = 0; ep->built && !out[e] && slot < STAND_BODIES_MAX; slot++) {
            if (ep->knock_count[slot] > 1) {
                offenders++;
                snprintf(text, sizeof(text), "g%u: slot %u knocked down %u times", e, slot, (unsigned)ep->knock_count[slot]);
                check_detail(c, text);
            }
        }
    }
    c->value = (double)offenders;
    c->limit = 0.0;
    c->passed = offenders == 0;
}

/* The scene body's knockdown or break, applied on the host, shown once by every guest that stayed:
   effects_once holds what was applied to every endpoint, this holds that the effect happened. */
static void check_effect(body_sync_stand_t *st, const body_sync_expect_t *x, body_sync_check_metrics_t *c) {
    uint32_t index = body_sync_scene_find_body(&st->scene, x->body);
    body_sync_id_t id = index < STAND_BODIES_MAX ? st->start_id[index] : BODY_SYNC_ID_NONE;
    const effect_t *f = NULL;
    for (uint32_t i = 0; id != BODY_SYNC_ID_NONE && i < st->effect_count && !f; i++) {
        if ((st->effects[i].kind == STAND_EFFECT_KNOCK || st->effects[i].kind == STAND_EFFECT_BREAK) && st->effects[i].target == id) {
            f = &st->effects[i];
        }
    }
    uint32_t offenders = 0, shown = 0, guests = 0;
    char text[BODY_SYNC_ERROR_TEXT_MAX];
    if (!f) {
        offenders++;
        snprintf(text, sizeof(text), "%s: no effect took it on the host", x->body);
        check_detail(c, text);
    }
    uint32_t slot = body_sync_id_slot(id);
    for (uint32_t e = 1; f && e < st->endpoints; e++) {
        endpoint_t *ep = &st->ep[e];
        if (!ep->built || guest_out(st, e, c, &offenders)) {
            continue;
        }
        guests++;
        bool ok = f->kind == STAND_EFFECT_KNOCK ? ep->knock_id[slot] == id && ep->knock_count[slot] == 1 : !stand_world_slot(ep->world, id);
        if (ok) {
            shown++;
        } else {
            offenders++;
            snprintf(text, sizeof(text), "g%u: %s not %s", e, x->body, f->kind == STAND_EFFECT_KNOCK ? "knocked down once" : "gone");
            check_detail(c, text);
        }
    }
    if (offenders == 0) {
        snprintf(text, sizeof(text), "%s %s on the host and %u of %u guests", x->body, f->kind == STAND_EFFECT_KNOCK ? "knocked down" : "broken",
                 shown, guests);
        check_detail(c, text);
    }
    c->value = (double)offenders;
    c->limit = 0.0;
    c->passed = offenders == 0;
}

static int32_t label_index(const body_sync_stand_t *st, const char *label) {
    for (uint32_t i = 0; i < st->scenario.action_count; i++) {
        if (strcmp(st->scenario.actions[i].label, label) == 0) {
            return (int32_t)i;
        }
    }
    return -1;
}

/* The ticks the network may hold back what the walker meets, against the reference where every
   player shares one world: another guest's body crosses two one-way paths (to the host, which
   relays it at once, and on to her) and her playback delay (at most SYNC_DELAY_MAX); a cut, a
   stall or a deaf spell holds a path for its length on top. Her own steps and her answers to
   what she touches are local on both runs, so nothing else of the sync may slow her. */
static uint32_t pace_lag_ticks(const body_sync_stand_t *st) {
    uint32_t way = (uint32_t)ceilf(st->one_way_max_ms * (float)BODY_SYNC_TICK_HZ / 1000.0f);
    return 2u * way + SYNC_DELAY_MAX + st->frozen_max;
}

static body_sync_stand_t *create_stand(const body_sync_stand_config_t *config, bool local, body_sync_error_t *error);

/* A reference for pace check `i`, once the run has ended: the same scenario, seed and modes on the
   host's world alone, every player's character local there, no network, and every player but the
   walker acting `shift` ticks late. Returns its walk's progress, NULL when it never ran. */
static float *pace_reference(body_sync_stand_t *st, uint32_t i, uint8_t walker, uint32_t shift, uint32_t *len) {
    body_sync_stand_config_t config = {.scene = &st->scene, .scenario = &st->scenario, .override_seed = true, .seed = st->seed,
                                       .override_light_mass = true, .light_mass_max = st->light_mass,
                                       .override_authority = true, .authority = st->authority};
    body_sync_error_t error;
    body_sync_stand_t *ref = create_stand(&config, true, &error);
    *len = 0;
    if (!ref) {
        return NULL;
    }
    ref->ref_walker = walker;
    ref->ref_shift = shift;
    while (body_sync_stand_tick(ref)) {
    }
    float *out = ref->pace[i];
    *len = ref->pace_len[i];
    ref->pace[i] = NULL;
    body_sync_stand_destroy(ref);
    return out;
}

static void check_pace_now(body_sync_stand_t *st, uint32_t i, const char *label, body_sync_check_metrics_t *c);

/* `expect pace`: at every tick of the walk, its progress is at least a reference's `lag` ticks
   earlier, less the walk's arrival distance (within it a walk is done on either run). What the
   others do reaches the walker's world up to `lag` ticks late, and a body met later is met
   elsewhere on her path: a throw the host plays a round trip late can leave debris in her way
   that the same throw on time had cleared. So the walk keeps pace with one reference at least
   among the others' delays 0..lag, tried at every tick; a copy that stops her where every one of
   them gave way still fails. */
static void check_pace(body_sync_stand_t *st, uint32_t i, body_sync_check_metrics_t *c) {
    const char *label = st->scenario.expects[i].labels[0];
    if (!st->ended) {
        c->passed = false;
        c->value = -1.0;
        snprintf(c->detail, sizeof(c->detail), "%s: the run has not ended", label);
        return;
    }
    if (!st->pace_checked[i]) {
        st->pace_checked[i] = true;
        check_pace_now(st, i, label, &st->pace_check[i]);
    }
    c->passed = st->pace_check[i].passed;
    c->value = st->pace_check[i].value;
    c->limit = st->pace_check[i].limit;
    snprintf(c->detail, sizeof(c->detail), "%s", st->pace_check[i].detail);
}

static void check_pace_now(body_sync_stand_t *st, uint32_t i, const char *label, body_sync_check_metrics_t *c) {
    uint32_t lag = pace_lag_ticks(st);
    c->limit = (double)STAND_ARRIVE_M;
    c->value = -1.0;
    c->passed = false;
    int32_t index = label_index(st, label);
    if (index < 0 || st->pace_len[i] == 0) {
        snprintf(c->detail, sizeof(c->detail), "%s: the walk never started", label);
        return;
    }
    uint8_t walker = st->scenario.actions[index].player;
    bool others = false;
    for (uint32_t a = 0; a < st->scenario.action_count; a++) {
        others = others || st->scenario.actions[a].player != walker;
    }
    uint32_t last = others && !st->plant_pace_on_time ? lag : 0u, worst_at = 0, at_shift = 0;
    for (uint32_t shift = 0; shift <= last && !c->passed; shift++) {
        uint32_t ref_len;
        float *ref = pace_reference(st, i, walker, shift, &ref_len);
        if (ref && ref_len > 0) {
            double value = 0.0;
            uint32_t at = 0;
            for (uint32_t k = lag; k < st->pace_len[i]; k++) {
                uint32_t r = k - lag < ref_len ? k - lag : ref_len - 1u;
                double behind = (double)ref[r] - (double)st->pace[i][k];
                if (behind > value) {
                    value = behind;
                    at = k;
                }
            }
            if (c->value < 0.0 || value < c->value) {
                c->value = value;
                worst_at = at;
                at_shift = shift;
            }
            c->passed = c->value <= c->limit;
        }
        free(ref);
    }
    if (c->value < 0.0) {
        snprintf(c->detail, sizeof(c->detail), "%s: the reference run's walk never started", label);
        return;
    }
    snprintf(c->detail, sizeof(c->detail), "%s: %.3g m behind the reference %u ticks earlier (others %u ticks late), at tick %u of the walk", label,
             c->value, lag, at_shift, worst_at);
}

static void evaluate(body_sync_stand_t *st) {
    body_sync_report_t *r = &st->report;
    memset(r, 0, sizeof(*r));
    snprintf(r->scenario, sizeof(r->scenario), "%s", st->scenario.name);
    snprintf(r->scene, sizeof(r->scene), "%s", st->scene.name);
    snprintf(r->implementation, sizeof(r->implementation), "%s", body_sync_implementation());
    r->seed = st->seed;
    r->tick_hz = BODY_SYNC_TICK_HZ;
    r->light_mass_max = st->light_mass;
    r->authority = st->authority;
    r->ticks = st->now;
    r->rest_tick = st->rest_tick;
    stand_metrics_summarize(&st->metrics, r);
    endpoint_t *host = &st->ep[0];
    for (uint8_t g = 1; g < st->endpoints; g++) {
        close_late(st, g);
    }
    for (uint32_t g = 1; g < st->endpoints; g++) {
        body_sync_guest_metrics_t *gm = &r->guests[g - 1];
        coop_seat_stats_t stats;
        if (coop_session_seat_stats(host->session, (uint8_t)g, &stats)) {
            gm->rtt_ms = (double)stats.rtt_ms;
            gm->loss = (double)stats.loss;
        } else {
            gm->rtt_ms = gm->loss = -1.0;
        }
        body_sync_traffic_t a, b;
        memset(&a, 0, sizeof(a));
        memset(&b, 0, sizeof(b));
        body_sync_traffic(host->sync, (uint8_t)g, &a);
        if (st->ep[g].built) {
            body_sync_traffic(st->ep[g].sync, BODY_SYNC_SEAT_HOST, &b);
        }
        gm->refused = a.refused + b.refused;
        gm->takes = a.takes;
        gm->take_refusals = a.take_refusals;
        gm->reclaims = a.reclaims;
        gm->returns = b.returns;
        gm->stale = a.stale + b.stale;
        gm->rejected = a.rejected + b.rejected;
        gm->late_drops = st->late_drops[g];
        gm->deferred = a.deferred + b.deferred;
        gm->stale_max_ticks = a.stale_max_ticks > b.stale_max_ticks ? a.stale_max_ticks : b.stale_max_ticks;
        gm->flush_over_limit = st->flush_over[g - 1];
        gm->packets_to_guest = a.packets_sent;
        gm->claims = st->claims_in[g];
        gm->claims_overflow = st->claims_overflow[g];
        gm->claims_repeat = st->claims_repeat[g];
        gm->claims_rejected = st->claims_rejected[g];
        gm->claims_idle = st->claims_idle[g];
        gm->claim_effects = st->claim_effects[g];
        gm->claims_format = st->claims_format[g];
        gm->claims_ended = st->claims_ended[g];
    }
    r->action_count = st->scenario.action_count;
    for (uint32_t i = 0; i < st->scenario.action_count; i++) {
        const body_sync_action_t *a = &st->scenario.actions[i];
        const action_track_t *t = &st->actions[i];
        body_sync_action_metrics_t *m = &r->actions[i];
        m->line = a->line;
        snprintf(m->label, sizeof(m->label), "%s", a->label);
        m->player = a->player;
        m->verb = a->verb;
        m->start_tick = t->started ? t->start_tick : UINT32_MAX;
        m->outcome = t->outcome;
        m->host_ticks = t->started ? t->host_ticks : -1;
        m->echo_ticks = t->started ? t->echo_ticks : -1;
        m->seen_ticks = t->started ? t->seen_ticks : -1;
        m->own_ticks = t->started ? t->own_ticks : -1;
        m->shows = t->started && t->shows;
        m->answered = t->started && t->answered;
        if (a->verb == BODY_SYNC_VERB_WAIT) {
            m->host_ticks = m->echo_ticks = m->seen_ticks = -1;
        }
    }
    r->sessions_closed = st->metrics.closed_count;
    r->check_count = st->scenario.expect_count;
    r->passed = true;
    for (uint32_t i = 0; i < st->scenario.expect_count; i++) {
        const body_sync_expect_t *x = &st->scenario.expects[i];
        body_sync_check_metrics_t *c = &r->checks[i];
        c->line = x->line;
        c->kind = x->kind;
        switch (x->kind) {
        case BODY_SYNC_EXPECT_AGREE_AT_REST:
            check_agree(st, x, c);
            break;
        case BODY_SYNC_EXPECT_NO_STUCK:
            check_stuck(st, c);
            break;
        case BODY_SYNC_EXPECT_EFFECTS_ONCE:
            check_effects(st, c);
            break;
        case BODY_SYNC_EXPECT_RESULT: {
            int32_t index = label_index(st, x->labels[0]);
            body_sync_outcome_t got = index >= 0 ? st->actions[index].outcome : BODY_SYNC_OUTCOME_PENDING;
            c->passed = got == x->outcome;
            c->value = (double)got;
            c->limit = (double)x->outcome;
            snprintf(c->detail, sizeof(c->detail), "%s: %s, expected %s", x->labels[0], body_sync_outcome_name(got), body_sync_outcome_name(x->outcome));
            break;
        }
        case BODY_SYNC_EXPECT_ONE_OF: {
            uint32_t count = 0;
            for (uint32_t l = 0; l < x->label_count; l++) {
                int32_t index = label_index(st, x->labels[l]);
                count += index >= 0 && st->actions[index].outcome == x->outcome ? 1u : 0u;
            }
            c->passed = count == 1;
            c->value = (double)count;
            c->limit = 1.0;
            snprintf(c->detail, sizeof(c->detail), "%u of %u ended %s", count, x->label_count, body_sync_outcome_name(x->outcome));
            break;
        }
        case BODY_SYNC_EXPECT_METRIC_AT_MOST: {
            double v = 0.0;
            char why[BODY_SYNC_ERROR_TEXT_MAX];
            stand_metric_value(r, &st->metrics, x->metric, &v, why, sizeof(why));
            c->value = v;
            c->limit = x->limit;
            c->passed = x->at_least ? v >= x->limit : v <= x->limit;
            snprintf(c->detail, sizeof(c->detail), "%s%s%s", x->metric, why[0] ? ": " : "", why);
            break;
        }
        case BODY_SYNC_EXPECT_MOVED: {
            body_sync_id_t id = body_id_by_name(st, host, x->body);
            stand_slot_t *s = id != BODY_SYNC_ID_NONE ? stand_world_slot(host->world, id) : NULL;
            float at[3], rot[4];
            double d = -1.0;
            if (s) {
                pose_of(host, body_sync_id_slot(id), at, rot);
                d = (double)stand_distance3(at, st->start_position[body_sync_id_slot(id)]);
            }
            c->value = d;
            c->limit = st->light_mass > 0.0f && x->layer_limit >= 0.0 ? x->layer_limit : x->limit;
            c->passed = d >= 0.0 && (x->at_least ? d >= c->limit : d <= c->limit);
            snprintf(c->detail, sizeof(c->detail), "%s moved %.3g m on the host, expected %s %.3g m", x->body, d, x->at_least ? ">=" : "<=", c->limit);
            break;
        }
        case BODY_SYNC_EXPECT_PACE:
            check_pace(st, i, c);
            break;
        case BODY_SYNC_EXPECT_EFFECT:
            check_effect(st, x, c);
            break;
        case BODY_SYNC_EXPECT_LEFT: {
            bool gone = st->left[x->endpoint];
            c->passed = gone;
            c->value = gone ? (double)st->left_tick[x->endpoint] : -1.0;
            c->limit = 0.0;
            if (gone) {
                snprintf(c->detail, sizeof(c->detail), "g%u left at tick %u", x->endpoint, st->left_tick[x->endpoint]);
            } else {
                snprintf(c->detail, sizeof(c->detail), "g%u stayed", x->endpoint);
            }
            break;
        }
        }
        r->passed = r->passed && c->passed;
    }
}

/* ---- create and destroy ---- */

static void connect_tick(body_sync_stand_t *st) {
    net_link_sim_advance(st->sim, 1.0 / (double)BODY_SYNC_TICK_HZ);
    for (uint32_t i = 0; i < st->endpoints; i++) {
        endpoint_t *ep = &st->ep[i];
        if (ep->session && ep->active) {
            coop_session_set_time(ep->session, net_link_sim_time(st->sim));
            poll_events(st, ep);
            coop_session_end_tick(ep->session);
        }
    }
}

static bool check_scenario(const body_sync_stand_t *st, body_sync_error_t *error) {
    const body_sync_scenario_t *s = &st->scenario;
    if (strcmp(s->scene, st->scene.name) != 0) {
        error_text(error, 0, "the scenario names another scene");
        return false;
    }
    for (uint32_t i = 0; i < s->action_count; i++) {
        const body_sync_action_t *a = &s->actions[i];
        if (a->player >= st->scene.player_count) {
            error_text(error, a->line, "the scene has no spawn point for this player");
            return false;
        }
        if (a->has_target && a->target.is_body && body_sync_scene_find_body(&st->scene, a->target.body) == UINT32_MAX) {
            char text[BODY_SYNC_ERROR_TEXT_MAX];
            snprintf(text, sizeof(text), "the scene has no body '%s'", a->target.body);
            error_text(error, a->line, text);
            return false;
        }
        bool by_host = a->verb == BODY_SYNC_VERB_RECALL || a->verb == BODY_SYNC_VERB_REMOVE;
        if (by_host && st->scene.bodies[body_sync_scene_find_body(&st->scene, a->target.body)].motion == BODY_SYNC_MOTION_STATIC) {
            error_text(error, a->line, "a static body is never bound: the host can recall or remove only one that moves");
            return false;
        }
        for (int k = 0; a->has_home && k < 3; k++) {
            if (!(a->home[k] > st->scene.bounds_min[k] && a->home[k] < st->scene.bounds_max[k])) {
                error_text(error, a->line, "this point is outside the scene's bounds");
                return false;
            }
        }
    }
    for (uint32_t i = 0; i < s->expect_count; i++) {
        const body_sync_expect_t *x = &s->expects[i];
        if ((x->kind == BODY_SYNC_EXPECT_MOVED || x->kind == BODY_SYNC_EXPECT_EFFECT) && body_sync_scene_find_body(&st->scene, x->body) == UINT32_MAX) {
            char text[BODY_SYNC_ERROR_TEXT_MAX];
            snprintf(text, sizeof(text), "the scene has no body '%s'", x->body);
            error_text(error, x->line, text);
            return false;
        }
    }
    return true;
}

void body_sync_stand_destroy(body_sync_stand_t *st) {
    if (!st) {
        return;
    }
    for (uint32_t i = st->endpoints; i-- > 0;) {
        endpoint_t *ep = &st->ep[i];
        coop_session_destroy(ep->session);
        if (ep->link) {
            net_link_destroy(ep->link);
        }
        body_sync_destroy(ep->sync);
        free(ep->rings);
        free(ep->drawn_rings);
        free(ep->drawn_paused);
        free(ep->drawn_turns);
        if (ep->world) {
            stand_world_destroy(ep->world);
            free(ep->world);
        }
    }
    net_link_sim_destroy(st->sim);
    for (uint32_t i = 0; i < BODY_SYNC_SCENARIO_EXPECTS_MAX; i++) {
        free(st->pace[i]);
    }
    free(st);
}

static body_sync_stand_t *create_stand(const body_sync_stand_config_t *config, bool local, body_sync_error_t *error) {
    if (error) {
        memset(error, 0, sizeof(*error));
    }
    if (!config || !config->scene || !config->scenario) {
        error_text(error, 0, "no scene or no scenario");
        return NULL;
    }
    if (!body_sync_scene_validate(config->scene, error)) {
        return NULL;
    }
    body_sync_stand_t *st = calloc(1, sizeof(*st));
    if (!st) {
        error_text(error, 0, "out of memory");
        return NULL;
    }
    st->scene = *config->scene;
    st->scenario = *config->scenario;
    st->series = config->series;
    st->seed = config->override_seed ? config->seed : st->scenario.seed;
    st->light_mass = config->override_light_mass ? config->light_mass_max : st->scenario.light_mass_max;
    st->authority = config->override_authority ? config->authority : st->scenario.authority;
    if (st->authority == BODY_SYNC_AUTHORITY_INTERACTION && !(st->light_mass > 0.0f)) {
        error_text(error, 0, "authority interaction needs light_mass: it takes only light bodies");
        free(st);
        return NULL;
    }
    st->local = local;
    st->endpoints = local ? 1u : 1u + st->scenario.guests;
    st->rest_tick = UINT32_MAX;
    st->limits = stand_limits_of(&st->scene);
    st->claim_reach_scale = st->claim_approach_scale = 1.0f;
    if (!check_scenario(st, error)) {
        free(st);
        return NULL;
    }
    stand_rng_seed(&st->rng, st->seed);
    stand_schedule_init(&st->schedule, &st->scenario);
    stand_metrics_reset(&st->metrics, st->scenario.guests);
    for (uint32_t i = 0; i < st->scenario.action_count; i++) {
        st->actions[i].host_ticks = st->actions[i].echo_ticks = st->actions[i].seen_ticks = st->actions[i].own_ticks = -1;
    }
    for (uint32_t i = 0; i < st->scenario.expect_count; i++) {
        if (st->scenario.expects[i].kind == BODY_SYNC_EXPECT_PACE && !(st->pace[i] = calloc(BODY_SYNC_STAND_TICKS_MAX, sizeof(float)))) {
            error_text(error, 0, "out of memory");
            body_sync_stand_destroy(st);
            return NULL;
        }
    }
    uint32_t places = st->scenario.guests > 0 ? st->scenario.guests : 1u;
    net_link_sim_config_t sim = {.seed = st->seed, .max_guests = places, .lanes = coop_session_lanes()};
    st->sim = net_link_sim_create(&sim);
    if (!st->sim) {
        error_text(error, 0, "the network simulator refused its config");
        body_sync_stand_destroy(st);
        return NULL;
    }
    for (uint32_t i = 0; i < st->endpoints; i++) {
        endpoint_t *ep = &st->ep[i];
        ep->index = (uint8_t)i;
        ep->rate_mhz = st->scenario.rate_mhz[i];
        ep->world = calloc(1, sizeof(*ep->world));
        ep->rings = calloc(STAND_BODIES_MAX, sizeof(*ep->rings));
        ep->drawn_rings = calloc(STAND_BODIES_MAX, sizeof(*ep->drawn_rings));
        ep->drawn_paused = calloc((size_t)STAND_BODIES_MAX * RING_TICKS, 1);
        ep->drawn_turns = calloc((size_t)STAND_BODIES_MAX * RING_TICKS, sizeof(*ep->drawn_turns));
        if (!ep->world || !ep->rings || !stand_world_build(ep->world, &st->scene, 1u + st->scenario.guests, error)) {
            if (error && !error->text[0]) {
                error_text(error, 0, "out of memory");
            }
            body_sync_stand_destroy(st);
            return NULL;
        }
        body_sync_config_t sc = {.world = ep->world->world,
                                 .seat = (uint8_t)i,
                                 .seats = st->endpoints,
                                 .tick_hz = BODY_SYNC_TICK_HZ,
                                 .max_speed = st->limits.max_speed,
                                 .max_spin = st->limits.max_spin,
                                 .character_category = STAND_CAT_CHARACTER,
                                 .light_mass_max = st->light_mass,
                                 .authority = (body_sync_authority_mode_t)st->authority};
        memcpy(sc.bounds_min, st->limits.bounds_min, sizeof(sc.bounds_min));
        memcpy(sc.bounds_max, st->limits.bounds_max, sizeof(sc.bounds_max));
        ep->sync = body_sync_create(&sc);
        if (!ep->sync) {
            error_text(error, 0, "the seam refused its config");
            body_sync_stand_destroy(st);
            return NULL;
        }
        for (uint32_t slot = 0; slot < STAND_BODIES_MAX; slot++) {
            stand_slot_t *s = &ep->world->slots[slot];
            if (!s->live || s->desc.motion == BODY_SYNC_MOTION_STATIC) {
                continue;
            }
            body_sync_bind_t bind = {
                .body = s->body,
                .body_class = s->character ? BODY_SYNC_CLASS_CHARACTER : (s->desc.flags & BODY_SYNC_BODY_HEAVY) ? BODY_SYNC_CLASS_HEAVY : BODY_SYNC_CLASS_LIGHT,
                .authority = s->character && !local ? s->player : BODY_SYNC_SEAT_HOST,
            };
            body_sync_bind(ep->sync, stand_world_id(ep->world, slot), &bind);
        }
        stand_character_init(&ep->character, ep->world, (uint8_t)i);
        for (uint32_t p = 1; local && p <= st->scenario.guests; p++) {
            stand_character_init(&ep->others[p - 1u], ep->world, (uint8_t)p);
        }
        ep->built = true;
    }
    endpoint_t *host = &st->ep[0];
    const uint64_t build_key = 0xB0D75A11C0DEull ^ BODY_SYNC_TICK_HZ;
    host->link = net_link_sim_host(st->sim, 1, places);
    coop_session_config_t hc = {.link = host->link, .seats = st->endpoints, .build_key = build_key, .world_key = st->scene.name,
                                .world_key_size = (uint32_t)strlen(st->scene.name), .tick_hz = BODY_SYNC_TICK_HZ};
    host->session = host->link ? coop_session_host(&hc) : NULL;
    if (!host->session) {
        error_text(error, 0, "the host's session did not start");
        body_sync_stand_destroy(st);
        return NULL;
    }
    host->active = true;
    for (uint32_t g = 1; g < st->endpoints; g++) {
        endpoint_t *ep = &st->ep[g];
        ep->link = net_link_sim_guest(st->sim, 100u + g);
        coop_session_config_t gc = {.link = ep->link, .build_key = build_key, .player_key = g, .world_key = st->scene.name,
                                    .world_key_size = (uint32_t)strlen(st->scene.name)};
        ep->session = ep->link ? coop_session_join(&gc) : NULL;
        if (!ep->session) {
            error_text(error, 0, "a guest's session did not start");
            body_sync_stand_destroy(st);
            return NULL;
        }
        ep->active = true;
        uint32_t waited = 0;
        while (coop_session_seat(ep->session) == COOP_SEAT_NONE && ep->active && waited++ < CONNECT_TICKS_MAX) {
            connect_tick(st);
        }
        if (coop_session_seat(ep->session) != g) {
            error_text(error, 0, "a guest was not seated in its own seat in time");
            body_sync_stand_destroy(st);
            return NULL;
        }
    }
    /* Let the last welcome's seat notices reach everyone before tick 0. */
    for (int i = 0; i < 30; i++) {
        connect_tick(st);
    }
    st->metrics.closed_count = 0;
    for (uint32_t i = 0; i < st->endpoints; i++) {
        rebuild_view(&st->ep[i]);
    }
    return st;
}

body_sync_stand_t *body_sync_stand_create(const body_sync_stand_config_t *config, body_sync_error_t *error) {
    return create_stand(config, false, error);
}

/* ---- the public reads ---- */

const body_sync_report_t *body_sync_stand_run(body_sync_stand_t *st) {
    while (body_sync_stand_tick(st)) {
    }
    return body_sync_stand_report(st);
}

const body_sync_report_t *body_sync_stand_report(body_sync_stand_t *st) {
    evaluate(st);
    return &st->report;
}

uint32_t body_sync_stand_now(const body_sync_stand_t *st) { return st ? st->now : 0; }

uint32_t body_sync_stand_endpoint_count(const body_sync_stand_t *st) { return st ? st->endpoints : 0; }

uint64_t body_sync_stand_world_hash(const body_sync_stand_t *st, uint32_t endpoint) {
    return st && endpoint < st->endpoints && st->ep[endpoint].built ? stand_world_hash(st->ep[endpoint].world) : 0;
}

uint32_t body_sync_stand_body_count(const body_sync_stand_t *st, uint32_t endpoint) {
    return st && endpoint < st->endpoints ? st->ep[endpoint].view_count : 0;
}

bool body_sync_stand_body(const body_sync_stand_t *st, uint32_t endpoint, uint32_t index, body_sync_body_view_t *out) {
    if (!st || endpoint >= st->endpoints || index >= st->ep[endpoint].view_count || !out) {
        return false;
    }
    const endpoint_t *ep = &st->ep[endpoint];
    uint32_t slot = ep->view[index];
    const stand_slot_t *s = &ep->world->slots[slot];
    if (!s->live) {
        return false;
    }
    memset(out, 0, sizeof(*out));
    body_sync_id_t id = stand_world_id(ep->world, slot);
    out->id = id;
    out->name = slot < st->scene.body_count || s->character ? s->desc.name : "";
    out->shape = s->desc.shape;
    memcpy(out->size, s->desc.size, sizeof(out->size));
    drawn_pose(ep, slot, out->position, out->rotation);
    out->model_tag = s->desc.model_tag;
    out->role = s->desc.role;
    out->player = s->character ? s->player : BODY_SYNC_PLAYERS_MAX;
    out->authority = BODY_SYNC_SEAT_HOST;
    if (s->desc.motion == BODY_SYNC_MOTION_STATIC) {
        out->flags |= BODY_SYNC_VIEW_STATIC;
    }
    if (s->character) {
        out->flags |= BODY_SYNC_VIEW_CHARACTER;
    }
    body_sync_authority_t a;
    if (body_sync_authority(ep->sync, id, &a)) {
        out->authority = a.seat;
        out->flags |= body_sync_is_local(ep->sync, id) ? 0u : BODY_SYNC_VIEW_COPY;
        out->flags |= a.holder != BODY_SYNC_ID_NONE ? BODY_SYNC_VIEW_HELD : 0u;
        out->flags |= a.hold == BODY_SYNC_HOLD_UNTIL_SETTLED ? BODY_SYNC_VIEW_FLYING : 0u;
    }
    if (!b3Body_IsAwake(s->body)) {
        out->flags |= BODY_SYNC_VIEW_ASLEEP;
    }
    if (s->knocked) {
        out->flags |= BODY_SYNC_VIEW_KNOCKED;
    }
    if (s->character) {
        /* Before its owner's first record a copy has no activity yet: it stands. */
        uint8_t activity = 0;
        body_sync_data(ep->sync, id, &activity, 1);
        out->activity = activity >= BODY_SYNC_ACTIVITY_IDLE && activity <= BODY_SYNC_ACTIVITY_PUNCH ? (body_sync_activity_t)activity
                                                                                                    : BODY_SYNC_ACTIVITY_IDLE;
    }
    return true;
}

bool body_sync_stand_write_json(body_sync_stand_t *st, char *buffer, size_t capacity, size_t *size) {
    evaluate(st);
    return stand_json_write(&st->report, &st->metrics, st->series, buffer, capacity, size);
}

bool body_sync_stand_save_json(body_sync_stand_t *st, const char *path, body_sync_error_t *error) {
    size_t size = 0;
    body_sync_stand_write_json(st, NULL, 0, &size);
    char *text = malloc(size + 1);
    if (!text || !body_sync_stand_write_json(st, text, size + 1, &size)) {
        free(text);
        error_text(error, 0, "out of memory for the metrics");
        return false;
    }
    FILE *file = path ? fopen(path, "wb") : NULL;
    bool ok = file && fwrite(text, 1, size, file) == size;
    if (file) {
        ok = fclose(file) == 0 && ok;
    }
    free(text);
    if (!ok) {
        error_text(error, 0, "cannot write the metrics file");
    }
    return ok;
}

/* ---- test access (stand_testing.h) ---- */

stand_world_t *stand_testing_world(body_sync_stand_t *st, uint32_t endpoint) {
    return st && endpoint < st->endpoints ? st->ep[endpoint].world : NULL;
}

body_sync_t *stand_testing_sync(body_sync_stand_t *st, uint32_t endpoint) {
    return st && endpoint < st->endpoints ? st->ep[endpoint].sync : NULL;
}

bool stand_testing_effect(body_sync_stand_t *st, stand_effect_t effect, body_sync_id_t target) {
    stand_slot_t *s = st ? stand_world_slot(st->ep[0].world, target) : NULL;
    if (!s) {
        return false;
    }
    float origin[3], rotation[4];
    pose_of(&st->ep[0], body_sync_id_slot(target), origin, rotation);
    origin[0] -= 1.0f;
    return apply_effect(st, effect, target, origin) != APPLIED_REFUSED;
}

void stand_testing_mark_left(body_sync_stand_t *st, uint8_t seat) { mark_left(st, seat); }

uint32_t stand_testing_tick(body_sync_stand_t *st, uint32_t endpoint) {
    return st && endpoint < st->endpoints ? coop_session_tick(st->ep[endpoint].session) : 0u;
}

bool stand_testing_record(body_sync_stand_t *st, uint8_t seat, const uint8_t *data, uint32_t size) {
    if (!st || seat == BODY_SYNC_SEAT_HOST || seat >= st->endpoints || size < 1 || data[0] != STAND_FAMILY_STAND) {
        return false;
    }
    return stand_bytes_in(st, &st->ep[0], seat, coop_session_tick(st->ep[seat].session), data + 1, size - 1);
}

void stand_testing_plant_declared_velocity(body_sync_stand_t *st, uint32_t endpoint, body_sync_id_t body, const float add[3], uint32_t ticks) {
    st->plant_declared_endpoint = endpoint;
    st->plant_declared_body = body;
    memcpy(st->plant_declared_add, add, sizeof(st->plant_declared_add));
    st->plant_declared_ticks = ticks;
}

void stand_testing_plant_claim_limits(body_sync_stand_t *st, float reach_scale, float approach_scale) {
    st->claim_reach_scale = reach_scale;
    st->claim_approach_scale = approach_scale;
}

void stand_testing_plant_shift(body_sync_stand_t *st, uint32_t ticks) { st->plant_shift = ticks; }
void stand_testing_plant_keep_broken(body_sync_stand_t *st, uint8_t endpoint) { st->plant_keep_broken = endpoint; }
void stand_testing_plant_drift(body_sync_stand_t *st, float metres) { st->plant_drift = metres; }
void stand_testing_plant_lead(body_sync_stand_t *st, float metres) { st->plant_lead = metres; }
void stand_testing_plant_pace_on_time(body_sync_stand_t *st) { st->plant_pace_on_time = true; }

void stand_testing_plant_wall(body_sync_stand_t *st, uint32_t endpoint, const char *body) {
    uint32_t slot = body_sync_scene_find_body(&st->scene, body);
    st->plant_wall_endpoint = endpoint;
    if (slot != UINT32_MAX && st->plant_wall_count < sizeof(st->plant_wall_slots) / sizeof(st->plant_wall_slots[0])) {
        st->plant_wall_slots[st->plant_wall_count++] = slot;
    }
}
void stand_testing_plant_jolt(body_sync_stand_t *st, uint32_t tick, float metres, bool local) {
    st->plant_jolt_tick = tick;
    st->plant_jolt_m = metres;
    st->plant_jolt_local = local;
}

void stand_testing_plant_jitter(body_sync_stand_t *st, uint32_t tick, float metres, uint32_t every, uint32_t count) {
    stand_testing_plant_jolt(st, tick, metres, false);
    st->plant_jolt_every = every;
    st->plant_jolt_count = count;
}

void stand_testing_plant_offset(body_sync_stand_t *st, uint32_t tick, float metres, float radians) {
    st->plant_offset_tick = tick;
    st->plant_offset_by = metres;
    st->plant_offset_turn_by = radians;
}

void stand_testing_plant_pause(body_sync_stand_t *st, uint32_t tick, uint32_t ticks) {
    st->plant_pause_tick = tick;
    st->plant_pause_ticks = ticks;
}

void stand_testing_plant_creep(body_sync_stand_t *st, uint32_t tick, float metres, uint32_t ticks, bool copy) {
    st->plant_creep_tick = tick;
    st->plant_creep_m = metres;
    st->plant_creep_ticks = ticks;
    st->plant_creep_copy = copy;
    st->plant_creep_turn = 0.0f;
}

void stand_testing_plant_turn(body_sync_stand_t *st, uint32_t tick, float radians, uint32_t ticks) {
    stand_testing_plant_creep(st, tick, 0.0f, ticks, false);
    st->plant_creep_turn = radians;
}

/* The first light copy of a host body guest 1 holds (`awake`: one whose host body is awake), as
   slot + 1; 0 none. */
static uint32_t plant_copy(const body_sync_stand_t *st, bool awake) {
    const endpoint_t *ep = &st->ep[1];
    for (uint32_t slot = 0; slot < STAND_BODIES_MAX; slot++) {
        const stand_slot_t *s = &ep->world->slots[slot];
        body_sync_id_t id = stand_world_id(ep->world, slot);
        const stand_slot_t *hs = s->live ? stand_world_slot(st->ep[0].world, id) : NULL;
        if (hs && !s->character && s->desc.motion != BODY_SYNC_MOTION_STATIC && !(s->desc.flags & BODY_SYNC_BODY_HEAVY) && !body_sync_is_local(ep->sync, id) &&
            (!awake || b3Body_IsAwake(hs->body))) {
            return slot + 1u;
        }
    }
    return 0;
}

/* stand_testing_plant_pause and stand_testing_plant_creep: each picks its copy at its tick; the
   creep moves the host body or the held copy from then on, after every endpoint's tick. */
static void plant_pause_creep(body_sync_stand_t *st) {
    if (st->endpoints < 2 || !st->ep[1].built) {
        return;
    }
    if (st->plant_pause_ticks && st->now == st->plant_pause_tick) {
        st->plant_pause_slot = plant_copy(st, false);
    }
    if (st->plant_creep_ticks && st->now == st->plant_creep_tick) {
        st->plant_creep_slot = plant_copy(st, true);
        if (st->plant_creep_slot && st->plant_creep_copy && st->plant_wall_count < 8) {
            st->plant_wall_endpoint = 1;
            st->plant_wall_slots[st->plant_wall_count++] = st->plant_creep_slot - 1u;
        }
    }
    if (st->plant_creep_slot && st->now - st->plant_creep_tick < st->plant_creep_ticks) {
        stand_slot_t *c = &st->ep[st->plant_creep_copy ? 1 : 0].world->slots[st->plant_creep_slot - 1u];
        b3Vec3 p = b3Body_GetPosition(c->body);
        b3Quat q = b3Body_GetRotation(c->body);
        if (st->plant_creep_turn != 0.0f) {
            q = b3NormalizeQuat(b3MulQuat(b3MakeQuatFromAxisAngle((b3Vec3){0.0f, 1.0f, 0.0f}, st->plant_creep_turn), q));
        }
        b3Body_SetTransform(c->body, (b3Vec3){p.x + st->plant_creep_m, p.y, p.z}, q);
        b3Body_SetAwake(c->body, true);
    }
}

/* stand_testing_plant_offset: at its tick, the first light body guest 1 simulates takes the offset. */
static void plant_offset(body_sync_stand_t *st) {
    endpoint_t *ep = st->endpoints > 1 ? &st->ep[1] : NULL;
    if (!ep || st->plant_offset_tick == 0 || st->now != st->plant_offset_tick || !ep->built) {
        return;
    }
    for (uint32_t slot = 0; slot < STAND_BODIES_MAX; slot++) {
        stand_slot_t *s = &ep->world->slots[slot];
        body_sync_id_t id = stand_world_id(ep->world, slot);
        if (s->live && !s->character && s->desc.motion != BODY_SYNC_MOTION_STATIC && !(s->desc.flags & BODY_SYNC_BODY_HEAVY) && body_sync_is_local(ep->sync, id)) {
            st->plant_offset_slot = slot;
            st->plant_offset_id = id;
            st->plant_offset_m = st->plant_offset_by;
            st->plant_offset_rad = st->plant_offset_turn_by;
            return;
        }
    }
}

static void plant_jolt(body_sync_stand_t *st) {
    plant_offset(st);
    plant_pause_creep(st);
    endpoint_t *ep = st->endpoints > 1 ? &st->ep[1] : NULL;
    if (!ep || st->plant_jolt_tick == 0 || st->now < st->plant_jolt_tick || !ep->built) {
        return;
    }
    uint32_t since = st->now - st->plant_jolt_tick, every = st->plant_jolt_every ? st->plant_jolt_every : 1u;
    if (since % every != 0 || since / every >= (st->plant_jolt_count ? st->plant_jolt_count : 1u)) {
        return;
    }
    float metres = since / every % 2u ? -st->plant_jolt_m : st->plant_jolt_m; /* back and forth */
    for (uint32_t slot = 0; slot < STAND_BODIES_MAX; slot++) {
        stand_slot_t *s = &ep->world->slots[slot];
        body_sync_id_t id = stand_world_id(ep->world, slot);
        const stand_slot_t *hs = stand_world_slot(st->ep[0].world, id);
        bool driven = hs && b3Body_IsAwake(hs->body); /* the jitter's copy: driven on an awake owner's pose */
        if (s->live && !s->character && s->desc.motion != BODY_SYNC_MOTION_STATIC && !(s->desc.flags & BODY_SYNC_BODY_HEAVY) &&
            body_sync_is_local(ep->sync, id) == st->plant_jolt_local && (st->plant_jolt_every == 0 || driven)) {
            b3Vec3 p = b3Body_GetPosition(s->body);
            b3Body_SetTransform(s->body, (b3Vec3){p.x + metres, p.y, p.z}, b3Body_GetRotation(s->body));
            b3Body_SetAwake(s->body, true);
            return;
        }
    }
}
