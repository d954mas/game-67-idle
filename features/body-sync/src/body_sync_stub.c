#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "features/body_sync/body_sync.h"
#include "stub_wire.h"
#include "sync_authority.h"
#include "sync_masks.h"
#include "sync_policy.h"

/* The naive seam: every bound body's raw pose, velocities and motion type to every seat, every
   tick, in as many packets as it takes; copies are kinematic bodies driven to the newest pose.
   It is the reference for the contract and the baseline the real sender and receiver are
   measured against, so it is plain rather than clever. Authority is the shared part's. */

_Static_assert(BODY_SYNC_BODIES_MAX == STUB_SLOTS, "stub_wire.c checks slots against STUB_SLOTS");
_Static_assert(BODY_SYNC_DATA_MAX == STUB_DATA_MAX, "the stub writes BODY_SYNC_DATA_MAX bytes of data");
_Static_assert(BODY_SYNC_TRAFFIC_COUNT == 5, "body_sync_metrics.h repeats the seam's kind count");

/* A copy farther than this from its new pose jumps there instead of being driven at speed. */
#define SNAP_DISTANCE 2.0f
/* State packets per seat whose fate the stub waits for: past a second of them at 60 Hz the
   oldest is forgotten, as one never answered. */
enum { SENT_WINDOW = 64 };

typedef struct sent_packet {
    bool used;
    uint16_t number;
    uint32_t tick;
} sent_packet_t;

typedef struct entry {
    b3BodyId body;
    body_sync_class_t body_class;
    b3BodyType bound_type;
    b3MotionLocks bound_locks; /* restored when a copy turns local with no state received */
    bool copy_mode;
    bool placed;
    bool has_out;
    stub_body_state_t out;
    bool has_in;
    bool in_fresh;
    uint8_t in_from;
    uint32_t in_tick;
    bool in_ordered; /* in_tick orders that sender's next states: one session of the seat */
    /* Host: the state received is an old owner's, from before the body moved to another guest: a
       take-over goes on from the copy as shown, not from that owner's motion. */
    bool in_old;
    stub_body_state_t in;
    sync_masks_t masks;
} entry_t;

struct body_sync {
    body_sync_config_t config;
    stub_limits_t limits;
    float dt;
    uint32_t now; /* the last apply's or collect's tick */
    sync_authority_t authority;
    entry_t entries[BODY_SYNC_BODIES_MAX];
    bool joined[BODY_SYNC_SEATS_MAX];
    uint32_t cursor[BODY_SYNC_SEATS_MAX];
    bool writing[BODY_SYNC_SEATS_MAX];
    body_sync_traffic_t traffic[BODY_SYNC_SEATS_MAX];
    /* The round trip to each seat, ticks from a state packet's send to its arrival fate, and its
       mean deviation, smoothed as TCP does; 0 unknown. The claim gate's grace is built from them. */
    sent_packet_t sent[BODY_SYNC_SEATS_MAX][SENT_WINDOW];
    uint32_t sent_next[BODY_SYNC_SEATS_MAX];
    float round_trip[BODY_SYNC_SEATS_MAX], round_trip_dev[BODY_SYNC_SEATS_MAX];
    stub_body_state_t scratch[BODY_SYNC_BODIES_MAX];
    char error[128];
};

const char *body_sync_implementation(void) { return "stub 0.8.0"; }

const char *body_sync_traffic_kind_name(body_sync_traffic_kind_t kind) {
    static const char *names[] = {"header", "copy", "sim", "held", "authority"};
    return (unsigned)kind < BODY_SYNC_TRAFFIC_COUNT ? names[kind] : "unknown";
}

const char *body_sync_last_error(const body_sync_t *sync) { return sync ? sync->error : ""; }

static body_sync_result_t invalid(body_sync_t *s, const char *what) {
    snprintf(s->error, sizeof(s->error), "%s", what);
    return BODY_SYNC_INVALID;
}

static bool is_host(const body_sync_t *s) { return s->config.seat == BODY_SYNC_SEAT_HOST; }
static bool tick_newer(uint32_t a, uint32_t b) { return (int32_t)(a - b) > 0; }

static sync_authority_body_t *auth_of(body_sync_t *s, uint32_t slot) { return &s->authority.bodies[slot]; }

static entry_t *entry_for(body_sync_t *s, body_sync_id_t id) {
    sync_authority_body_t *b = sync_authority_body(&s->authority, id);
    return b ? &s->entries[body_sync_id_slot(id)] : NULL;
}

static const entry_t *entry_const(const body_sync_t *s, body_sync_id_t id) { return entry_for((body_sync_t *)s, id); }

static bool local_slot(const body_sync_t *s, uint32_t slot) { return sync_authority_local(&s->authority, &s->authority.bodies[slot]); }

