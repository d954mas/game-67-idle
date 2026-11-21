#include <math.h>
#include <stdio.h>
#include <string.h>

#include "stub_wire.h"

/* A rotation whose squared length is this far from 1 is refused; closer ones are normalised. */
#define ROTATION_TOLERANCE 0.02f

static void put_u16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}
static void put_u32(uint8_t *p, uint32_t v) {
    for (int i = 0; i < 4; i++) {
        p[i] = (uint8_t)(v >> (8 * i));
    }
}
static void put_f32(uint8_t *p, float f) {
    uint32_t v;
    memcpy(&v, &f, 4);
    put_u32(p, v);
}
static uint16_t get_u16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t get_u32(const uint8_t *p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
static float get_f32(const uint8_t *p) {
    uint32_t v = get_u32(p);
    float f;
    memcpy(&f, &v, 4);
    return f;
}

static bool id_ok(uint32_t id) { return (id & 0xFFFFu) < STUB_SLOTS && (id >> 16) != 0; }

static bool fail(char *error, size_t size, const char *what) {
    if (error && size) {
        snprintf(error, size, "%s", what);
    }
    return false;
}

static bool finite3(const float v[3]) { return isfinite(v[0]) && isfinite(v[1]) && isfinite(v[2]); }

static void scale_to(float v[3], float limit) {
    if (!finite3(v)) {
        v[0] = v[1] = v[2] = 0.0f;
        return;
    }
    const float length = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (length > limit) {
        const float k = limit / length * 0.9999f; /* below the limit after rounding, never past it */
        for (int i = 0; i < 3; i++) {
            v[i] *= k;
        }
    }
}

bool stub_wire_limit(const stub_limits_t *limits, stub_body_state_t *body) {
    if (!finite3(body->position) || !finite3(body->rotation) || !isfinite(body->rotation[3])) {
        return false;
    }
    for (int i = 0; i < 3; i++) {
        const float lo = limits->bounds_min[i], hi = limits->bounds_max[i];
        body->position[i] = body->position[i] < lo ? lo : body->position[i] > hi ? hi : body->position[i];
    }
    scale_to(body->velocity, limits->max_speed);
    scale_to(body->angular, limits->max_spin);
    return true;
}

uint32_t stub_wire_bodies_fit(uint32_t capacity) { return capacity < STUB_HEADER_BYTES ? 0 : (capacity - STUB_HEADER_BYTES) / STUB_BODY_BYTES; }

void stub_wire_put_header(uint8_t *buffer, uint16_t count) {
    buffer[0] = STUB_WIRE_VERSION;
    put_u16(buffer + 1, count);
}

void stub_wire_put_body(uint8_t *p, const stub_body_state_t *b) {
    put_u32(p, b->id);
    p[4] = (uint8_t)((b->flags & 0x0F) | (b->kinematic ? STUB_FLAG_KINEMATIC : 0) | (b->asleep ? STUB_FLAG_ASLEEP : 0));
    p[5] = (uint8_t)(b->locks & STUB_LOCKS_ALL);
    p += 6;
    for (int i = 0; i < 3; i++, p += 4) {
        put_f32(p, b->position[i]);
    }
    for (int i = 0; i < 4; i++, p += 4) {
        put_f32(p, b->rotation[i]);
    }
    for (int i = 0; i < 3; i++, p += 4) {
        put_f32(p, b->velocity[i]);
    }
    for (int i = 0; i < 3; i++, p += 4) {
        put_f32(p, b->angular[i]);
    }
    p[0] = b->seat;
    put_u16(p + 1, b->number);
    p[3] = b->hold;
    put_u32(p + 4, b->holder);
}

static bool length_within(const float v[3], float limit) {
    float sum = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
    return isfinite(sum) && sum <= limit * limit;
}

bool stub_wire_read_state(const uint8_t *data, uint32_t size, uint32_t seats, const stub_limits_t *limits, stub_body_state_t *out,
                          uint32_t max, uint32_t *count, char *error, size_t error_size) {
    *count = 0;
    if (!data || size < STUB_HEADER_BYTES) {
        return fail(error, error_size, "state packet shorter than its header");
    }
    if (data[0] != STUB_WIRE_VERSION) {
        return fail(error, error_size, "state packet of another format");
    }
    uint32_t n = get_u16(data + 1);
    if ((uint64_t)n * STUB_BODY_BYTES + STUB_HEADER_BYTES != size) {
        return fail(error, error_size, "state packet size does not match its body count");
    }
    if (n > max) {
        return fail(error, error_size, "state packet names more bodies than a world holds");
    }
    const uint8_t *p = data + STUB_HEADER_BYTES;
    for (uint32_t i = 0; i < n; i++, p += STUB_BODY_BYTES) {
        stub_body_state_t *b = &out[i];
        memset(b, 0, sizeof(*b));
        b->id = get_u32(p);
        uint8_t flags = p[4];
        if (!id_ok(b->id)) {
            return fail(error, error_size, "body id out of range");
        }
        if (flags & STUB_FLAG_RESERVED) {
            return fail(error, error_size, "reserved body flag bits");
        }
        if (p[5] & ~STUB_LOCKS_ALL) {
            return fail(error, error_size, "reserved lock bits");
        }
        b->locks = p[5];
        b->flags = flags & 0x0F;
        b->kinematic = (flags & STUB_FLAG_KINEMATIC) != 0;
        b->asleep = (flags & STUB_FLAG_ASLEEP) != 0;
        const uint8_t *f = p + 6;
        for (int k = 0; k < 3; k++, f += 4) {
            b->position[k] = get_f32(f);
        }
        for (int k = 0; k < 4; k++, f += 4) {
            b->rotation[k] = get_f32(f);
        }
        for (int k = 0; k < 3; k++, f += 4) {
            b->velocity[k] = get_f32(f);
        }
        for (int k = 0; k < 3; k++, f += 4) {
            b->angular[k] = get_f32(f);
        }
        for (int k = 0; k < 3; k++) {
            if (!(b->position[k] >= limits->bounds_min[k] && b->position[k] <= limits->bounds_max[k])) {
                return fail(error, error_size, "position outside the bounds");
            }
        }
        if (!length_within(b->velocity, limits->max_speed) || !length_within(b->angular, limits->max_spin)) {
            return fail(error, error_size, "velocity out of range");
        }
        float norm = 0.0f;
        for (int k = 0; k < 4; k++) {
            if (!(fabsf(b->rotation[k]) <= 1.01f)) {
                return fail(error, error_size, "rotation out of range");
            }
            norm += b->rotation[k] * b->rotation[k];
        }
        if (!(fabsf(norm - 1.0f) <= ROTATION_TOLERANCE)) {
            return fail(error, error_size, "rotation is not a unit quaternion");
        }
        float scale = 1.0f / sqrtf(norm);
        for (int k = 0; k < 4; k++) {
            b->rotation[k] *= scale;
        }
        b->seat = f[0];
        b->number = get_u16(f + 1);
        b->hold = f[3];
        b->holder = get_u32(f + 4);
        if (b->seat >= seats || b->hold > 1 || (b->holder != 0 && !id_ok(b->holder))) {
            return fail(error, error_size, "authority out of range");
        }
    }
    *count = n;
    return true;
}

static uint32_t record_size(stub_record_type_t type) {
    return type == STUB_REC_CLAIM ? 8u : type == STUB_REC_RELEASE ? 11u : type == STUB_REC_ASSIGN ? 18u : 0u;
}

/* Two bytes: flags in bits 0..3, kinematic in 4, motion known in 5; then the locks. */
static void put_body(uint8_t *at, const stub_record_t *r) {
    at[0] = (uint8_t)((r->flags & 0x0Fu) | (r->kinematic ? 0x10u : 0u) | (r->motion_known ? 0x20u : 0u));
    at[1] = (uint8_t)(r->locks & STUB_LOCKS_ALL);
}

static bool get_body(const uint8_t *at, stub_record_t *out) {
    if ((at[0] & 0xC0u) || (at[1] & ~STUB_LOCKS_ALL)) {
        return false;
    }
    out->flags = at[0] & 0x0Fu;
    out->kinematic = (at[0] & 0x10u) != 0;
    out->motion_known = (at[0] & 0x20u) != 0;
    out->locks = at[1];
    return true;
}

uint32_t stub_wire_put_record(const stub_record_t *r, uint8_t *buffer, uint32_t capacity) {
    uint32_t size = record_size(r->type);
    if (size == 0 || capacity < size) {
        return 0;
    }
    buffer[0] = (uint8_t)r->type;
    put_u32(buffer + 1, r->id);
    if (r->type == STUB_REC_CLAIM) {
        buffer[5] = r->hold;
        put_u16(buffer + 6, r->sequence);
    } else if (r->type == STUB_REC_RELEASE) {
        put_u16(buffer + 5, r->number);
        put_u16(buffer + 7, r->sequence);
        put_body(buffer + 9, r);
    } else if (r->type == STUB_REC_ASSIGN) {
        buffer[5] = r->seat;
        put_u16(buffer + 6, r->number);
        buffer[8] = r->hold;
        put_u16(buffer + 9, r->sequence);
        put_u32(buffer + 11, r->holder);
        buffer[15] = r->forced;
        put_body(buffer + 16, r);
    }
    return size;
}

bool stub_wire_read_record(const uint8_t *data, uint32_t size, uint32_t seats, bool from_host, stub_record_t *out, char *error, size_t error_size) {
    memset(out, 0, sizeof(*out));
    if (!data || size < 1) {
        return fail(error, error_size, "empty seam record");
    }
    if (data[0] < STUB_REC_CLAIM || data[0] > STUB_REC_ASSIGN) {
        return fail(error, error_size, "unknown seam record");
    }
    out->type = (stub_record_type_t)data[0];
    if (size != record_size(out->type)) {
        return fail(error, error_size, "seam record of the wrong size");
    }
    if ((out->type == STUB_REC_ASSIGN) != from_host) {
        return fail(error, error_size, "a seam record this side may not send");
    }
    out->id = get_u32(data + 1);
    if (!id_ok(out->id)) {
        return fail(error, error_size, "body id out of range");
    }
    if (out->type == STUB_REC_CLAIM) {
        out->hold = data[5];
        out->sequence = get_u16(data + 6);
        if (out->sequence == 0) {
            return fail(error, error_size, "claim without a sequence");
        }
    } else if (out->type == STUB_REC_RELEASE) {
        out->number = get_u16(data + 5);
        out->sequence = get_u16(data + 7);
        if (!get_body(data + 9, out)) {
            return fail(error, error_size, "reserved body bits in a release");
        }
    } else if (out->type == STUB_REC_ASSIGN) {
        out->seat = data[5];
        out->number = get_u16(data + 6);
        out->hold = data[8];
        out->sequence = get_u16(data + 9);
        out->holder = get_u32(data + 11);
        out->forced = data[15];
        if (!get_body(data + 16, out)) {
            return fail(error, error_size, "reserved body bits in an announcement");
        }
        if (out->seat >= seats) {
            return fail(error, error_size, "seat out of range");
        }
        if ((out->holder != 0 && !id_ok(out->holder)) || out->forced > 1) {
            return fail(error, error_size, "holder or forced out of range");
        }
    }
    if (out->hold > 1) {
        return fail(error, error_size, "hold out of range");
    }
    return true;
}
