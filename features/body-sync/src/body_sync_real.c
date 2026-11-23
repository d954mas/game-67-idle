#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "features/body_sync/body_sync.h"
#include "sync_authority.h"
#include "sync_codec.h"
#include "sync_masks.h"
#include "sync_playback.h"
#include "sync_receiver.h"
#include "sync_sender.h"
#include "sync_soft.h"

/* The real seam: the Box3D shell around the parts. It reads bodies into the sender's inputs,
   decodes packets into the receiver and the playback buffer, and moves bodies as they say. Who
   gets which record, whose poses count, when a hold settles, how a copy is held, where it is
   at playback time, how fast it may be driven and what the eye sees of a jump are the parts'
   decisions; what is left here is reading and writing Box3D and the contact test. */

enum { BODY_SLOTS_HASH = 4 * BODY_SYNC_BODIES_MAX };

typedef struct entry {
    b3BodyId body;
    body_sync_class_t body_class;
    b3BodyType bound_type;
    b3MotionLocks bound_locks; /* restored when a copy turns local with no state received */
    bool copy_mode;
    bool placed; /* placed as a copy since it became one: until then its first pose is a placement */
    bool shown;  /* it has a drawn pose: a placement goes to the render offset instead of jumping */
    /* At a take from a copy: the newest snapshot then, and whether the take is still an
       optimistic claim. Back as a copy once the take was confirmed (granted, then moved on),
       those snapshots are stale: this side simulated on. Back while the claim still waited (it
       was refused, withdrawn, or a forced move overtook it), they are still the owner's. */
    bool took;
    bool took_pending;
    bool took_any;
    uint32_t took_tick;
    sync_masks_t masks;
    sync_offset_t offset;
    /* The second layer: light enough for it (class LIGHT, mass at most light_mass_max), held
       soft now, and what the last step did to it. */
    bool light;
    bool soft;
    bool soft_hit;
    bool owner_flying;
    sync_soft_still_t owner_still;
    bool rest_held;       /* driven in the rest hold this tick */
    bool pushed;          /* a body of this side's pushed it in the last step */
    bool layer;           /* a layer copy now: it keeps the character bits */
    uint32_t marked;      /* tick + 1 a moving body of this side's last came within reach */
    sync_soft_t soft_state;
} entry_t;

/* The light copies one apply marks (mark_reached), and the walk through their contacts. */
typedef struct marking {
    struct body_sync *s;
    uint16_t queue[BODY_SYNC_BODIES_MAX];
    uint32_t count;
} marking_t;

struct body_sync {
    body_sync_config_t config;
    marking_t marking;
    float dt;
    uint32_t now; /* the last apply's or collect's tick */
    sync_codec_t codec;
    sync_authority_t authority;
    sync_sender_t sender;
    sync_receiver_t receiver;
    sync_playback_t playback;
    entry_t entries[BODY_SYNC_BODIES_MAX];
    bool joined[BODY_SYNC_SEATS_MAX];
    body_sync_traffic_t traffic[BODY_SYNC_SEATS_MAX];
    sync_body_input_t inputs[BODY_SYNC_BODIES_MAX];
    uint8_t send_to[BODY_SYNC_BODIES_MAX];
    sync_record_t scratch[BODY_SYNC_BODIES_MAX];
    char error[128];
    /* A bound body's slot by its Box3D index, to tell a contact with a body this side simulates:
       open addressing, BODY_SLOTS_HASH entries for at most BODY_SYNC_BODIES_MAX bodies. */
    int32_t hash_index[BODY_SLOTS_HASH];
    uint16_t hash_slot[BODY_SLOTS_HASH];
};