body_sync_t *body_sync_create(const body_sync_config_t *config) {
    if (!config || !b3World_IsValid(config->world) || config->seats < 1 || config->seats > BODY_SYNC_SEATS_MAX ||
        config->seat >= config->seats || config->tick_hz < 1 || config->tick_hz > 1000 || !(isfinite(config->max_speed) && config->max_speed > 0.0f) ||
        !(isfinite(config->max_spin) && config->max_spin > 0.0f)) {
        return NULL;
    }
    for (int i = 0; i < 3; i++) {
        if (!(isfinite(config->bounds_min[i]) && isfinite(config->bounds_max[i]) && config->bounds_min[i] < config->bounds_max[i])) {
            return NULL;
        }
    }
    body_sync_t *s = calloc(1, sizeof(*s));
    if (!s) {
        return NULL;
    }
    s->config = *config;
    for (int i = 0; i < 3; i++) {
        s->limits.bounds_min[i] = config->bounds_min[i];
        s->limits.bounds_max[i] = config->bounds_max[i];
    }
    s->limits.max_speed = config->max_speed;
    s->limits.max_spin = config->max_spin;
    s->dt = 1.0f / (float)config->tick_hz;
    sync_authority_init(&s->authority, config->seat, (uint8_t)config->seats, s->error, sizeof(s->error));
    s->authority.rebind_ticks = sync_policy_rebind_ticks(config->tick_hz);
    s->authority.agent_stride = sync_policy_agent_stride(config->tick_hz, SYNC_AUTHORITY_AGENT_POSES);
    return s;
}

void body_sync_destroy(body_sync_t *sync) {
    if (!sync) {
        return;
    }
    for (uint32_t slot = 0; slot < BODY_SYNC_BODIES_MAX; slot++) {
        entry_t *e = &sync->entries[slot];
        if (sync->authority.bodies[slot].bound && b3Body_IsValid(e->body)) {
            sync_masks_restore(&e->masks, e->body);
        }
    }
    free(sync);
}

static bool seat_ok(const body_sync_t *s, uint8_t seat) { return seat < s->config.seats && seat != s->config.seat; }

/* A new session of the seat numbers its packets afresh, over a path of its own. */
static void forget_round_trip(body_sync_t *s, uint8_t seat) {
    memset(s->sent[seat], 0, sizeof(s->sent[seat]));
    s->sent_next[seat] = 0;
    s->round_trip[seat] = s->round_trip_dev[seat] = 0.0f;
}

/* The tick order holds within one session of a seat: a join forgets what the seat sent before,
   a leave keeps it (the host continues those bodies from it) without its order. */
static void forget_states(body_sync_t *s, uint8_t seat, bool keep_states) {
    for (uint32_t slot = 0; slot < BODY_SYNC_BODIES_MAX; slot++) {
        entry_t *e = &s->entries[slot];
        if (e->has_in && e->in_from == seat) {
            e->in_ordered = false;
            e->has_in = keep_states;
            e->in_fresh = e->in_fresh && keep_states;
        }
    }
}

void body_sync_seat_joined(body_sync_t *s, uint8_t seat) {
    if (!s || !seat_ok(s, seat) || (!is_host(s) && seat != BODY_SYNC_SEAT_HOST)) {
        return;
    }
    s->writing[seat] = false;
    forget_states(s, seat, false);
    forget_round_trip(s, seat);
    sync_authority_seat_joined(&s->authority, seat);
    s->joined[seat] = true;
}

void body_sync_seat_left(body_sync_t *s, uint8_t seat) {
    if (!s || !seat_ok(s, seat)) {
        return;
    }
    s->joined[seat] = false;
    s->writing[seat] = false;
    forget_states(s, seat, true);
    forget_round_trip(s, seat);
    sync_authority_seat_left(&s->authority, seat);
}

body_sync_result_t body_sync_bind(body_sync_t *s, body_sync_id_t id, const body_sync_bind_t *bind) {
    uint32_t slot = body_sync_id_slot(id);
    if (!bind || slot >= BODY_SYNC_BODIES_MAX || body_sync_id_generation(id) == 0 || !b3Body_IsValid(bind->body) ||
        b3Body_GetType(bind->body) == b3_staticBody || bind->body_class > BODY_SYNC_CLASS_CHARACTER || bind->authority >= s->config.seats ||
        (s->config.character_category && sync_masks_shapes(bind->body) > BODY_SYNC_SHAPES_MAX)) {
        return invalid(s, "bind: id, body, class, authority or shape count out of range");
    }
    body_sync_result_t r = sync_authority_bind(&s->authority, id, bind->authority, bind->number);
    if (r != BODY_SYNC_OK) {
        return r;
    }
    entry_t *e = &s->entries[slot];
    memset(e, 0, sizeof(*e));
    e->body = bind->body;
    e->body_class = bind->body_class;
    e->bound_type = b3Body_GetType(bind->body);
    e->bound_locks = b3Body_GetMotionLocks(bind->body);
    if (is_host(s) && bind->body_class == BODY_SYNC_CLASS_CHARACTER) {
        sync_authority_set_agent(&s->authority, id, true);
    }
    return BODY_SYNC_OK;
}

