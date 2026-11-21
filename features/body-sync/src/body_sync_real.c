#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "features/body_sync/body_sync.h"
#include "sync_authority.h"
#include "sync_codec.h"
#include "sync_receiver.h"
#include "sync_sender.h"

/* The real seam: the Box3D shell around the parts. It reads bodies into the sender's inputs,
   decodes packets into the receiver, and moves bodies as the receiver and authority say. Who
   gets which record, whose poses count, when a hold settles, which motion a take continues
   with and what a holder names are the parts' decisions; what is left here is reading and
   writing Box3D and the contact test. */

typedef struct entry {
    b3BodyId body;
    body_sync_class_t body_class;
    b3BodyType bound_type;
    b3MotionLocks bound_locks; /* restored when a copy turns local with no state received */
    bool copy_mode;
    bool placed;
} entry_t;

struct body_sync {
    body_sync_config_t config;
    float dt;
    sync_codec_t codec;
    sync_authority_t authority;
    sync_sender_t sender;
    sync_receiver_t receiver;
    entry_t entries[BODY_SYNC_BODIES_MAX];
    bool joined[BODY_SYNC_SEATS_MAX];
    body_sync_traffic_t traffic[BODY_SYNC_SEATS_MAX];
    sync_body_input_t inputs[BODY_SYNC_BODIES_MAX];
    uint8_t send_to[BODY_SYNC_BODIES_MAX];
    sync_record_t scratch[BODY_SYNC_BODIES_MAX];
    char error[128];
};

const char *body_sync_implementation(void) { return "real 0.5.0"; }

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
static bool seat_ok(const body_sync_t *s, uint8_t seat) { return seat < s->config.seats && seat != s->config.seat; }

body_sync_t *body_sync_create(const body_sync_config_t *config) {
    if (!config || !b3World_IsValid(config->world) || config->seats < 1 || config->seats > BODY_SYNC_SEATS_MAX ||
        config->seat >= config->seats || config->tick_hz < 1 || config->tick_hz > 1000) {
        return NULL;
    }
    sync_limits_t limits;
    memcpy(limits.bounds_min, config->bounds_min, sizeof(limits.bounds_min));
    memcpy(limits.bounds_max, config->bounds_max, sizeof(limits.bounds_max));
    limits.max_speed = config->max_speed;
    limits.max_spin = config->max_spin;
    body_sync_t *s = calloc(1, sizeof(*s));
    if (!s) {
        return NULL;
    }
    if (!sync_codec_init(&s->codec, &limits) || !sync_sender_budget_ok(&s->codec, (uint8_t)config->seats, config->state_budget)) {
        free(s);
        return NULL;
    }
    s->config = *config;
    s->dt = 1.0f / (float)config->tick_hz;
    sync_authority_init(&s->authority, config->seat, (uint8_t)config->seats, s->error, sizeof(s->error));
    sync_sender_init(&s->sender, &s->codec, (uint8_t)config->seats, config->state_budget);
    sync_receiver_init(&s->receiver, config->tick_hz, config->max_speed);
    return s;
}

void body_sync_destroy(body_sync_t *sync) { free(sync); }

void body_sync_seat_joined(body_sync_t *s, uint8_t seat) {
    if (!s || !seat_ok(s, seat) || (!is_host(s) && seat != BODY_SYNC_SEAT_HOST)) {
        return;
    }
    sync_authority_seat_joined(&s->authority, seat);
    sync_sender_seat(&s->sender, seat, true);
    sync_receiver_forget_seat(&s->receiver, seat, false);
    s->joined[seat] = true;
}

void body_sync_seat_left(body_sync_t *s, uint8_t seat) {
    if (!s || !seat_ok(s, seat)) {
        return;
    }
    s->joined[seat] = false;
    sync_sender_seat(&s->sender, seat, false);
    sync_receiver_forget_seat(&s->receiver, seat, true);
    sync_authority_seat_left(&s->authority, seat);
}

static void forget_slot(body_sync_t *s, uint32_t slot) {
    sync_sender_forget(&s->sender, (uint16_t)slot);
    sync_receiver_forget(&s->receiver, (uint16_t)slot);
}

