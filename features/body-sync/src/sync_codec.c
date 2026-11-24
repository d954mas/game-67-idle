#include <math.h>
#include <stdio.h>
#include <string.h>

#include "sync_codec.h"

/* The wire's own structs: net-bits tables name members of these types. */
typedef struct w_head {
    uint32_t slot;
    uint32_t gen;
    bool sim;
    bool rest;
    bool rare;
    bool relayed;
} w_head_t;

typedef struct w_origin {
    uint32_t seat;
    uint32_t age;
} w_origin_t;

typedef struct w_pose {
    float x, y, z;
    float q[4];
} w_pose_t;

typedef struct w_motion {
    float vx, vy, vz, wx, wy, wz;
} w_motion_t;

typedef struct w_mask {
    uint32_t mask;
} w_mask_t;

typedef struct w_rmotion {
    bool kinematic;
    uint32_t locks;
} w_rmotion_t;

typedef struct w_rflags {
    uint32_t flags;
} w_rflags_t;

typedef struct w_rauthority {
    uint32_t seat;
    uint32_t number;
    uint32_t hold; /* body_sync_hold_t: 2 bits, UNTIL_REST the largest */
} w_rauthority_t;

typedef struct w_rholder {
    bool present;
    uint32_t slot;
    uint32_t gen;
} w_rholder_t;

_Static_assert(BODY_SYNC_BODIES_MAX == 1 << SYNC_SLOT_BITS, "a slot is SYNC_SLOT_BITS wide");
_Static_assert(BODY_SYNC_SEATS_MAX <= 4, "the authority group names a seat in 2 bits");
_Static_assert(BODY_SYNC_FLAG_BITS == 4, "the flags group is 4 bits");

static bool limits_ok(const sync_limits_t *l) {
    for (int i = 0; i < 3; i++) {
        if (!(isfinite(l->bounds_min[i]) && isfinite(l->bounds_max[i]) && l->bounds_min[i] < l->bounds_max[i])) {
            return false;
        }
    }
    return isfinite(l->max_speed) && l->max_speed > 0.0f && isfinite(l->max_spin) && l->max_spin > 0.0f;
}

/* Symmetric with zero on the grid and no point past the limit. */
static net_bits_range_t symmetric(float limit, float step) {
    float half = floorf(limit / step) * step;
    return (net_bits_range_t){-half, half, step};
}

/* From the box's low edge in whole steps, the last point at or inside its high edge: a reader
   never sees a coordinate outside the box. net-bits ends a grid at the first point at or past
   its max, so the max sits half a step before that point. */
static net_bits_range_t inside(float lo, float hi, float step) {
    double n = floor(((double)hi - (double)lo) / (double)step);
    return (net_bits_range_t){lo, (float)((double)lo + (n - 0.5) * (double)step), step};
}

/* One expression on both sides, so a writer's check and a reader's agree to the bit. */
static float length2(const float v[3]) { return v[0] * v[0] + v[1] * v[1] + v[2] * v[2]; }

static bool layout(net_bits_layout_t *l, const net_bits_field_t *fields, uint32_t count, size_t size, uint64_t *hash) {
    if (!net_bits_layout_init(l, fields, count, size)) {
        return false;
    }
    *hash = net_bits_hash_mix(*hash, l->hash);
    return true;
}