body_sync_result_t body_sync_unbind(body_sync_t *s, body_sync_id_t id) {
    entry_t *e = entry_for(s, id);
    if (e && b3Body_IsValid(e->body)) {
        sync_masks_restore(&e->masks, e->body); /* the caller gets its body back as it bound it */
    }
    return sync_authority_unbind(&s->authority, id);
}

b3BodyId body_sync_body(const body_sync_t *s, body_sync_id_t id) {
    const entry_t *e = entry_const(s, id);
    return e ? e->body : b3_nullBodyId;
}

bool body_sync_authority(const body_sync_t *s, body_sync_id_t id, body_sync_authority_t *out) {
    const sync_authority_body_t *b = sync_authority_body_const(&s->authority, id);
    if (!b) {
        return false;
    }
    sync_authority_describe(&s->authority, b, out);
    return true;
}

bool body_sync_contact_number(const body_sync_t *s, body_sync_id_t id, uint16_t *number) {
    const sync_authority_body_t *b = sync_authority_body_const(&s->authority, id);
    return b && sync_authority_contact_number(&s->authority, b, number);
}

static float length3(const float v[3]) { return sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); }

/* The claim's grace, ticks: the real seam's (sync_policy_evidence_ticks) with no playback delay,
   since a copy here shows the newest pose at once. A state and the claim behind it cross on
   packets of their own, each late by the path's jitter, and a lost one is resent: four mean
   deviations of the round trip cover what the jitter spreads, as TCP's retransmit timer does. */
static uint32_t claim_grace(const body_sync_t *s, uint8_t seat) {
    float rtt = seat < s->config.seats ? s->round_trip[seat] + 4.0f * s->round_trip_dev[seat] : 0.0f;
    return (uint32_t)ceilf(sync_policy_evidence_ticks(rtt, 0.0f, s->config.tick_hz));
}

/* The agent is measured where the host's last taken state of that ownership put it
   (body_sync_real.c says why not its declared velocity). */
body_sync_contact_verdict_t body_sync_check_contact(const body_sync_t *s, uint8_t seat, body_sync_id_t agent, uint16_t number,
                                                    body_sync_id_t target, uint32_t contact_tick, body_sync_contact_t *out) {
    body_sync_contact_t scratch;
    out = out ? out : &scratch;
    *out = (body_sync_contact_t){.gap = INFINITY};
    const sync_authority_ended_t *ended;
    uint32_t grace = claim_grace(s, seat);
    body_sync_contact_verdict_t verdict = sync_authority_check_contact(&s->authority, seat, agent, number, grace, &ended);
    const entry_t *g = entry_const(s, agent), *t = entry_const(s, target);
    if (verdict == BODY_SYNC_CONTACT_STALE || agent == target || !g || !t || !b3Body_IsValid(g->body) || !b3Body_IsValid(t->body)) {
        return BODY_SYNC_CONTACT_STALE;
    }
    out->ended = ended != NULL;
    out->ended_age_ticks = ended ? s->authority.now - ended->tick : 0u;
    b3Vec3 vb = b3Body_GetLinearVelocity(t->body), at = b3Body_GetPosition(g->body);
    b3AABB box = b3Body_ComputeAABB(g->body), tb = b3Body_ComputeAABB(t->body);
    sync_contact_scene_t scene = {.target_lo = {tb.lowerBound.x, tb.lowerBound.y, tb.lowerBound.z},
                                  .target_hi = {tb.upperBound.x, tb.upperBound.y, tb.upperBound.z},
                                  .agent_lo = {box.lowerBound.x - at.x, box.lowerBound.y - at.y, box.lowerBound.z - at.z},
                                  .agent_hi = {box.upperBound.x - at.x, box.upperBound.y - at.y, box.upperBound.z - at.z},
                                  .target_speed = length3((const float[3]){vb.x, vb.y, vb.z}),
                                  .target_velocity = {vb.x, vb.y, vb.z},
                                  .rewind = true,
                                  .tick_hz = s->config.tick_hz,
                                  .max_speed = s->limits.max_speed,
                                  .contact_tick = contact_tick,
                                  .window_ticks = grace > 2u ? grace : 2u}; /* at least the tick a state crosses in and the one the claim is judged in */
    sync_authority_contact_measure(&s->authority, agent, seat, ended, &scene, &out->gap, &out->reach, &out->approach_max);
    out->slop = SYNC_POLICY_TAKE_SLOP_M;
    return verdict;
}

bool body_sync_is_local(const body_sync_t *s, body_sync_id_t id) {
    const sync_authority_body_t *b = sync_authority_body_const(&s->authority, id);
    return b && sync_authority_local(&s->authority, b);
}

body_sync_result_t body_sync_claim(body_sync_t *s, body_sync_id_t id, body_sync_hold_t hold) { return sync_authority_claim(&s->authority, id, hold); }
body_sync_result_t body_sync_release(body_sync_t *s, body_sync_id_t id) { return sync_authority_release(&s->authority, id); }
bool body_sync_poll_claim(body_sync_t *s, body_sync_claim_event_t *out) { return sync_authority_poll(&s->authority, out); }
static uint8_t locks_bits(b3MotionLocks l);