const char *body_sync_implementation(void) { return "real 0.7.0"; }

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
        config->seat >= config->seats || config->tick_hz < 1 || config->tick_hz > 1000 || config->delay_min > 60 || config->delay_max > 60 ||
        !(config->light_mass_max >= 0.0f && config->light_mass_max <= 1e6f) ||
        (config->delay_min && config->delay_max && config->delay_min > config->delay_max)) {
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
    sync_playback_init(&s->playback, config->tick_hz, config->delay_min, config->delay_max);
    for (uint32_t i = 0; i < BODY_SYNC_BODIES_MAX; i++) {
        sync_offset_clear(&s->entries[i].offset);
    }
    for (uint32_t i = 0; i < BODY_SLOTS_HASH; i++) {
        s->hash_index[i] = -1;
    }
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

void body_sync_seat_joined(body_sync_t *s, uint8_t seat) {
    if (!s || !seat_ok(s, seat) || (!is_host(s) && seat != BODY_SYNC_SEAT_HOST)) {
        return;
    }
    sync_authority_seat_joined(&s->authority, seat);
    sync_sender_seat(&s->sender, seat, true);
    sync_receiver_forget_seat(&s->receiver, seat, false);
    sync_playback_forget_seat(&s->playback, seat, false);
    s->joined[seat] = true;
}

void body_sync_seat_left(body_sync_t *s, uint8_t seat) {
    if (!s || !seat_ok(s, seat)) {
        return;
    }
    s->joined[seat] = false;
    sync_sender_seat(&s->sender, seat, false);
    sync_receiver_forget_seat(&s->receiver, seat, true);
    sync_playback_forget_seat(&s->playback, seat, true);
    sync_authority_seat_left(&s->authority, seat);
}

static void forget_slot(body_sync_t *s, uint32_t slot) {
    sync_sender_forget(&s->sender, (uint16_t)slot);
    sync_receiver_forget(&s->receiver, (uint16_t)slot);
    sync_playback_forget_body(&s->playback, (uint16_t)slot);
}

static uint32_t hash_of(int32_t index) { return ((uint32_t)index * 2654435761u) % BODY_SLOTS_HASH; }

static void hash_put(body_sync_t *s, b3BodyId body, uint32_t slot) {
    uint32_t h = hash_of(body.index1);
    /* An entry whose slot no longer holds that body is free again: rebinds never fill it. */
    for (uint32_t n = 0; n < BODY_SLOTS_HASH && s->hash_index[h] != -1 && s->hash_index[h] != body.index1; n++) {
        uint32_t was = s->hash_slot[h];
        if (!s->authority.bodies[was].bound || s->entries[was].body.index1 != s->hash_index[h]) {
            break;
        }
        h = (h + 1u) % BODY_SLOTS_HASH;
    }
    s->hash_index[h] = body.index1;
    s->hash_slot[h] = (uint16_t)slot;
}

/* The bound slot of a Box3D body, BODY_SYNC_BODIES_MAX when none. */
static uint32_t hash_get(const body_sync_t *s, b3BodyId body) {
    uint32_t h = hash_of(body.index1);
    for (uint32_t n = 0; n < BODY_SLOTS_HASH && s->hash_index[h] != -1; n++, h = (h + 1u) % BODY_SLOTS_HASH) {
        if (s->hash_index[h] == body.index1) {
            uint32_t slot = s->hash_slot[h];
            return s->authority.bodies[slot].bound && B3_ID_EQUALS(s->entries[slot].body, body) ? slot : BODY_SYNC_BODIES_MAX;
        }
    }
    return BODY_SYNC_BODIES_MAX;
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
    e->light = s->config.light_mass_max > 0.0f && bind->body_class == BODY_SYNC_CLASS_LIGHT && b3Body_GetMass(bind->body) <= s->config.light_mass_max;
    sync_offset_clear(&e->offset);
    forget_slot(s, slot);
    hash_put(s, bind->body, slot);
    return BODY_SYNC_OK;
}

body_sync_result_t body_sync_unbind(body_sync_t *s, body_sync_id_t id) {
    sync_authority_body_t *a = sync_authority_body(&s->authority, id);
    if (a) {
        entry_t *e = &s->entries[body_sync_id_slot(id)];
        if (b3Body_IsValid(e->body)) {
            sync_masks_restore(&e->masks, e->body); /* the caller gets its body back as it bound it */
        }
    }
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

static uint8_t locks_bits(b3MotionLocks l);

/* A grant or a forced move carries the body as it is now, not as the last collect read it. */
static void note_live(body_sync_t *s, body_sync_id_t id) {
    sync_authority_body_t *a = sync_authority_body(&s->authority, id);
    b3BodyId body = s->entries[body_sync_id_slot(id)].body;
    if (a && b3Body_IsValid(body)) {
        sync_authority_note_motion(&s->authority, a, b3Body_GetType(body) == b3_kinematicBody, locks_bits(b3Body_GetMotionLocks(body)));
    }
}

body_sync_result_t body_sync_claim(body_sync_t *s, body_sync_id_t id, body_sync_hold_t hold) { return sync_authority_claim(&s->authority, id, hold); }
body_sync_result_t body_sync_release(body_sync_t *s, body_sync_id_t id) { return sync_authority_release(&s->authority, id); }
bool body_sync_poll_claim(body_sync_t *s, body_sync_claim_event_t *out) { return sync_authority_poll(&s->authority, out); }
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
uint8_t body_sync_flags(const body_sync_t *s, body_sync_id_t id) {
    const sync_authority_body_t *a = sync_authority_body_const(&s->authority, id);
    if (a && !sync_authority_local(&s->authority, a)) {
        uint8_t known = 0;
        const sync_rare_t *rare = sync_playback_rare(&s->playback, (uint16_t)body_sync_id_slot(id), &known);
        if (known & SYNC_RARE_FLAGS) {
            return rare->flags; /* as of the copy's playback time */
        }
    }
    return sync_authority_flags(&s->authority, id);
}

body_sync_result_t body_sync_set_data(body_sync_t *s, body_sync_id_t id, const void *data, uint32_t size) {
    return sync_authority_set_data(&s->authority, id, data, size);
}

uint32_t body_sync_data(const body_sync_t *s, body_sync_id_t id, void *out, uint32_t capacity) {
    const sync_authority_body_t *a = sync_authority_body_const(&s->authority, id);
    if (!a || !out) {
        return 0;
    }
    if (sync_authority_local(&s->authority, a)) {
        return sync_authority_data(&s->authority, id, out, capacity);
    }
    uint8_t known = 0;
    const sync_rare_t *rare = sync_playback_rare(&s->playback, (uint16_t)body_sync_id_slot(id), &known);
    uint32_t size = (known & SYNC_RARE_DATA) ? rare->data_size : 0u; /* as of playback time */
    size = size < capacity ? size : capacity;
    memcpy(out, rare->data, size);
    return size;
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

/* The timeline a record plays on and its sample tick there. */
static void timeline_of(const sync_record_t *r, uint8_t seat, uint32_t tick, uint8_t *timeline, uint32_t *sample) {
    *timeline = r->origin.relayed ? r->origin.seat : seat;
    *sample = r->origin.relayed ? tick - r->origin.age : tick;
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
    /* Only the host relays, and only a guest's body to another guest. */
    for (uint32_t i = 0; i < count; i++) {
        const sync_origin_t *o = &s->scratch[i].origin;
        if (o->relayed && (is_host(s) || o->seat == BODY_SYNC_SEAT_HOST || o->seat == s->config.seat)) {
            return format(s, seat, "a relayed record from a guest, or naming the host or this seat");
        }
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
        uint8_t timeline;
        uint32_t sample;
        timeline_of(r, seat, tick, &timeline, &sample);
        /* A tick far ahead of its timeline's clock would drag every copy of that owner with it:
           alone it is dropped, a run of them restarts the clock (sync_playback_admit). */
        if (!sync_playback_admit(&s->playback, timeline, sample, s->now)) {
            t->rejected++;
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
                /* Not applied, yet buffered: the host's state of a body this guest still holds
                   (its take-back is on the way) is what the copy plays once it is one again. */
                sync_playback_arrival(&s->playback, timeline, sample, s->now);
                sync_playback_file(&s->playback, timeline, sample, r);
                continue;
            }
        }
        if (!sync_receiver_store(&s->receiver, seat, sample, r)) {
            t->stale++;
            continue;
        }
        /* The clock hears only what the seam took: a record dropped above moves no timeline. */
        sync_playback_arrival(&s->playback, timeline, sample, s->now);
        sync_playback_file(&s->playback, timeline, sample, r);
        const sync_received_t *in = &s->receiver.bodies[r->slot];
        body_sync_authority_t now;
        sync_authority_describe(&s->authority, a, &now);
        sync_authority_received(a, (in->known & SYNC_RARE_AUTHORITY) ? in->rare.hold : (uint8_t)now.hold,
                                (in->known & SYNC_RARE_HOLDER)
                                    ? sync_authority_holder_id(&s->authority, in->rare.holder_slot, in->rare.holder_generation_low, SYNC_GEN_BITS)
                                    : BODY_SYNC_ID_NONE,
                                (in->known & SYNC_RARE_FLAGS) ? in->rare.flags : a->flags);
        sync_authority_received_motion(a, (in->known & SYNC_RARE_MOTION) != 0, in->rare.kinematic, in->rare.locks);
        if (in->known & SYNC_RARE_DATA) {
            sync_authority_received_data(a, in->rare.data_size, in->rare.data);
        }
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

static void pose_now(b3BodyId body, float position[3], float rotation[4]) {
    b3Vec3 p = b3Body_GetPosition(body);
    b3Quat q = b3Body_GetRotation(body);
    position[0] = p.x;
    position[1] = p.y;
    position[2] = p.z;
    rotation[0] = q.v.x;
    rotation[1] = q.v.y;
    rotation[2] = q.v.z;
    rotation[3] = q.s;
}

/* Moves the physics pose and lets the render offset keep the drawn pose where it was. */
static void place(entry_t *e, const float position[3], const float rotation[4]) {
    float p0[3], q0[4];
    pose_now(e->body, p0, q0);
    b3Body_SetTransform(e->body, (b3Vec3){position[0], position[1], position[2]}, quat_of(rotation));
    if (e->shown) {
        sync_offset_absorb(&e->offset, p0, q0, position, rotation);
    }
    e->placed = true;
    e->shown = true;
}

/* A body that turned local continues as authority says: the motion of the record that moved
   it, or of the state received, or of the bind; the pose and velocities of the newest state
   received, the drawn pose kept by the render offset. */
static void resume(body_sync_t *s, uint32_t slot, bool was_copy) {
    sync_authority_body_t *a = &s->authority.bodies[slot];
    entry_t *e = &s->entries[slot];
    const sync_received_t *in = &s->receiver.bodies[slot];
    e->copy_mode = false;
    a->resume = false;
    sync_masks_restore(&e->masks, e->body);
    if (was_copy) {
        e->took = true;
        e->took_pending = a->claim_pending;
        e->took_any = sync_playback_newest(&s->playback, (uint16_t)slot, &e->took_tick);
    }
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
    /* A body already local keeps the velocities it simulated (a throw made since the grant);
       only a copy taking over continues from the state received. */
    bool was_soft = e->soft;
    e->soft = false;
    /* A soft copy taken over keeps its own simulation: it is what this player saw, within the
       dead zone or the pause of its owner. */
    if (in->has && was_copy && !was_soft) {
        place(e, in->newest.position, in->newest.rotation);
        b3Body_SetLinearVelocity(e->body, (b3Vec3){in->velocity[0], in->velocity[1], in->velocity[2]});
        b3Body_SetAngularVelocity(e->body, (b3Vec3){in->angular[0], in->angular[1], in->angular[2]});
    }
}

/* Whether a soft copy touches a body this side simulates (its own character, its thrown body). */
/* Whether a body this side simulates and that moves faster than a lean touches it. */
static bool touches_local(const body_sync_t *s, b3BodyId body) {
    b3ContactData contacts[16];
    int n = b3Body_GetContactData(body, contacts, 16);
    for (int i = 0; i < n; i++) {
        bool touching = false;
        for (int m = 0; m < contacts[i].manifoldCount && !touching; m++) {
            touching = contacts[i].manifolds[m].pointCount > 0;
        }
        if (!touching || !b3Shape_IsValid(contacts[i].shapeIdA) || !b3Shape_IsValid(contacts[i].shapeIdB)) {
            continue;
        }
        b3BodyId a = b3Shape_GetBody(contacts[i].shapeIdA), b = b3Shape_GetBody(contacts[i].shapeIdB);
        b3BodyId toucher = B3_ID_EQUALS(a, body) ? b : a;
        uint32_t other = hash_get(s, toucher);
        b3Vec3 v = b3Body_GetLinearVelocity(toucher);
        /* A body of this side's that only leans on it (a character standing still) pauses nothing:
           the pull would wait for ever. */
        if (other < BODY_SYNC_BODIES_MAX && sync_authority_local(&s->authority, &s->authority.bodies[other]) &&
            v.x * v.x + v.y * v.y + v.z * v.z > SYNC_SOFT_TOUCH_SPEED * SYNC_SOFT_TOUCH_SPEED) {
            return true;
        }
    }
    return false;
}

/* The second layer's pause: the host's answer to this side's touch reaches its playback one
   round trip plus the playback delay later (2 ticks of margin for the host's step and the
   acknowledgement's wait), bounded by the layer's floor and cap. */
static uint16_t soft_pause(const body_sync_t *s) {
    return sync_soft_pause(sync_sender_round_trip(&s->sender, BODY_SYNC_SEAT_HOST), s->playback.timelines[BODY_SYNC_SEAT_HOST].delay);
}

/* A soft copy this tick: the part decides (sync_soft.h), the shell performs. */
static void drive_soft(body_sync_t *s, uint32_t slot, const float p[3], const float q[4], const sync_play_t *play) {
    entry_t *e = &s->entries[slot];
    sync_soft_in_t in;
    memset(&in, 0, sizeof(in));
    memcpy(in.position, p, sizeof(in.position));
    memcpy(in.rotation, q, sizeof(in.rotation));
    b3Vec3 v = b3Body_GetLinearVelocity(e->body), w = b3Body_GetAngularVelocity(e->body);
    in.velocity[0] = v.x;
    in.velocity[1] = v.y;
    in.velocity[2] = v.z;
    in.angular[0] = w.x;
    in.angular[1] = w.y;
    in.angular[2] = w.z;
    in.asleep = !b3Body_IsAwake(e->body);
    /* A push pauses the pull as a moving touch does: a slow one stops its pusher in the step. */
    in.touches_local = e->pushed || (!in.asleep && touches_local(s, e->body));
    in.max_speed = s->config.max_speed;
    in.max_spin = s->config.max_spin;
    in.pause_ticks = soft_pause(s);
    sync_soft_out_t out;
    sync_soft_decide(&e->soft_state, &in, e->soft_hit, play, &out);
    e->soft_hit = false;
    switch (out.action) {
    case SYNC_SOFT_NONE:
        break;
    case SYNC_SOFT_PLACE:
        place(e, play->position, play->rotation);
        /* fall through: the owner's velocities go with the placement */
    case SYNC_SOFT_VELOCITY:
        b3Body_SetLinearVelocity(e->body, (b3Vec3){out.velocity[0], out.velocity[1], out.velocity[2]});
        b3Body_SetAngularVelocity(e->body, (b3Vec3){out.angular[0], out.angular[1], out.angular[2]});
        break;
    }
}

/* A copy this tick: its hold, then a move to its playback pose within the seam's speeds. */
static void drive_copy(body_sync_t *s, uint32_t slot) {
    sync_authority_body_t *a = &s->authority.bodies[slot];
    entry_t *e = &s->entries[slot];
    const sync_received_t *in = &s->receiver.bodies[slot];
    sync_play_t play;
    sync_playback_sample(&s->playback, (uint16_t)slot, &play);
    /* A body its owner moves fast is in flight there (an owner's own throw has no hold to say
       so): it is driven along the owner's path, and soft again once it has slowed. */
    float owner_speed = sqrtf(play.velocity[0] * play.velocity[0] + play.velocity[1] * play.velocity[1] + play.velocity[2] * play.velocity[2]);
    e->owner_flying = play.known && (e->owner_flying ? owner_speed >= SYNC_SOFT_LAND_SPEED : owner_speed >= SYNC_SOFT_FLIGHT_SPEED);
    bool owner_rests = sync_soft_owner_rests(&e->owner_still, &play);
    float home_p[3], home_q[4], home_e[3], home_r[3];
    pose_now(e->body, home_p, home_q);
    for (int i = 0; i < 3; i++) {
        home_e[i] = play.position[i] - home_p[i];
    }
    sync_turn_between(home_q, play.rotation, home_r);
    /* A soft copy joining the rest hold inside the dead zone is set on the pose exactly, a move
       too small for the offset to carry; one further off is placed, the offset taking it. */
    bool home = sqrtf(home_e[0] * home_e[0] + home_e[1] * home_e[1] + home_e[2] * home_e[2]) <= SYNC_SOFT_DEAD_M &&
                sqrtf(home_r[0] * home_r[0] + home_r[1] * home_r[1] + home_r[2] * home_r[2]) <= SYNC_SOFT_DEAD_RAD;
    bool rest_hold = e->light && sync_soft_rest_hold(owner_rests, &e->soft_state, e->soft, e->marked == s->now + 1u);
    sync_hold_kind_t kind = sync_authority_hold_kind(&s->authority, a, in->has && (in->known & SYNC_RARE_MOTION) && in->rare.kinematic,
                                                     e->light && !e->owner_flying && !rest_hold);
    bool dynamic = kind != SYNC_HOLD_DRIVEN, soft = kind == SYNC_HOLD_SOFT;
    e->rest_held = rest_hold && kind == SYNC_HOLD_DRIVEN;
    /* A layer copy keeps the character bits, soft, in the rest hold or in flight: this side's own
       character pushes it, meets it at rest or is struck by it; any other driven LIGHT copy (the
       layer off, or held) lets the character through. */
    bool bits = soft || (e->light && sync_authority_hold_kind(&s->authority, a, in->has && (in->known & SYNC_RARE_MOTION) && in->rare.kinematic, true) ==
                                         SYNC_HOLD_SOFT);
    e->layer = bits;
    if (bits) {
        sync_masks_restore(&e->masks, e->body);
    } else if (e->body_class == BODY_SYNC_CLASS_LIGHT && e->copy_mode) {
        sync_masks_clear(&e->masks, e->body, s->config.character_category);
    }
    if (soft && (!e->soft || !e->copy_mode)) {
        sync_soft_enter(&e->soft_state, !e->copy_mode, soft_pause(s));
        e->soft_hit = false;
    }
    bool from_local = !e->copy_mode;
    if (!e->copy_mode || (b3Body_GetType(e->body) == b3_dynamicBody) != dynamic) {
        /* A copy starts with a placement at its playback pose; a body that was local has a
           drawn pose, so that placement goes to the render offset instead of jumping. */
        if (!e->copy_mode) {
            e->placed = false;
            if (e->took && e->took_any && !e->took_pending) {
                sync_playback_forget_until(&s->playback, (uint16_t)slot, e->took_tick);
            }
            e->took = false;
            if (e->body_class == BODY_SYNC_CLASS_LIGHT && !bits) {
                sync_masks_clear(&e->masks, e->body, s->config.character_category);
            }
        }
        e->copy_mode = true;
        b3Body_SetType(e->body, dynamic ? b3_dynamicBody : b3_kinematicBody);
    }
    if (soft && from_local) {
        e->placed = true; /* it goes on from its own simulation: no placement */
    } else if (!soft && e->soft && rest_hold && home) {
        b3Body_SetTransform(e->body, (b3Vec3){play.position[0], play.position[1], play.position[2]}, quat_of(play.rotation));
    } else if (!soft && e->soft) {
        /* Soft to driven is a placement, the offset taking it: a kinematic sweep from where this
           side's prediction left it would bat the soft copies in its path at the sweep's speed. */
        e->placed = false;
    }
    e->soft = soft;
    sync_playback_sample(&s->playback, (uint16_t)slot, &play); /* the entry may have dropped snapshots */
    /* A kinematic copy turns to its owner's rotation, which a lock would stop; a dynamic one
       keeps its owner's locks as they change (a character stays upright). */
    b3MotionLocks locks = dynamic && (in->known & SYNC_RARE_MOTION) ? locks_of(in->rare.locks) : (b3MotionLocks){0};
    if (locks_bits(b3Body_GetMotionLocks(e->body)) != locks_bits(locks)) {
        b3Body_SetMotionLocks(e->body, locks);
    }
    if (!play.known) {
        if (b3Body_IsAwake(e->body) && !dynamic) {
            stop(e->body); /* nothing to play yet: stay put */
        }
        return;
    }
    float p[3], q[4];
    pose_now(e->body, p, q);
    if (soft) {
        drive_soft(s, slot, p, q, &play);
        return;
    }
    /* A copy that would need more than the seam's speeds to reach its pose in one step is placed
       there, the render offset taking the jump; a dynamic one also when its error passes the
       offset's reach, since its correction is bounded far below max_speed. */
    float err[3] = {play.position[0] - p[0], play.position[1] - p[1], play.position[2] - p[2]};
    bool far = sqrtf(err[0] * err[0] + err[1] * err[1] + err[2] * err[2]) > SYNC_OFFSET_SNAP_M;
    bool reachable = !(dynamic && far) && sync_drive_reachable(p, q, &play, s->dt, s->config.max_speed, s->config.max_spin);
    if (play.jumped || !e->placed || !reachable) {
        place(e, play.position, play.rotation);
        if (dynamic) {
            b3Body_SetLinearVelocity(e->body, (b3Vec3){play.velocity[0], play.velocity[1], play.velocity[2]});
            b3Body_SetAngularVelocity(e->body, (b3Vec3){play.angular[0], play.angular[1], play.angular[2]});
        } else {
            stop(e->body);
        }
        return;
    }
    if (dynamic) {
        float v[3], w[3];
        sync_drive_dynamic(p, q, &play, s->dt, v, w);
        b3Body_SetLinearVelocity(e->body, (b3Vec3){v[0], v[1], v[2]});
        b3Body_SetAngularVelocity(e->body, (b3Vec3){w[0], w[1], w[2]});
        /* Locked axes ignore a spin: their part of the owner's rotation is set, as the owner sets
           it; the free axes follow the spin above. */
        float turned[4];
        uint8_t locked = (uint8_t)((locks_bits(locks) >> 3) & 7u);
        if (locked && sync_drive_locked_turn(q, play.rotation, locked, turned)) {
            b3Body_SetTransform(e->body, (b3Vec3){p[0], p[1], p[2]}, quat_of(turned));
        }
    } else if (play.at_rest) {
        /* At rest the worlds agree to the bit: the copy is set on the rest pose, not driven
           (a sleeping kinematic body ignores a target that small). */
        if (memcmp(p, play.position, sizeof(p)) != 0 || memcmp(q, play.rotation, sizeof(q)) != 0) {
            place(e, play.position, play.rotation);
        }
        if (b3Body_IsAwake(e->body)) {
            stop(e->body);
        }
    } else if (play.frozen || (rest_hold && sqrtf(home_e[0] * home_e[0] + home_e[1] * home_e[1] + home_e[2] * home_e[2]) <= SYNC_SOFT_HOLD_M &&
                                sqrtf(home_r[0] * home_r[0] + home_r[1] * home_r[1] + home_r[2] * home_r[2]) <= SYNC_SOFT_HOLD_RAD)) {
        /* A still, awake owner's pose is rounded to the copy grid and wanders by a step of it: a
           copy in the rest hold stays where it is until the owner is a step away. */
        if (b3Body_IsAwake(e->body)) {
            stop(e->body);
        }
    } else {
        b3Body_SetTargetTransform(e->body, (b3WorldTransform){{play.position[0], play.position[1], play.position[2]}, quat_of(play.rotation)}, s->dt, true);
    }
}

static void mark(marking_t *m, b3BodyId body) {
    uint32_t slot = hash_get(m->s, body);
    entry_t *e = slot < BODY_SYNC_BODIES_MAX ? &m->s->entries[slot] : NULL;
    if (e && e->light && e->marked != m->s->now + 1u) {
        e->marked = m->s->now + 1u;
        m->queue[m->count++] = (uint16_t)slot;
    }
}

static bool mark_near(b3ShapeId shape, void *context) {
    mark(context, b3Shape_GetBody(shape));
    return true;
}

/* Every moving body (this side's own, soft copies set going, a copy in flight) marks the light
   copies within reach of its next steps, and through them every light copy they touch: those
   leave the rest hold before the contact, so a pile meets what comes at it as one simulated
   whole, never with a kinematic neighbour as a wall. */
static void mark_reached(body_sync_t *s) {
    if (s->config.light_mass_max <= 0.0f || sync_authority_is_host(&s->authority)) {
        return;
    }
    marking_t *m = &s->marking;
    m->s = s;
    m->count = 0;
    for (uint32_t slot = 0; slot < BODY_SYNC_BODIES_MAX; slot++) {
        entry_t *e = &s->entries[slot];
        if (!s->authority.bodies[slot].bound || !b3Body_IsValid(e->body) || !b3Body_IsAwake(e->body) || b3Body_GetType(e->body) == b3_staticBody) {
            continue;
        }
        /* A soft copy marks only while something of this side's set it going (its pause): moved
           by the pull it is the correction, and marking from it would keep a whole pile soft and
           pulling, never resting, around a heroine who stands in it. */
        if (e->soft && e->copy_mode && e->soft_state.pause == 0) {
            continue;
        }
        b3Vec3 v = b3Body_GetLinearVelocity(e->body), w = b3Body_GetAngularVelocity(e->body);
        b3AABB box = b3Body_ComputeAABB(e->body);
        b3Vec3 half = {0.5f * (box.upperBound.x - box.lowerBound.x), 0.5f * (box.upperBound.y - box.lowerBound.y), 0.5f * (box.upperBound.z - box.lowerBound.z)};
        float reach;
        if (!sync_soft_mover((const float[3]){v.x, v.y, v.z}, (const float[3]){w.x, w.y, w.z}, sqrtf(half.x * half.x + half.y * half.y + half.z * half.z), s->dt,
                             &reach)) {
            continue;
        }
        box.lowerBound = (b3Vec3){box.lowerBound.x - reach, box.lowerBound.y - reach, box.lowerBound.z - reach};
        box.upperBound = (b3Vec3){box.upperBound.x + reach, box.upperBound.y + reach, box.upperBound.z + reach};
        b3World_OverlapAABB(s->config.world, box, b3DefaultQueryFilter(), mark_near, m);
    }
    for (uint32_t k = 0; k < m->count; k++) {
        b3ContactData contacts[16];
        int n = b3Body_GetContactData(s->entries[m->queue[k]].body, contacts, 16);
        for (int i = 0; i < n; i++) {
            if (b3Shape_IsValid(contacts[i].shapeIdA) && b3Shape_IsValid(contacts[i].shapeIdB)) {
                mark(m, b3Shape_GetBody(contacts[i].shapeIdA));
                mark(m, b3Shape_GetBody(contacts[i].shapeIdB));
            }
        }
    }
}

void body_sync_apply(body_sync_t *s, uint32_t tick) {
    s->now = tick;
    sync_playback_advance(&s->playback, tick);
    mark_reached(s);
    for (uint32_t slot = 0; slot < BODY_SYNC_BODIES_MAX; slot++) {
        sync_authority_body_t *a = &s->authority.bodies[slot];
        entry_t *e = &s->entries[slot];
        if (!a->bound || !b3Body_IsValid(e->body)) {
            continue;
        }
        sync_offset_decay(&e->offset);
        if (sync_authority_local(&s->authority, a)) {
            if (e->copy_mode || a->resume) {
                resume(s, slot, e->copy_mode);
            }
            e->shown = true; /* its pose is drawn from here on */
            if (e->took && e->took_pending && !a->claim_pending) {
                e->took_pending = false; /* the claim was granted while it simulated */
            }
            continue;
        }
        drive_copy(s, slot);
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
    b3Vec3 v = b3Body_GetLinearVelocity(e->body);
    b3Vec3 w = b3Body_GetAngularVelocity(e->body);
    bool kinematic = b3Body_GetType(e->body) == b3_kinematicBody;
    uint8_t locks = locks_bits(b3Body_GetMotionLocks(e->body));
    sync_authority_note_motion(&s->authority, a, kinematic, locks);
    sync_record_t *r = &in->state;
    memset(r, 0, sizeof(*r));
    r->slot = (uint16_t)slot;
    r->generation_low = (uint8_t)(a->generation & SYNC_GEN_MASK);
    pose_now(e->body, r->position, r->rotation);
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
    r->rare.data_size = a->data_size;
    memcpy(r->rare.data, a->data, sizeof(r->rare.data));
    in->present = true;
    in->asleep = !b3Body_IsAwake(e->body);
    in->held = a->holder != BODY_SYNC_ID_NONE;
    in->flying = a->hold == BODY_SYNC_HOLD_UNTIL_SETTLED;
    in->character = e->body_class == BODY_SYNC_CLASS_CHARACTER;
}

/* Host: a guest's body relayed to the other guests from the state that guest sent, stamped
   with its age so it plays on that guest's timeline; too old a state is not relayed. */
static void read_relayed(body_sync_t *s, uint32_t slot, uint32_t tick, sync_body_input_t *in) {
    const sync_authority_body_t *a = &s->authority.bodies[slot];
    const sync_received_t *got = &s->receiver.bodies[slot];
    uint32_t arrived = 0;
    if (!sync_playback_line_tick(&s->playback, got->from, got->tick, &arrived)) {
        return;
    }
    int32_t age = (int32_t)(tick - arrived);
    if (age < 0 || age > SYNC_RELAY_AGE_MAX) {
        return;
    }
    in->state = got->newest;
    in->state.slot = (uint16_t)slot;
    in->state.generation_low = (uint8_t)(a->generation & SYNC_GEN_MASK);
    in->state.origin = (sync_origin_t){.relayed = true, .seat = a->seat, .age = (uint8_t)age};
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
    if (!(got->known & SYNC_RARE_DATA)) {
        in->state.rare.data_size = 0;
    }
    in->present = true;
    in->asleep = got->newest.at_rest;
    in->held = now.holder != BODY_SYNC_ID_NONE;
    in->flying = now.hold == BODY_SYNC_HOLD_UNTIL_SETTLED;
    in->character = s->entries[slot].body_class == BODY_SYNC_CLASS_CHARACTER;
}

/* A layer copy that a body of this side's (bound and local, or not bound at all) pushed in the
   step: the game moves its bodies after the apply, so a push from standstill is stopped once by
   the rest hold and seen here, and a slow push keeps the copy soft while it lasts. */
static bool pushed_by_this_side(const body_sync_t *s, b3BodyId body) {
    b3ContactData contacts[16];
    int n = b3Body_GetContactData(body, contacts, 16);
    for (int i = 0; i < n; i++) {
        if (!b3Shape_IsValid(contacts[i].shapeIdA) || !b3Shape_IsValid(contacts[i].shapeIdB)) {
            continue;
        }
        b3BodyId a = b3Shape_GetBody(contacts[i].shapeIdA), b = b3Shape_GetBody(contacts[i].shapeIdB);
        b3BodyId pusher = B3_ID_EQUALS(a, body) ? b : a;
        if (b3Body_GetType(pusher) != b3_dynamicBody) {
            continue;
        }
        uint32_t slot = hash_get(s, pusher);
        if (slot < BODY_SYNC_BODIES_MAX && !sync_authority_local(&s->authority, &s->authority.bodies[slot])) {
            continue; /* a copy pushes nothing on its own account */
        }
        float impulse = 0.0f;
        for (int m = 0; m < contacts[i].manifoldCount; m++) {
            for (int p = 0; p < contacts[i].manifolds[m].pointCount; p++) {
                impulse += contacts[i].manifolds[m].points[p].totalNormalImpulse;
            }
        }
        if (sync_soft_pushed(impulse, b3Body_GetMass(pusher))) {
            return true;
        }
    }
    return false;
}

/* A push reaches through a pile: every layer copy touching a pushed one, and on through them, is
   pressed too. Pulled back toward its owner's older pose it would be pushed into the body that
   presses and placed there again and again; pressed, its pull waits (sync_soft_decide). */
static void press_through(body_sync_t *s, uint32_t tick) {
    if (s->config.light_mass_max <= 0.0f || sync_authority_is_host(&s->authority)) {
        return;
    }
    marking_t *m = &s->marking;
    m->count = 0;
    for (uint32_t slot = 0; slot < BODY_SYNC_BODIES_MAX; slot++) {
        if (s->authority.bodies[slot].bound && s->entries[slot].pushed) {
            m->queue[m->count++] = (uint16_t)slot;
        }
    }
    for (uint32_t k = 0; k < m->count; k++) {
        b3ContactData contacts[16];
        int n = b3Body_GetContactData(s->entries[m->queue[k]].body, contacts, 16);
        for (int i = 0; i < n; i++) {
            if (!b3Shape_IsValid(contacts[i].shapeIdA) || !b3Shape_IsValid(contacts[i].shapeIdB)) {
                continue;
            }
            for (int side = 0; side < 2; side++) {
                uint32_t other = hash_get(s, b3Shape_GetBody(side ? contacts[i].shapeIdB : contacts[i].shapeIdA));
                entry_t *o = other < BODY_SYNC_BODIES_MAX ? &s->entries[other] : NULL;
                if (o && o->copy_mode && o->layer && (o->soft || o->rest_held) && !o->pushed) {
                    o->pushed = true;
                    o->marked = tick + 2u;
                    m->queue[m->count++] = (uint16_t)other;
                }
            }
        }
    }
}

void body_sync_collect(body_sync_t *s, uint32_t tick) {
    s->now = tick;
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
        e->pushed = (e->rest_held || e->soft) && e->copy_mode && pushed_by_this_side(s, e->body);
        if (e->pushed) {
            e->marked = tick + 2u; /* soft for the next apply, and kept soft while the push lasts */
        }
        if (e->soft && e->copy_mode && b3Body_IsAwake(e->body)) {
            b3Vec3 v = b3Body_GetLinearVelocity(e->body), w = b3Body_GetAngularVelocity(e->body), g = b3World_GetGravity(s->config.world);
            float k = b3Body_GetGravityScale(e->body) * s->dt;
            const float vv[3] = {v.x, v.y, v.z}, ww[3] = {w.x, w.y, w.z}, gg[3] = {g.x * k, g.y * k, g.z * k};
            e->soft_hit = sync_soft_hit(&e->soft_state, vv, ww, gg);
        }
        bool mine = sync_authority_local(&s->authority, a);
        bool have = s->receiver.bodies[slot].has;
        if (mine) {
            read_local(s, slot, in);
        } else if (is_host(s) && have) {
            read_relayed(s, slot, tick, in);
        }
        s->send_to[slot] = in->present ? sync_authority_route(&s->authority, slot, have) : 0u;
    }
    press_through(s, tick);
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
    const sync_offset_t *o = &s->entries[body_sync_id_slot(id)].offset;
    memcpy(position, o->position, sizeof(o->position));
    memcpy(rotation, o->rotation, sizeof(o->rotation));
    return true;
}

bool body_sync_playback(const body_sync_t *s, body_sync_id_t id, double *owner_tick) {
    const sync_authority_body_t *a = sync_authority_body_const(&s->authority, id);
    if (!a) {
        return false;
    }
    if (sync_authority_local(&s->authority, a)) {
        *owner_tick = (double)s->now;
        return true;
    }
    return sync_playback_tick(&s->playback, (uint16_t)body_sync_id_slot(id), owner_tick);
}

bool body_sync_is_predicted(const body_sync_t *s, body_sync_id_t id) {
    const sync_authority_body_t *a = sync_authority_body_const(&s->authority, id);
    return a && !sync_authority_local(&s->authority, a) && s->entries[body_sync_id_slot(id)].soft;
}

float body_sync_round_trip(const body_sync_t *s, uint8_t seat) { return sync_sender_round_trip(&s->sender, seat); }

bool body_sync_is_layer_copy(const body_sync_t *s, body_sync_id_t id) {
    const sync_authority_body_t *a = sync_authority_body_const(&s->authority, id);
    return a && !sync_authority_local(&s->authority, a) && s->entries[body_sync_id_slot(id)].copy_mode && s->entries[body_sync_id_slot(id)].layer;
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