bool sync_codec_init(sync_codec_t *c, const sync_limits_t *limits) {
    memset(c, 0, sizeof(*c));
    if (!limits || !limits_ok(limits)) {
        return false;
    }
    c->limits = *limits;
    const float *lo = limits->bounds_min, *hi = limits->bounds_max;
    const net_bits_range_t v = symmetric(limits->max_speed, SYNC_VELOCITY_STEP), w = symmetric(limits->max_spin, SYNC_SPIN_STEP);
    const net_bits_field_t head[] = {NET_BITS_UINT(w_head_t, slot, SYNC_SLOT_BITS), NET_BITS_UINT(w_head_t, gen, SYNC_GEN_BITS),
                                     NET_BITS_BOOL(w_head_t, sim), NET_BITS_BOOL(w_head_t, rest), NET_BITS_BOOL(w_head_t, rare),
                                     NET_BITS_BOOL(w_head_t, relayed)};
    const net_bits_field_t origin[] = {NET_BITS_UINT(w_origin_t, seat, 2), NET_BITS_UINT(w_origin_t, age, SYNC_RELAY_AGE_BITS)};
    net_bits_range_t cp[3], sp[3];
    for (int i = 0; i < 3; i++) {
        cp[i] = inside(lo[i], hi[i], SYNC_COPY_POSITION_STEP);
        sp[i] = inside(lo[i], hi[i], SYNC_SIM_POSITION_STEP);
    }
    const net_bits_field_t copy[] = {NET_BITS_RANGED(w_pose_t, x, cp[0].min, cp[0].max, cp[0].step),
                                     NET_BITS_RANGED(w_pose_t, y, cp[1].min, cp[1].max, cp[1].step),
                                     NET_BITS_RANGED(w_pose_t, z, cp[2].min, cp[2].max, cp[2].step), NET_BITS_QUAT(w_pose_t, q, SYNC_COPY_QUAT_BITS)};
    const net_bits_field_t sim[] = {NET_BITS_RANGED(w_pose_t, x, sp[0].min, sp[0].max, sp[0].step),
                                    NET_BITS_RANGED(w_pose_t, y, sp[1].min, sp[1].max, sp[1].step),
                                    NET_BITS_RANGED(w_pose_t, z, sp[2].min, sp[2].max, sp[2].step), NET_BITS_QUAT(w_pose_t, q, SYNC_SIM_QUAT_BITS)};
    const net_bits_field_t motion[] = {NET_BITS_RANGED(w_motion_t, vx, v.min, v.max, v.step), NET_BITS_RANGED(w_motion_t, vy, v.min, v.max, v.step),
                                       NET_BITS_RANGED(w_motion_t, vz, v.min, v.max, v.step), NET_BITS_RANGED(w_motion_t, wx, w.min, w.max, w.step),
                                       NET_BITS_RANGED(w_motion_t, wy, w.min, w.max, w.step), NET_BITS_RANGED(w_motion_t, wz, w.min, w.max, w.step)};
    const net_bits_field_t mask[] = {NET_BITS_UINT(w_mask_t, mask, SYNC_RARE_GROUPS)};
    const net_bits_field_t rmotion[] = {NET_BITS_BOOL(w_rmotion_t, kinematic), NET_BITS_UINT(w_rmotion_t, locks, 6)};
    const net_bits_field_t rflags[] = {NET_BITS_UINT(w_rflags_t, flags, 4)};
    const net_bits_field_t rauthority[] = {NET_BITS_UINT(w_rauthority_t, seat, 2), NET_BITS_UINT(w_rauthority_t, number, 16),
                                           NET_BITS_UINT(w_rauthority_t, hold, 2)};
    const net_bits_field_t rholder[] = {NET_BITS_BOOL(w_rholder_t, present), NET_BITS_UINT(w_rholder_t, slot, SYNC_SLOT_BITS),
                                        NET_BITS_UINT(w_rholder_t, gen, SYNC_GEN_BITS)};
    memcpy(c->head_fields, head, sizeof(head));
    memcpy(c->origin_fields, origin, sizeof(origin));
    memcpy(c->copy_fields, copy, sizeof(copy));
    memcpy(c->sim_fields, sim, sizeof(sim));
    memcpy(c->motion_fields, motion, sizeof(motion));
    memcpy(c->mask_fields, mask, sizeof(mask));
    memcpy(c->rmotion_fields, rmotion, sizeof(rmotion));
    memcpy(c->rflags_fields, rflags, sizeof(rflags));
    memcpy(c->rauthority_fields, rauthority, sizeof(rauthority));
    memcpy(c->rholder_fields, rholder, sizeof(rholder));
    uint64_t hash = net_bits_hash_mix(net_bits_hash_mix(SYNC_PROTOCOL, SYNC_GEN_BITS), SYNC_DATA_MAX);
    if (!layout(&c->head, c->head_fields, 6, sizeof(w_head_t), &hash) || !layout(&c->origin, c->origin_fields, 2, sizeof(w_origin_t), &hash) || !layout(&c->copy_pose, c->copy_fields, 4, sizeof(w_pose_t), &hash) ||
        !layout(&c->sim_pose, c->sim_fields, 4, sizeof(w_pose_t), &hash) || !layout(&c->motion, c->motion_fields, 6, sizeof(w_motion_t), &hash) ||
        !layout(&c->rare_mask, c->mask_fields, 1, sizeof(w_mask_t), &hash) ||
        !layout(&c->rare_motion, c->rmotion_fields, 2, sizeof(w_rmotion_t), &hash) ||
        !layout(&c->rare_flags, c->rflags_fields, 1, sizeof(w_rflags_t), &hash) ||
        !layout(&c->rare_authority, c->rauthority_fields, 3, sizeof(w_rauthority_t), &hash) ||
        !layout(&c->rare_holder, c->rholder_fields, 3, sizeof(w_rholder_t), &hash)) {
        return false;
    }
    c->hash = hash;
    c->record_bits_max = 1 + c->head.bits + c->origin.bits + c->sim_pose.bits + c->motion.bits + c->rare_mask.bits + c->rare_motion.bits +
                         c->rare_flags.bits + c->rare_authority.bits + c->rare_holder.bits + 4u + 8u * SYNC_DATA_MAX;
    c->record_bits_min = 1 + c->head.bits + c->copy_pose.bits;
    return c->record_bits_max <= BODY_SYNC_RECORD_MAX * 8u;
}