/* A grant or a forced move carries the body as it is now, not as the last collect read it. */
static void note_live(body_sync_t *s, body_sync_id_t id) {
    sync_authority_body_t *a = sync_authority_body(&s->authority, id);
    b3BodyId body = s->entries[body_sync_id_slot(id)].body;
    if (a && b3Body_IsValid(body)) {
        sync_authority_note_motion(&s->authority, a, b3Body_GetType(body) == b3_kinematicBody, locks_bits(b3Body_GetMotionLocks(body)));
    }
}

/* Host: a copy moved from guest `from` to another guest: what it received is the old owner's. */
static void host_handover(body_sync_t *s, uint32_t slot, uint8_t from) {
    const sync_authority_body_t *a = auth_of(s, slot);
    if (is_host(s) && from != BODY_SYNC_SEAT_HOST && a->seat != BODY_SYNC_SEAT_HOST && a->seat != from) {
        s->entries[slot].in_old = true;
    }
}

body_sync_result_t body_sync_answer(body_sync_t *s, const body_sync_claim_event_t *claim, bool granted) {
    if (!claim) {
        return sync_authority_answer(&s->authority, claim, granted);
    }
    note_live(s, claim->id);
    const sync_authority_body_t *a = sync_authority_body_const(&s->authority, claim->id);
    uint8_t from = a ? a->seat : BODY_SYNC_SEAT_HOST;
    body_sync_result_t r = sync_authority_answer(&s->authority, claim, granted);
    if (r == BODY_SYNC_OK && a) {
        host_handover(s, body_sync_id_slot(claim->id), from);
    }
    return r;
}
static void take_over(body_sync_t *s, uint32_t slot);
body_sync_result_t body_sync_assign(body_sync_t *s, body_sync_id_t id, uint8_t seat, body_sync_hold_t hold) {
    note_live(s, id);
    const sync_authority_body_t *was = sync_authority_body_const(&s->authority, id);
    uint8_t from = was ? was->seat : BODY_SYNC_SEAT_HOST;
    body_sync_result_t r = sync_authority_assign(&s->authority, id, seat, hold);
    if (r == BODY_SYNC_OK && was) {
        host_handover(s, body_sync_id_slot(id), from);
    }
    /* A move to this side takes over now, so what the caller does next (a recall puts the body
       home) is not undone by the next apply's take-over. */
    uint32_t slot = body_sync_id_slot(id);
    entry_t *e = &s->entries[slot];
    if (r != BODY_SYNC_OK || !body_sync_is_local(s, id) || !b3Body_IsValid(e->body)) {
        return r;
    }
    if (e->copy_mode) {
        take_over(s, slot);
    } else {
        /* Never a copy here (a grant this tick, before the apply that would make it one): it goes
           on as this side simulates it, not in the motion and velocities of an older owner's
           states. */
        sync_authority_body_t *a = auth_of(s, slot);
        a->resume = false;
        a->take_motion = false;
        sync_authority_applied(a, b3Body_GetType(e->body) == b3_kinematicBody, locks_bits(b3Body_GetMotionLocks(e->body)));
    }
    return r;
}
body_sync_result_t body_sync_teleported(body_sync_t *s, body_sync_id_t id) {
    const sync_authority_body_t *a = sync_authority_body_const(&s->authority, id);
    if (!a) {
        return BODY_SYNC_UNKNOWN;
    }
    /* No render offset here: a teleport is drawn where physics has it already. */
    return sync_authority_local(&s->authority, a) ? BODY_SYNC_OK : invalid(s, "teleported: a copy's pose is its owner's");
}
body_sync_result_t body_sync_set_holder(body_sync_t *s, body_sync_id_t id, body_sync_id_t holder) {
    return sync_authority_set_holder(&s->authority, id, holder);
}
body_sync_result_t body_sync_set_flags(body_sync_t *s, body_sync_id_t id, uint8_t flags) { return sync_authority_set_flags(&s->authority, id, flags); }
uint8_t body_sync_flags(const body_sync_t *s, body_sync_id_t id) { return sync_authority_flags(&s->authority, id); }

body_sync_result_t body_sync_set_data(body_sync_t *s, body_sync_id_t id, const void *data, uint32_t size) {
    return sync_authority_set_data(&s->authority, id, data, size);
}

uint32_t body_sync_data(const body_sync_t *s, body_sync_id_t id, void *out, uint32_t capacity) {
    return sync_authority_data(&s->authority, id, out, capacity);
}

void body_sync_set_peer_tick(body_sync_t *s, uint8_t seat, double tick) {
    (void)s;
    (void)seat;
    (void)tick;
}

static body_sync_result_t format(body_sync_t *s, uint8_t seat, const char *what) {
    snprintf(s->error, sizeof(s->error), "seat %u: %s", (unsigned)seat, what);
    s->traffic[seat].refused++;
    return BODY_SYNC_FORMAT;
}