body_sync_result_t body_sync_bind(body_sync_t *s, body_sync_id_t id, const body_sync_bind_t *bind) {
    uint32_t slot = body_sync_id_slot(id);
    if (!bind || slot >= BODY_SYNC_BODIES_MAX || body_sync_id_generation(id) == 0 || !b3Body_IsValid(bind->body) ||
        b3Body_GetType(bind->body) == b3_staticBody || bind->body_class > BODY_SYNC_CLASS_CHARACTER || bind->authority >= s->config.seats) {
        return invalid(s, "bind: id, body, class or authority out of range");
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
    forget_slot(s, slot);
    return BODY_SYNC_OK;
}

body_sync_result_t body_sync_unbind(body_sync_t *s, body_sync_id_t id) {
    body_sync_result_t r = sync_authority_unbind(&s->authority, id);
    if (r == BODY_SYNC_OK) {
        forget_slot(s, body_sync_id_slot(id));
    }
    return r;
}

b3BodyId body_sync_body(const body_sync_t *s, body_sync_id_t id) {
    return sync_authority_body_const(&s->authority, id) ? s->entries[body_sync_id_slot(id)].body : b3_nullBodyId;
}

bool body_sync_authority(const body_sync_t *s, body_sync_id_t id, body_sync_authority_t *out) {
    const sync_authority_body_t *b = sync_authority_body_const(&s->authority, id);
    if (!b) {
        return false;
    }
    sync_authority_describe(&s->authority, b, out);
    return true;
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

body_sync_result_t body_sync_answer(body_sync_t *s, const body_sync_claim_event_t *claim, bool granted) {
    if (claim) {
        note_live(s, claim->id);
    }
    return sync_authority_answer(&s->authority, claim, granted);
}
body_sync_result_t body_sync_assign(body_sync_t *s, body_sync_id_t id, uint8_t seat, body_sync_hold_t hold) {
    note_live(s, id);
    return sync_authority_assign(&s->authority, id, seat, hold);
}
body_sync_result_t body_sync_set_holder(body_sync_t *s, body_sync_id_t id, body_sync_id_t holder) {
    return sync_authority_set_holder(&s->authority, id, holder);
}
body_sync_result_t body_sync_set_flags(body_sync_t *s, body_sync_id_t id, uint8_t flags) { return sync_authority_set_flags(&s->authority, id, flags); }
uint8_t body_sync_flags(const body_sync_t *s, body_sync_id_t id) { return sync_authority_flags(&s->authority, id); }

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

body_sync_result_t body_sync_receive_state(body_sync_t *s, uint8_t seat, uint32_t tick, const uint8_t *data, uint32_t size) {
    if (!seat_ok(s, seat) || !s->joined[seat] || (!is_host(s) && seat != BODY_SYNC_SEAT_HOST)) {
        return invalid(s, "receive_state: not a seat this side hears");
    }
    uint32_t count = 0;
    char why[96];
    if (!sync_packet_read(&s->codec, (uint8_t)s->config.seats, data, size, s->scratch, BODY_SYNC_BODIES_MAX, &count, why, sizeof(why))) {
        return format(s, seat, why);
    }
    body_sync_traffic_t *t = &s->traffic[seat];
    t->received_bytes[BODY_SYNC_TRAFFIC_HEADER] += 1;
    for (uint32_t i = 0; i < count; i++) {
        const sync_record_t *r = &s->scratch[i];
        t->received_bytes[r->sim ? BODY_SYNC_TRAFFIC_SIM : BODY_SYNC_TRAFFIC_COPY] += sync_codec_record_bits(&s->codec, r) / 8u;
        body_sync_id_t id = sync_authority_holder_id(&s->authority, r->slot, r->generation_low, SYNC_GEN_BITS);
        sync_authority_body_t *a = sync_authority_body(&s->authority, id);
        if (!a) {
            t->stale++;
            continue;
        }
        if (is_host(s)) {
            sync_intake_t intake = sync_receiver_host_intake(&s->receiver, seat, tick, r, a->seat, a->number);
            if (intake != SYNC_INTAKE_TAKE) {
                t->stale += intake == SYNC_INTAKE_STALE ? 1u : 0u;
                t->rejected += intake == SYNC_INTAKE_REJECTED ? 1u : 0u;
                continue;
            }
        } else {
            if (r->rare.groups & SYNC_RARE_AUTHORITY) {
                sync_authority_from_state(&s->authority, a, r->rare.seat, r->rare.number, r->rare.hold);
            }
            if (sync_authority_local(&s->authority, a)) {
                continue;
            }
        }
        if (!sync_receiver_store(&s->receiver, seat, tick, r)) {
            t->stale++;
            continue;
        }
        const sync_received_t *in = &s->receiver.bodies[r->slot];
        body_sync_authority_t now;
        sync_authority_describe(&s->authority, a, &now);
        sync_authority_received(a, (in->known & SYNC_RARE_AUTHORITY) ? in->rare.hold : (uint8_t)now.hold,
                                (in->known & SYNC_RARE_HOLDER)
                                    ? sync_authority_holder_id(&s->authority, in->rare.holder_slot, in->rare.holder_generation_low, SYNC_GEN_BITS)
                                    : BODY_SYNC_ID_NONE,
                                (in->known & SYNC_RARE_FLAGS) ? in->rare.flags : a->flags);
        sync_authority_received_motion(a, (in->known & SYNC_RARE_MOTION) != 0, in->rare.kinematic, in->rare.locks);
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

static void stop(b3BodyId body) {
    b3Body_SetLinearVelocity(body, (b3Vec3){0.0f, 0.0f, 0.0f});
    b3Body_SetAngularVelocity(body, (b3Vec3){0.0f, 0.0f, 0.0f});
}

/* A body that turned local continues as authority says: the motion of the record that moved
   it, or of the state received, or of the bind; the velocities received. */
static void resume(body_sync_t *s, uint32_t slot, bool was_copy) {
    sync_authority_body_t *a = &s->authority.bodies[slot];
    entry_t *e = &s->entries[slot];
    const sync_received_t *in = &s->receiver.bodies[slot];
    e->copy_mode = false;
    a->resume = false;
    if (!in->has && !was_copy && !a->take_motion) {
        return;
    }
    bool kinematic;
    uint8_t locks;
    sync_authority_resume_motion(a, in->has && (in->known & SYNC_RARE_MOTION) != 0, in->rare.kinematic, in->rare.locks,
                                 e->bound_type == b3_kinematicBody, locks_bits(e->bound_locks), &kinematic, &locks);
    b3Body_SetType(e->body, kinematic ? b3_kinematicBody : b3_dynamicBody);
    b3Body_SetMotionLocks(e->body, locks_of(locks));
    sync_authority_applied(a, kinematic, locks);
    if (in->has) {
        b3Body_SetLinearVelocity(e->body, (b3Vec3){in->velocity[0], in->velocity[1], in->velocity[2]});
        b3Body_SetAngularVelocity(e->body, (b3Vec3){in->angular[0], in->angular[1], in->angular[2]});
    }
}

void body_sync_apply(body_sync_t *s, uint32_t tick) {
    (void)tick;
    for (uint32_t slot = 0; slot < BODY_SYNC_BODIES_MAX; slot++) {
        sync_authority_body_t *a = &s->authority.bodies[slot];
        entry_t *e = &s->entries[slot];
        if (!a->bound || !b3Body_IsValid(e->body)) {
            continue;
        }
        if (sync_authority_local(&s->authority, a)) {
            if (e->copy_mode || a->resume) {
                resume(s, slot, e->copy_mode);
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
        }
        b3Vec3 p = b3Body_GetPosition(e->body);
        b3Quat q = b3Body_GetRotation(e->body);
        float now_p[3] = {p.x, p.y, p.z}, now_q[4] = {q.v.x, q.v.y, q.v.z, q.s}, tp[3], tq[4];
        switch (sync_receiver_decide(&s->receiver, (uint16_t)slot, now_p, now_q, e->placed, tp, tq)) {
        case SYNC_COPY_PLACE:
            b3Body_SetTransform(e->body, (b3Vec3){tp[0], tp[1], tp[2]}, quat_of(tq));
            stop(e->body);
            e->placed = true;
            break;
        case SYNC_COPY_DRIVE:
            b3Body_SetTargetTransform(e->body, (b3WorldTransform){{tp[0], tp[1], tp[2]}, quat_of(tq)}, s->dt, true);
            break;
        case SYNC_COPY_HOLD:
            /* No news: hold still rather than coast on the last target's velocity. */
            if (b3Body_IsAwake(e->body)) {
                stop(e->body);
            }
            break;
        }
    }
}

/* A begin-touch of `body` with a body that is neither static nor `ignore`. */
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

static void holder_fields(sync_rare_t *rare, body_sync_id_t holder) {
    if (holder == BODY_SYNC_ID_NONE) {
        rare->holder_slot = BODY_SYNC_BODIES_MAX;
        rare->holder_generation_low = 0;
    } else {
        rare->holder_slot = (uint16_t)body_sync_id_slot(holder);
        rare->holder_generation_low = (uint8_t)(body_sync_id_generation(holder) & SYNC_GEN_MASK);
    }
}

/* A local body as the sender wants it: full precision, every rare group's value. */
static void read_local(body_sync_t *s, uint32_t slot, sync_body_input_t *in) {
    sync_authority_body_t *a = &s->authority.bodies[slot];
    entry_t *e = &s->entries[slot];
    b3Vec3 p = b3Body_GetPosition(e->body);
    b3Quat q = b3Body_GetRotation(e->body);
    b3Vec3 v = b3Body_GetLinearVelocity(e->body);
    b3Vec3 w = b3Body_GetAngularVelocity(e->body);
    bool kinematic = b3Body_GetType(e->body) == b3_kinematicBody;
    uint8_t locks = locks_bits(b3Body_GetMotionLocks(e->body));
    sync_authority_note_motion(&s->authority, a, kinematic, locks);
    sync_record_t *r = &in->state;
    memset(r, 0, sizeof(*r));
    r->slot = (uint16_t)slot;
    r->generation_low = (uint8_t)(a->generation & SYNC_GEN_MASK);
    r->position[0] = p.x;
    r->position[1] = p.y;
    r->position[2] = p.z;
    r->rotation[0] = q.v.x;
    r->rotation[1] = q.v.y;
    r->rotation[2] = q.v.z;
    r->rotation[3] = q.s;
    r->velocity[0] = v.x;
    r->velocity[1] = v.y;
    r->velocity[2] = v.z;
    r->angular[0] = w.x;
    r->angular[1] = w.y;
    r->angular[2] = w.z;
    r->rare.kinematic = kinematic;
    r->rare.locks = locks;
    r->rare.flags = a->flags;
    r->rare.seat = a->seat;
    r->rare.number = a->number;
    r->rare.hold = a->hold;
    holder_fields(&r->rare, a->holder);
    in->present = true;
    in->asleep = !b3Body_IsAwake(e->body);
    in->held = a->holder != BODY_SYNC_ID_NONE;
    in->flying = a->hold == BODY_SYNC_HOLD_UNTIL_SETTLED;
    in->character = e->body_class == BODY_SYNC_CLASS_CHARACTER;
}

/* Host: a guest's body relayed to the other guests from the state that guest sent. */
static void read_relayed(body_sync_t *s, uint32_t slot, sync_body_input_t *in) {
    const sync_authority_body_t *a = &s->authority.bodies[slot];
    const sync_received_t *got = &s->receiver.bodies[slot];
    in->state = got->newest;
    in->state.slot = (uint16_t)slot;
    in->state.generation_low = (uint8_t)(a->generation & SYNC_GEN_MASK);
    memcpy(in->state.velocity, got->velocity, sizeof(got->velocity));
    memcpy(in->state.angular, got->angular, sizeof(got->angular));
    in->state.rare = got->rare;
    body_sync_authority_t now;
    sync_authority_describe(&s->authority, a, &now);
    in->state.rare.seat = now.seat;
    in->state.rare.number = now.number;
    in->state.rare.hold = (uint8_t)now.hold;
    holder_fields(&in->state.rare, now.holder);
    in->state.rare.flags = (got->known & SYNC_RARE_FLAGS) ? got->rare.flags : 0;
    in->present = true;
    in->asleep = got->newest.at_rest;
    in->held = now.holder != BODY_SYNC_ID_NONE;
    in->flying = now.hold == BODY_SYNC_HOLD_UNTIL_SETTLED;
    in->character = s->entries[slot].body_class == BODY_SYNC_CLASS_CHARACTER;
}

void body_sync_collect(body_sync_t *s, uint32_t tick) {
    b3ContactEvents events = b3World_GetContactEvents(s->config.world);
    for (uint32_t slot = 0; slot < BODY_SYNC_BODIES_MAX; slot++) {
        sync_authority_body_t *a = &s->authority.bodies[slot];
        entry_t *e = &s->entries[slot];
        sync_body_input_t *in = &s->inputs[slot];
        in->present = false;
        s->send_to[slot] = 0;
        if (!a->bound || !b3Body_IsValid(e->body)) {
            continue;
        }
        if (sync_authority_local(&s->authority, a) && a->hold == BODY_SYNC_HOLD_UNTIL_SETTLED) {
            const sync_authority_body_t *h = sync_authority_body_const(&s->authority, a->last_holder);
            b3BodyId ignore = h ? s->entries[body_sync_id_slot(a->last_holder)].body : b3_nullBodyId;
            sync_authority_settle(&s->authority, body_sync_id_make(slot, a->generation), !b3Body_IsAwake(e->body),
                                  touched_moving(&events, e->body, ignore));
        }
        bool mine = sync_authority_local(&s->authority, a);
        bool have = s->receiver.bodies[slot].has;
        if (mine) {
            read_local(s, slot, in);
        } else if (is_host(s) && have) {
            read_relayed(s, slot, in);
        }
        s->send_to[slot] = in->present ? sync_authority_route(&s->authority, slot, have) : 0u;
    }
    sync_sender_tick(&s->sender, tick, s->inputs, s->send_to);
}

bool body_sync_write_state(body_sync_t *s, uint8_t seat, uint8_t *buffer, uint32_t capacity, uint32_t *size) {
    *size = 0;
    if (!seat_ok(s, seat) || !s->joined[seat] || (!is_host(s) && seat != BODY_SYNC_SEAT_HOST) || capacity < 64 || !buffer) {
        return false;
    }
    *size = sync_sender_write(&s->sender, seat, sync_authority_sends_sim(&s->authority), buffer, capacity);
    return *size > 0;
}

void body_sync_state_sent(body_sync_t *s, uint8_t seat, uint16_t number) {
    if (seat < s->config.seats) {
        sync_sender_sent(&s->sender, seat, number);
        s->traffic[seat].packets_sent++;
    }
}

uint32_t body_sync_unresolved(const body_sync_t *s, uint8_t seat, uint16_t *numbers, uint32_t max) {
    return sync_sender_unresolved(&s->sender, seat, numbers, max);
}

void body_sync_resolve(body_sync_t *s, uint8_t seat, uint16_t number, bool arrived) { sync_sender_resolve(&s->sender, seat, number, arrived); }

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

bool body_sync_render_offset(const body_sync_t *s, body_sync_id_t id, float position[3], float rotation[4]) {
    if (!sync_authority_body_const(&s->authority, id)) {
        return false;
    }
    position[0] = position[1] = position[2] = 0.0f;
    rotation[0] = rotation[1] = rotation[2] = 0.0f;
    rotation[3] = 1.0f;
    return true;
}

bool body_sync_traffic(const body_sync_t *s, uint8_t seat, body_sync_traffic_t *out) {
    if (seat >= s->config.seats) {
        return false;
    }
    *out = s->traffic[seat];
    const sync_sender_stats_t *st = &s->sender.stats[seat];
    for (int k = 0; k < BODY_SYNC_TRAFFIC_COUNT; k++) {
        out->sent_bytes[k] += st->bits[k] / 8u;
    }
    out->deferred = st->deferred;
    out->stale_max_ticks = st->stale_max_ticks;
    return true;
}