static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

static bool finite3(const float v[3]) { return isfinite(v[0]) && isfinite(v[1]) && isfinite(v[2]); }

static void scale_to(float v[3], float limit) {
    float length = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (length > limit) {
        float k = limit / length;
        v[0] *= k;
        v[1] *= k;
        v[2] *= k;
    }
}

bool sync_codec_limit(const sync_codec_t *c, sync_record_t *r) {
    if (!finite3(r->position) || !isfinite(r->rotation[0]) || !isfinite(r->rotation[1]) || !isfinite(r->rotation[2]) || !isfinite(r->rotation[3])) {
        return false;
    }
    for (int i = 0; i < 3; i++) {
        r->position[i] = clampf(r->position[i], c->limits.bounds_min[i], c->limits.bounds_max[i]);
    }
    if (!finite3(r->velocity) || !finite3(r->angular) || r->at_rest) {
        memset(r->velocity, 0, sizeof(r->velocity));
        memset(r->angular, 0, sizeof(r->angular));
    }
    scale_to(r->velocity, c->limits.max_speed);
    scale_to(r->angular, c->limits.max_spin);
    /* On the grid, and no longer than the limit there: rounding a component up can lengthen
       the vector, so the longest component steps toward zero until it fits. */
    for (int which = 0; which < 2; which++) {
        float *v = which ? r->angular : r->velocity;
        const net_bits_range_t range = (which ? c->motion_fields[3] : c->motion_fields[0]).range;
        float limit = which ? c->limits.max_spin : c->limits.max_speed;
        for (int i = 0; i < 3; i++) {
            v[i] = net_bits_quantize(range, v[i]);
        }
        for (int guard = 0; guard < 64 && length2(v) > limit * limit; guard++) {
            int k = fabsf(v[0]) >= fabsf(v[1]) && fabsf(v[0]) >= fabsf(v[2]) ? 0 : fabsf(v[1]) >= fabsf(v[2]) ? 1 : 2;
            v[k] = net_bits_quantize(range, v[k] > 0.0f ? v[k] - range.step : v[k] + range.step);
        }
    }
    r->rare.groups &= SYNC_RARE_ALL;
    r->rare.data_size = r->rare.data_size > SYNC_DATA_MAX ? SYNC_DATA_MAX : r->rare.data_size;
    r->origin.seat &= 3u;
    r->origin.age = r->origin.age > SYNC_RELAY_AGE_MAX ? (uint8_t)SYNC_RELAY_AGE_MAX : r->origin.age;
    r->rare.flags &= (uint8_t)((1u << BODY_SYNC_FLAG_BITS) - 1u);
    r->rare.locks &= 0x3Fu;
    r->rare.seat &= 3u;
    r->rare.hold = r->rare.hold > BODY_SYNC_HOLD_UNTIL_REST ? (uint8_t)BODY_SYNC_HOLD_UNTIL_RELEASE : r->rare.hold;
    r->slot &= (uint16_t)(BODY_SYNC_BODIES_MAX - 1);
    r->generation_low &= SYNC_GEN_MASK;
    return true;
}