static void store_in(body_sync_t *s, uint32_t slot, uint8_t from, uint32_t tick, const stub_body_state_t *b, body_sync_traffic_t *t) {
    entry_t *e = &s->entries[slot];
    /* A new sender's clock is not comparable with the old one's; within one sender, older loses. */
    if (e->has_in && e->in_ordered && e->in_from == from && !tick_newer(tick, e->in_tick)) {
        t->stale++;
        return;
    }
    e->in = *b;
    e->has_in = true;
    e->in_old = false;
    e->in_fresh = true;
    e->in_from = from;
    e->in_tick = tick;
    e->in_ordered = true;
    if (is_host(s)) {
        sync_authority_received_pose(&s->authority, auth_of(s, slot), from, tick, 1u, s->limits.max_speed / (float)s->config.tick_hz, b->position);
    }
    sync_authority_received(auth_of(s, slot), b->hold, b->holder, b->flags);
    sync_authority_received_data(auth_of(s, slot), b->data_size, b->data);
    sync_authority_received_motion(auth_of(s, slot), true, b->kinematic, b->locks);
}

body_sync_result_t body_sync_receive_state(body_sync_t *s, uint8_t seat, uint32_t tick, const uint8_t *data, uint32_t size) {
    if (!seat_ok(s, seat) || !s->joined[seat] || (!is_host(s) && seat != BODY_SYNC_SEAT_HOST)) {
        return invalid(s, "receive_state: not a seat this side hears");
    }
    uint32_t count = 0;
    char why[96];
    if (!stub_wire_read_state(data, size, s->config.seats, &s->limits, s->scratch, BODY_SYNC_BODIES_MAX, &count, why, sizeof(why))) {
        return format(s, seat, why);
    }
    body_sync_traffic_t *t = &s->traffic[seat];
    t->received_bytes[BODY_SYNC_TRAFFIC_HEADER] += STUB_HEADER_BYTES;
    t->received_bytes[BODY_SYNC_TRAFFIC_COPY] += (uint64_t)count * STUB_COPY_BYTES;
    t->received_bytes[BODY_SYNC_TRAFFIC_SIM] += (uint64_t)count * STUB_SIM_BYTES;
    t->received_bytes[BODY_SYNC_TRAFFIC_AUTHORITY] += (uint64_t)count * STUB_AUTHORITY_BYTES;
    t->received_bytes[BODY_SYNC_TRAFFIC_HELD] += (uint64_t)count * STUB_HELD_BYTES;
    for (uint32_t i = 0; i < count; i++) {
        const stub_body_state_t *b = &s->scratch[i];
        sync_authority_body_t *a = sync_authority_body(&s->authority, b->id);
        if (!a) {
            t->stale++;
            sync_authority_old_owner_drop(&s->authority, seat, body_sync_id_slot(b->id), body_sync_id_generation(b->id), 16u);
            continue;
        }
        if (is_host(s)) {
            if (a->seat != seat || b->number != a->number) {
                t->rejected++;
                sync_authority_old_owner_drop(&s->authority, seat, body_sync_id_slot(b->id), a->generation, 16u);
                continue;
            }
        } else {
            sync_authority_from_state(&s->authority, a, b->seat, b->number, b->hold);
            if (sync_authority_local(&s->authority, a)) {
                continue;
            }
        }
        store_in(s, body_sync_id_slot(b->id), seat, tick, b, t);
    }
    return BODY_SYNC_OK;
}

body_sync_result_t body_sync_receive_record(body_sync_t *s, uint8_t seat, uint32_t tick, const uint8_t *data, uint32_t size) {
    if (!seat_ok(s, seat) || !s->joined[seat] || (!is_host(s) && seat != BODY_SYNC_SEAT_HOST)) {
        return invalid(s, "receive_record: not a seat this side hears");
    }
    char why[96];
    body_sync_traffic_t *t = &s->traffic[seat];
    if (sync_authority_receive(&s->authority, seat, tick, data, size, &t->stale, why, sizeof(why)) == BODY_SYNC_FORMAT) {
        return format(s, seat, why);
    }
    t->received_bytes[BODY_SYNC_TRAFFIC_AUTHORITY] += size;
    return BODY_SYNC_OK;
}

static b3Quat quat_of(const float q[4]) { return (b3Quat){{q[0], q[1], q[2]}, q[3]}; }

static uint8_t locks_bits(b3MotionLocks l) {
    return (uint8_t)(l.linearX | l.linearY << 1 | l.linearZ << 2 | l.angularX << 3 | l.angularY << 4 | l.angularZ << 5);
}

static b3MotionLocks locks_of(uint8_t bits) {
    return (b3MotionLocks){.linearX = (bits & 1u) != 0, .linearY = (bits & 2u) != 0, .linearZ = (bits & 4u) != 0,
                           .angularX = (bits & 8u) != 0, .angularY = (bits & 16u) != 0, .angularZ = (bits & 32u) != 0};
}

/* A body that became local continues from the newest state received: its motion, masks and
   velocities. */