static bool has_motion(const sync_record_t *r) { return r->sim && !r->at_rest; }

uint32_t sync_codec_rare_bits(const sync_codec_t *c, const sync_rare_t *rare) {
    uint8_t g = rare->groups & SYNC_RARE_ALL;
    if (!g) {
        return 0;
    }
    uint32_t bits = c->rare_mask.bits;
    bits += (g & SYNC_RARE_MOTION) ? c->rare_motion.bits : 0;
    bits += (g & SYNC_RARE_FLAGS) ? c->rare_flags.bits : 0;
    bits += (g & SYNC_RARE_AUTHORITY) ? c->rare_authority.bits : 0;
    bits += (g & SYNC_RARE_HOLDER) ? c->rare_holder.bits : 0;
    bits += (g & SYNC_RARE_DATA) ? 4u + 8u * (rare->data_size > SYNC_DATA_MAX ? SYNC_DATA_MAX : rare->data_size) : 0;
    return bits;
}

uint32_t sync_codec_record_bits(const sync_codec_t *c, const sync_record_t *r) {
    return 1 + c->head.bits + (r->origin.relayed ? c->origin.bits : 0) + (r->sim ? c->sim_pose.bits : c->copy_pose.bits) +
           (has_motion(r) ? c->motion.bits : 0) + sync_codec_rare_bits(c, &r->rare);
}

static void write_body(net_bits_writer_t *wr, const sync_codec_t *c, const sync_record_t *r) {
    uint8_t g = r->rare.groups & SYNC_RARE_ALL;
    w_head_t head = {.slot = r->slot, .gen = r->generation_low, .sim = r->sim, .rest = r->at_rest, .rare = g != 0, .relayed = r->origin.relayed};
    net_bits_write_record(wr, &c->head, &head);
    if (r->origin.relayed) {
        w_origin_t o = {r->origin.seat, r->origin.age};
        net_bits_write_record(wr, &c->origin, &o);
    }
    w_pose_t pose = {r->position[0], r->position[1], r->position[2], {r->rotation[0], r->rotation[1], r->rotation[2], r->rotation[3]}};
    net_bits_write_record(wr, r->sim ? &c->sim_pose : &c->copy_pose, &pose);
    if (has_motion(r)) {
        w_motion_t m = {r->velocity[0], r->velocity[1], r->velocity[2], r->angular[0], r->angular[1], r->angular[2]};
        net_bits_write_record(wr, &c->motion, &m);
    }
    if (!g) {
        return;
    }
    w_mask_t mask = {g};
    net_bits_write_record(wr, &c->rare_mask, &mask);
    if (g & SYNC_RARE_MOTION) {
        w_rmotion_t m = {r->rare.kinematic, r->rare.locks};
        net_bits_write_record(wr, &c->rare_motion, &m);
    }
    if (g & SYNC_RARE_FLAGS) {
        w_rflags_t f = {r->rare.flags};
        net_bits_write_record(wr, &c->rare_flags, &f);
    }
    if (g & SYNC_RARE_AUTHORITY) {
        w_rauthority_t a = {r->rare.seat, r->rare.number, r->rare.hold};
        net_bits_write_record(wr, &c->rare_authority, &a);
    }
    if (g & SYNC_RARE_HOLDER) {
        bool present = r->rare.holder_slot < BODY_SYNC_BODIES_MAX;
        w_rholder_t h = {present, present ? r->rare.holder_slot : 0u, present ? (uint32_t)(r->rare.holder_generation_low & SYNC_GEN_MASK) : 0u};
        net_bits_write_record(wr, &c->rare_holder, &h);
    }
    if (g & SYNC_RARE_DATA) {
        uint8_t size = r->rare.data_size > SYNC_DATA_MAX ? (uint8_t)SYNC_DATA_MAX : r->rare.data_size;
        net_bits_write(wr, size, 4);
        for (uint8_t i = 0; i < size; i++) {
            net_bits_write(wr, r->rare.data[i], 8);
        }
    }
}