static void take_over(body_sync_t *s, uint32_t slot) {
    sync_authority_body_t *a = auth_of(s, slot);
    entry_t *e = &s->entries[slot];
    bool was_copy = e->copy_mode;
    e->copy_mode = false;
    a->resume = false;
    bool has_in = e->has_in && !e->in_old;
    e->in_old = false;
    if (has_in || was_copy || a->take_motion) {
        bool kinematic;
        uint8_t locks;
        sync_authority_resume_motion(a, has_in, e->in.kinematic, e->in.locks, e->bound_type == b3_kinematicBody, locks_bits(e->bound_locks),
                                     &kinematic, &locks);
        b3Body_SetType(e->body, kinematic ? b3_kinematicBody : b3_dynamicBody);
        b3Body_SetMotionLocks(e->body, locks_of(locks));
        sync_authority_applied(a, kinematic, locks);
    }
    sync_masks_restore(&e->masks, e->body);
    if (has_in) {
        b3Body_SetLinearVelocity(e->body, (b3Vec3){e->in.velocity[0], e->in.velocity[1], e->in.velocity[2]});
        b3Body_SetAngularVelocity(e->body, (b3Vec3){e->in.angular[0], e->in.angular[1], e->in.angular[2]});
    }
}

void body_sync_apply(body_sync_t *s, uint32_t tick) {
    s->now = tick;
    s->authority.now = tick;
    for (uint32_t slot = 0; slot < BODY_SYNC_BODIES_MAX; slot++) {
        sync_authority_body_t *a = auth_of(s, slot);
        entry_t *e = &s->entries[slot];
        if (!a->bound || !b3Body_IsValid(e->body)) {
            continue;
        }
        if (sync_authority_local(&s->authority, a)) {
            if (e->copy_mode || a->resume) {
                take_over(s, slot);
            }
            continue;
        }
        if (!e->copy_mode) {
            e->copy_mode = true;
            e->placed = false;
            b3Body_SetType(e->body, b3_kinematicBody);
            /* A lock would drop the angular velocity that turns a copy to its owner's rotation
               (a character's facing); the owner's locks travel in its state instead. */
            b3Body_SetMotionLocks(e->body, (b3MotionLocks){0});
            if (e->body_class == BODY_SYNC_CLASS_LIGHT) {
                sync_masks_clear(&e->masks, e->body, s->config.character_category);
            }
        }
        if (e->has_in && e->in_fresh) {
            e->in_fresh = false;
            const stub_body_state_t *in = &e->in;
            b3Vec3 target = {in->position[0], in->position[1], in->position[2]};
            b3Vec3 now = b3Body_GetPosition(e->body);
            b3Vec3 d = {target.x - now.x, target.y - now.y, target.z - now.z};
            float distance = sqrtf(d.x * d.x + d.y * d.y + d.z * d.z);
            b3Quat q = b3Body_GetRotation(e->body);
            bool same = distance == 0.0f && q.v.x == in->rotation[0] && q.v.y == in->rotation[1] && q.v.z == in->rotation[2] &&
                        q.s == in->rotation[3];
            /* A body asleep at its owner is placed on that exact pose, so worlds at rest agree to the bit. */
            if (!e->placed || distance > SNAP_DISTANCE || (in->asleep && !same)) {
                b3Body_SetTransform(e->body, target, quat_of(in->rotation));
                b3Body_SetLinearVelocity(e->body, (b3Vec3){0.0f, 0.0f, 0.0f});
                b3Body_SetAngularVelocity(e->body, (b3Vec3){0.0f, 0.0f, 0.0f});
                e->placed = true;
            } else if (!in->asleep) {
                b3Body_SetTargetTransform(e->body, (b3WorldTransform){target, quat_of(in->rotation)}, s->dt, true);
            } else if (b3Body_IsAwake(e->body)) {
                b3Body_SetLinearVelocity(e->body, (b3Vec3){0.0f, 0.0f, 0.0f});
                b3Body_SetAngularVelocity(e->body, (b3Vec3){0.0f, 0.0f, 0.0f});
            }
        } else if (b3Body_IsAwake(e->body)) {
            /* No news this tick: hold still rather than coast on the last target's velocity. */
            b3Body_SetLinearVelocity(e->body, (b3Vec3){0.0f, 0.0f, 0.0f});
            b3Body_SetAngularVelocity(e->body, (b3Vec3){0.0f, 0.0f, 0.0f});
        }
    }
}

static bool touched_moving(const b3ContactEvents *events, b3BodyId body, b3BodyId ignore) {
    for (int i = 0; i < events->beginCount; i++) {
        b3ShapeId a = events->beginEvents[i].shapeIdA, b = events->beginEvents[i].shapeIdB;
        if (!b3Shape_IsValid(a) || !b3Shape_IsValid(b)) {
            continue;
        }
        b3BodyId ba = b3Shape_GetBody(a), bb = b3Shape_GetBody(b);
        b3BodyId other;
        if (B3_ID_EQUALS(ba, body)) {
            other = bb;
        } else if (B3_ID_EQUALS(bb, body)) {
            other = ba;
        } else {
            continue;
        }
        if (b3Body_GetType(other) != b3_staticBody && !B3_ID_EQUALS(other, ignore)) {
            return true;
        }
    }
    return false;
}

void body_sync_collect(body_sync_t *s, uint32_t tick) {
    s->now = tick;
    s->authority.now = tick;
    b3ContactEvents events = b3World_GetContactEvents(s->config.world);
    for (uint32_t slot = 0; slot < BODY_SYNC_BODIES_MAX; slot++) {
        sync_authority_body_t *a = auth_of(s, slot);
        entry_t *e = &s->entries[slot];
        if (!a->bound || !sync_authority_local(&s->authority, a) || !b3Body_IsValid(e->body)) {
            continue;
        }
        stub_body_state_t *o = &e->out;
        b3Vec3 p = b3Body_GetPosition(e->body);
        b3Quat q = b3Body_GetRotation(e->body);
        b3Vec3 v = b3Body_GetLinearVelocity(e->body);
        b3Vec3 w = b3Body_GetAngularVelocity(e->body);
        o->id = body_sync_id_make(slot, a->generation);
        o->position[0] = p.x;
        o->position[1] = p.y;
        o->position[2] = p.z;
        o->rotation[0] = q.v.x;
        o->rotation[1] = q.v.y;
        o->rotation[2] = q.v.z;
        o->rotation[3] = q.s;
        o->velocity[0] = v.x;
        o->velocity[1] = v.y;
        o->velocity[2] = v.z;
        o->angular[0] = w.x;
        o->angular[1] = w.y;
        o->angular[2] = w.z;
        o->kinematic = b3Body_GetType(e->body) == b3_kinematicBody;
        o->asleep = !b3Body_IsAwake(e->body);
        o->locks = locks_bits(b3Body_GetMotionLocks(e->body));
        sync_authority_note_motion(&s->authority, a, o->kinematic, o->locks);
        o->data_size = a->data_size;
        memcpy(o->data, a->data, sizeof(o->data));
        o->flags = a->flags;
        o->holder = a->holder;
        o->hold = a->hold;
        o->seat = a->seat;
        o->number = sync_authority_declared_number(&s->authority, a);
        e->has_out = true;
        if (a->hold == BODY_SYNC_HOLD_UNTIL_SETTLED) {
            b3BodyId ignore = b3_nullBodyId;
            const entry_t *h = entry_const(s, a->last_holder);
            if (h) {
                ignore = h->body;
            }
            if (o->asleep || touched_moving(&events, e->body, ignore)) {
                uint8_t hold = sync_authority_settled(&s->authority, o->id);
                if (is_host(s)) {
                    o->hold = hold;
                }
            }
        }
    }
}

static bool eligible(const body_sync_t *s, uint32_t slot, uint8_t seat, stub_body_state_t *out) {
    const sync_authority_body_t *a = &s->authority.bodies[slot];
    const entry_t *e = &s->entries[slot];
    if (!a->bound) {
        return false;
    }
    if (!is_host(s)) {
        if (!local_slot(s, slot) || !e->has_out) {
            return false;
        }
        *out = e->out;
        return true;
    }
    if (a->seat == seat) {
        return false;
    }
    if (local_slot(s, slot)) {
        if (!e->has_out) {
            return false;
        }
        *out = e->out;
        return true;
    }
    if (!e->has_in) {
        return false;
    }
    *out = e->in;
    out->seat = a->seat;
    out->number = a->number;
    return true;
}

bool body_sync_write_state(body_sync_t *s, uint8_t seat, uint8_t *buffer, uint32_t capacity, uint32_t *size) {
    *size = 0;
    if (!seat_ok(s, seat) || !s->joined[seat] || (!is_host(s) && seat != BODY_SYNC_SEAT_HOST) || capacity < 64 || !buffer) {
        return false;
    }
    uint32_t fit = stub_wire_bodies_fit(capacity);
    fit = fit > 0xFFFF ? 0xFFFF : fit;
    if (fit == 0) {
        return false;
    }
    if (!s->writing[seat]) {
        s->writing[seat] = true;
        s->cursor[seat] = 0;
    }
    uint32_t n = 0;
    uint32_t slot = s->cursor[seat];
    stub_body_state_t state;
    for (; slot < BODY_SYNC_BODIES_MAX && n < fit; slot++) {
        if (eligible(s, slot, seat, &state) && stub_wire_limit(&s->limits, &state)) {
            stub_wire_put_body(buffer + STUB_HEADER_BYTES + n * STUB_BODY_BYTES, &state);
            n++;
        }
    }
    s->cursor[seat] = slot;
    if (n == 0) {
        s->writing[seat] = false;
        return false;
    }
    stub_wire_put_header(buffer, (uint16_t)n);
    *size = STUB_HEADER_BYTES + n * STUB_BODY_BYTES;
    body_sync_traffic_t *t = &s->traffic[seat];
    t->sent_bytes[BODY_SYNC_TRAFFIC_HEADER] += STUB_HEADER_BYTES;
    t->sent_bytes[BODY_SYNC_TRAFFIC_COPY] += (uint64_t)n * STUB_COPY_BYTES;
    t->sent_bytes[BODY_SYNC_TRAFFIC_SIM] += (uint64_t)n * STUB_SIM_BYTES;
    t->sent_bytes[BODY_SYNC_TRAFFIC_AUTHORITY] += (uint64_t)n * STUB_AUTHORITY_BYTES;
    t->sent_bytes[BODY_SYNC_TRAFFIC_HELD] += (uint64_t)n * STUB_HELD_BYTES;
    return true;
}