void sync_codec_quantize(const sync_codec_t *c, sync_record_t *r) {
    /* The same tables the reader walks, so the result is what it will hold, bit for bit. */
    uint8_t buffer[BODY_SYNC_RECORD_MAX + 1];
    net_bits_writer_t wr;
    net_bits_writer_init(&wr, buffer, sizeof(buffer));
    net_bits_write(&wr, 1, 1);
    write_body(&wr, c, r);
    net_bits_write(&wr, 0, 1);
    size_t size = net_bits_writer_finish(&wr);
    sync_record_t back;
    uint32_t count = 0;
    if (size && sync_packet_read(c, BODY_SYNC_SEATS_MAX, buffer, (uint32_t)size, &back, 1, &count, NULL, 0) && count == 1) {
        *r = back;
    }
}

void sync_packet_begin(sync_packet_writer_t *w, uint8_t *buffer, uint32_t capacity) {
    net_bits_writer_init(&w->bits, buffer, capacity);
    w->count = 0;
}

bool sync_packet_add(sync_packet_writer_t *w, const sync_codec_t *c, const sync_record_t *r) {
    if (!w->bits.ok || (size_t)sync_codec_record_bits(c, r) + 1u > net_bits_writer_room(&w->bits)) {
        return false;
    }
    net_bits_write(&w->bits, 1, 1);
    write_body(&w->bits, c, r);
    if (w->bits.ok) {
        w->count++;
    }
    return w->bits.ok;
}

uint32_t sync_packet_finish(sync_packet_writer_t *w) {
    if (w->count == 0) {
        return 0;
    }
    net_bits_write(&w->bits, 0, 1);
    return (uint32_t)net_bits_writer_finish(&w->bits);
}

static bool fail(char *error, size_t error_size, const char *why) {
    if (error && error_size) {
        snprintf(error, error_size, "%s", why);
    }
    return false;
}

static bool within(const float v[3], float limit) { return length2(v) <= limit * limit; }