void body_sync_state_sent(body_sync_t *s, uint8_t seat, uint16_t number) {
    if (seat < s->config.seats) {
        s->traffic[seat].packets_sent++;
        s->sent[seat][s->sent_next[seat]] = (sent_packet_t){.used = true, .number = number, .tick = s->now};
        s->sent_next[seat] = (s->sent_next[seat] + 1u) % SENT_WINDOW;
    }
}

uint32_t body_sync_unresolved(const body_sync_t *s, uint8_t seat, uint16_t *numbers, uint32_t max) {
    uint32_t n = 0;
    for (uint32_t i = 0; seat < s->config.seats && numbers && i < SENT_WINDOW && n < max; i++) {
        if (s->sent[seat][i].used) {
            numbers[n++] = s->sent[seat][i].number;
        }
    }
    return n;
}

void body_sync_resolve(body_sync_t *s, uint8_t seat, uint16_t number, bool arrived) {
    for (uint32_t i = 0; seat < s->config.seats && i < SENT_WINDOW; i++) {
        sent_packet_t *p = &s->sent[seat][i];
        if (!p->used || p->number != number) {
            continue;
        }
        p->used = false;
        if (arrived) {
            float sample = (float)(int32_t)(s->now - p->tick), *rtt = &s->round_trip[seat], *dev = &s->round_trip_dev[seat];
            if (*rtt <= 0.0f) {
                *rtt = sample;
                *dev = sample / 2.0f;
            } else {
                *dev += (fabsf(sample - *rtt) - *dev) / 4.0f;
                *rtt += (sample - *rtt) / 8.0f;
            }
        }
        return;
    }
}

bool body_sync_peek_record(const body_sync_t *s, uint8_t seat, uint8_t *buffer, uint32_t capacity, uint32_t *size) {
    *size = 0;
    if (seat >= s->config.seats || !buffer) {
        return false;
    }
    return sync_authority_peek(&s->authority, seat, buffer, capacity, size);
}

void body_sync_pop_record(body_sync_t *s, uint8_t seat) {
    if (seat >= s->config.seats) {
        return;
    }
    uint32_t size = sync_authority_pop(&s->authority, seat);
    if (size) {
        s->traffic[seat].sent_bytes[BODY_SYNC_TRAFFIC_AUTHORITY] += size;
        s->traffic[seat].records_sent++;
    }
}

bool body_sync_playback(const body_sync_t *s, body_sync_id_t id, double *owner_tick) {
    const entry_t *e = entry_const(s, id);
    if (!e) {
        return false;
    }
    if (local_slot(s, body_sync_id_slot(id))) {
        *owner_tick = (double)s->now;
        return true;
    }
    if (!e->has_in) {
        return false;
    }
    *owner_tick = (double)e->in_tick;
    return true;
}

bool body_sync_is_predicted(const body_sync_t *s, body_sync_id_t id) {
    (void)s;
    (void)id;
    return false;
}

bool body_sync_is_pull_paused(const body_sync_t *s, body_sync_id_t id) {
    (void)s;
    (void)id;
    return false;
}

bool body_sync_lost_take(const body_sync_t *s, body_sync_id_t id, uint32_t *count, float *hand_back_m) {
    (void)s;
    (void)id;
    *count = 0;
    *hand_back_m = 0.0f;
    return false;
}

float body_sync_round_trip(const body_sync_t *s, uint8_t seat) { return seat < s->config.seats ? s->round_trip[seat] : 0.0f; }

bool body_sync_is_layer_copy(const body_sync_t *s, body_sync_id_t id) {
    (void)s;
    (void)id;
    return false;
}

bool body_sync_render_offset(const body_sync_t *s, body_sync_id_t id, float position[3], float rotation[4]) {
    if (!entry_const(s, id)) {
        return false;
    }
    position[0] = position[1] = position[2] = 0.0f;
    rotation[0] = rotation[1] = rotation[2] = 0.0f;
    rotation[3] = 1.0f;
    return true;
}

uint32_t body_sync_old_owner_drops(const body_sync_t *s, body_sync_id_t id, uint8_t seat) {
    return s ? sync_authority_old_owner_drops(&s->authority, id, seat) : 0u;
}

bool body_sync_traffic(const body_sync_t *s, uint8_t seat, body_sync_traffic_t *out) {
    if (seat >= s->config.seats) {
        return false;
    }
    *out = s->traffic[seat];
    return true;
}