bool sync_packet_read(const sync_codec_t *c, uint8_t seats, const uint8_t *data, uint32_t size, sync_record_t *out, uint32_t max, uint32_t *count,
                      char *error, size_t error_size) {
    *count = 0;
    if (!data || size == 0) {
        return fail(error, error_size, "empty state packet");
    }
    net_bits_reader_t rd;
    net_bits_reader_init(&rd, data, size);
    uint32_t n = 0;
    for (;;) {
        uint32_t more = net_bits_read(&rd, 1);
        if (!rd.ok) {
            return fail(error, error_size, "state packet ends before its end bit");
        }
        if (!more) {
            break;
        }
        if (n >= max) {
            return fail(error, error_size, "state packet holds more records than a world has bodies");
        }
        sync_record_t *r = &out[n];
        memset(r, 0, sizeof(*r));
        w_head_t head;
        w_pose_t pose;
        if (!net_bits_read_record(&rd, &c->head, &head)) {
            return fail(error, error_size, "record head cut short");
        }
        r->slot = (uint16_t)head.slot;
        r->generation_low = (uint8_t)head.gen;
        r->sim = head.sim;
        r->at_rest = head.rest;
        if (head.relayed) {
            w_origin_t o;
            if (!net_bits_read_record(&rd, &c->origin, &o)) {
                return fail(error, error_size, "origin cut short");
            }
            if (o.seat >= seats) {
                return fail(error, error_size, "origin names a seat out of range");
            }
            r->origin = (sync_origin_t){.relayed = true, .seat = (uint8_t)o.seat, .age = (uint8_t)o.age};
        }
        if (!net_bits_read_record(&rd, head.sim ? &c->sim_pose : &c->copy_pose, &pose)) {
            return fail(error, error_size, "pose cut short or off its grid");
        }
        r->position[0] = pose.x;
        r->position[1] = pose.y;
        r->position[2] = pose.z;
        memcpy(r->rotation, pose.q, sizeof(r->rotation));
        if (has_motion(r)) {
            w_motion_t m;
            if (!net_bits_read_record(&rd, &c->motion, &m)) {
                return fail(error, error_size, "velocities cut short or off their grid");
            }
            r->velocity[0] = m.vx;
            r->velocity[1] = m.vy;
            r->velocity[2] = m.vz;
            r->angular[0] = m.wx;
            r->angular[1] = m.wy;
            r->angular[2] = m.wz;
            if (!within(r->velocity, c->limits.max_speed) || !within(r->angular, c->limits.max_spin)) {
                return fail(error, error_size, "velocity out of range");
            }
        }
        if (head.rare) {
            w_mask_t mask;
            if (!net_bits_read_record(&rd, &c->rare_mask, &mask) || mask.mask == 0) {
                return fail(error, error_size, "rare fields with no group");
            }
            r->rare.groups = (uint8_t)mask.mask;
            r->rare.holder_slot = BODY_SYNC_BODIES_MAX;
            if (mask.mask & SYNC_RARE_MOTION) {
                w_rmotion_t m;
                if (!net_bits_read_record(&rd, &c->rare_motion, &m)) {
                    return fail(error, error_size, "motion group cut short");
                }
                r->rare.kinematic = m.kinematic;
                r->rare.locks = (uint8_t)m.locks;
            }
            if (mask.mask & SYNC_RARE_FLAGS) {
                w_rflags_t f;
                if (!net_bits_read_record(&rd, &c->rare_flags, &f)) {
                    return fail(error, error_size, "flags group cut short");
                }
                r->rare.flags = (uint8_t)f.flags;
            }
            if (mask.mask & SYNC_RARE_AUTHORITY) {
                w_rauthority_t a;
                if (!net_bits_read_record(&rd, &c->rare_authority, &a)) {
                    return fail(error, error_size, "authority group cut short");
                }
                if (a.seat >= seats || a.hold > BODY_SYNC_HOLD_UNTIL_REST) {
                    return fail(error, error_size, "authority names a seat or a hold out of range");
                }
                r->rare.seat = (uint8_t)a.seat;
                r->rare.number = (uint16_t)a.number;
                r->rare.hold = (uint8_t)a.hold;
            }
            if (mask.mask & SYNC_RARE_HOLDER) {
                w_rholder_t h;
                if (!net_bits_read_record(&rd, &c->rare_holder, &h)) {
                    return fail(error, error_size, "holder group cut short");
                }
                if (h.present) {
                    r->rare.holder_slot = (uint16_t)h.slot;
                    r->rare.holder_generation_low = (uint8_t)h.gen;
                } else if (h.slot || h.gen) {
                    return fail(error, error_size, "holder group names no holder but carries one");
                }
            }
            if (mask.mask & SYNC_RARE_DATA) {
                uint32_t bytes = net_bits_read(&rd, 4);
                if (!rd.ok || bytes > SYNC_DATA_MAX) {
                    return fail(error, error_size, "data group cut short or too long");
                }
                r->rare.data_size = (uint8_t)bytes;
                for (uint32_t i = 0; i < bytes; i++) {
                    r->rare.data[i] = (uint8_t)net_bits_read(&rd, 8);
                }
                if (!rd.ok) {
                    return fail(error, error_size, "data group cut short");
                }
            }
        }
        n++;
    }
    if (!net_bits_reader_done(&rd)) {
        return fail(error, error_size, "state packet has bytes past its end bit");
    }
    *count = n;
    return true;
}
