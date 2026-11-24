#include <math.h>
#include <stdio.h>
#include <string.h>

#include "stand_logic.h"

/* ---- a rest's end ---- */

bool stand_copy_rests(bool slow, bool shown, float host_gap_m, float near_m) { return slow && shown && host_gap_m <= near_m; }

bool stand_path_moved(const float (*path)[3], uint32_t count, float step_m, uint32_t ticks) {
    for (uint32_t k = ticks; k < count; k++) {
        float d[3] = {path[k][0] - path[k - ticks][0], path[k][1] - path[k - ticks][1], path[k][2] - path[k - ticks][2]};
        if (sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]) > step_m) {
            return true;
        }
    }
    return false;
}

bool stand_turns_moved(const float (*turns)[4], uint32_t count, float step_rad, uint32_t ticks) {
    for (uint32_t k = ticks; k < count; k++) {
        const float *a = turns[k - ticks], *b = turns[k];
        /* conj(a) * b: its vector part's length against its scalar gives the angle without acos's
           loss near 1, where a step of 0.0195 rad lies. */
        float x = a[3] * b[0] - a[0] * b[3] - a[1] * b[2] + a[2] * b[1];
        float y = a[3] * b[1] + a[0] * b[2] - a[1] * b[3] - a[2] * b[0];
        float z = a[3] * b[2] - a[0] * b[1] + a[1] * b[0] - a[2] * b[3];
        float w = a[3] * b[3] + a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
        if (2.0f * atan2f(sqrtf(x * x + y * y + z * z), fabsf(w)) > step_rad) {
            return true;
        }
    }
    return false;
}

bool stand_copy_breaks(bool host_rests, bool copy_rests, bool host_moved_at_play) { return host_rests && !copy_rests && !host_moved_at_play; }

/* ---- a driven body's rest ---- */

void stand_drive_note(stand_drive_t *drive, float speed, float spin) {
    drive->speed[drive->next] = speed;
    drive->spin[drive->next] = spin;
    drive->next = (drive->next + 1u) % STAND_DRIVE_TICKS;
}

void stand_drive_motion(const stand_drive_t *drive, float dt, float *speed, float *spin) {
    uint32_t last = (drive->next + STAND_DRIVE_TICKS - 1u) % STAND_DRIVE_TICKS;
    float v = 0.0f, w = 0.0f;
    for (uint32_t i = 0; i < STAND_DRIVE_TICKS; i++) {
        v += drive->speed[i];
        w += drive->spin[i];
    }
    *speed = drive->speed[last] * dt > STAND_DRIVE_STEP_M ? drive->speed[last] : v / (float)STAND_DRIVE_TICKS;
    *spin = drive->spin[last] * dt > STAND_DRIVE_STEP_RAD ? drive->spin[last] : w / (float)STAND_DRIVE_TICKS;
}

/* ---- random draws ---- */

void stand_rng_seed(stand_rng_t *rng, uint64_t seed) { rng->state = seed ^ 0x6A09E667F3BCC909ull; }

uint64_t stand_rng_next(stand_rng_t *rng) {
    uint64_t z = (rng->state += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

float stand_rng_unit(stand_rng_t *rng) { return (float)(stand_rng_next(rng) >> 40) * (1.0f / 16777216.0f); }

/* ---- math ---- */

float stand_length3(const float v[3]) { return sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); }

float stand_distance3(const float a[3], const float b[3]) {
    const float d[3] = {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
    return stand_length3(d);
}

float stand_quat_angle_deg(const float a[4], const float b[4]) {
    float dot = fabsf(a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3]);
    dot = dot > 1.0f ? 1.0f : dot;
    return 2.0f * acosf(dot) * 57.29578f;
}

void stand_quat_rotate(const float q[4], const float v[3], float out[3]) {
    const float t[3] = {2.0f * (q[1] * v[2] - q[2] * v[1]), 2.0f * (q[2] * v[0] - q[0] * v[2]), 2.0f * (q[0] * v[1] - q[1] * v[0])};
    const float r[3] = {v[0] + q[3] * t[0] + (q[1] * t[2] - q[2] * t[1]), v[1] + q[3] * t[1] + (q[2] * t[0] - q[0] * t[2]),
                        v[2] + q[3] * t[2] + (q[0] * t[1] - q[1] * t[0])};
    out[0] = r[0];
    out[1] = r[1];
    out[2] = r[2];
}

void stand_quat_mul(const float a[4], const float b[4], float out[4]) {
    const float r[4] = {a[3] * b[0] + b[3] * a[0] + a[1] * b[2] - a[2] * b[1], a[3] * b[1] + b[3] * a[1] + a[2] * b[0] - a[0] * b[2],
                        a[3] * b[2] + b[3] * a[2] + a[0] * b[1] - a[1] * b[0], a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2]};
    memcpy(out, r, sizeof(r));
}

void stand_quat_yaw(float yaw, float out[4]) {
    out[0] = 0.0f;
    out[1] = sinf(0.5f * yaw);
    out[2] = 0.0f;
    out[3] = cosf(0.5f * yaw);
}

float stand_shape_volume(body_sync_shape_t shape, const float size[3]) {
    const float pi = 3.14159265f;
    switch (shape) {
    case BODY_SYNC_SHAPE_BOX:
        return 8.0f * size[0] * size[1] * size[2];
    case BODY_SYNC_SHAPE_SPHERE:
        return 4.0f / 3.0f * pi * size[0] * size[0] * size[0];
    case BODY_SYNC_SHAPE_CAPSULE:
        return pi * size[0] * size[0] * 2.0f * size[1] + 4.0f / 3.0f * pi * size[0] * size[0] * size[0];
    }
    return 0.0f;
}

float stand_footprint_radius(const body_sync_body_desc_t *body) {
    if (body->shape == BODY_SYNC_SHAPE_BOX) {
        return body->size[0] > body->size[2] ? body->size[0] : body->size[2];
    }
    return body->size[0];
}

void stand_ballistic(const float from[3], const float to[3], float speed, float out[3]) {
    float d[3] = {to[0] - from[0], to[1] - from[1], to[2] - from[2]};
    float distance = stand_length3(d);
    distance = distance < 0.1f ? 0.1f : distance;
    float t = distance / speed;
    t = t > 1.0f ? 1.0f : t;
    for (int i = 0; i < 3; i++) {
        out[i] = d[i] * speed / distance;
    }
    out[1] += 0.5f * STAND_GRAVITY * t + STAND_THROW_LIFT * (1.0f - t);
}

/* ---- schedule ---- */

void stand_schedule_init(stand_schedule_t *schedule, const body_sync_scenario_t *scenario) {
    memset(schedule, 0, sizeof(*schedule));
    schedule->scenario = scenario;
    for (uint32_t p = 0; p < BODY_SYNC_PLAYERS_MAX; p++) {
        schedule->active[p] = -1;
    }
    for (uint32_t i = 0; i < scenario->action_count; i++) {
        uint8_t player = scenario->actions[i].player;
        if (player < BODY_SYNC_PLAYERS_MAX) {
            schedule->order[player][schedule->count[player]++] = (uint16_t)i;
        }
    }
}

int32_t stand_schedule_next(stand_schedule_t *schedule, uint8_t player, uint32_t tick) {
    if (player >= BODY_SYNC_PLAYERS_MAX) {
        return -1;
    }
    const body_sync_action_t *actions = schedule->scenario->actions;
    uint32_t cursor = schedule->cursor[player];
    uint32_t count = schedule->count[player];
    int64_t due = -1;
    for (uint32_t j = cursor; j < count; j++) {
        const body_sync_action_t *a = &actions[schedule->order[player][j]];
        if (a->timed && a->tick <= tick) {
            due = j;
        }
    }
    uint32_t start;
    if (due >= 0) {
        start = (uint32_t)due;
    } else if (schedule->active[player] < 0 && cursor < count && !actions[schedule->order[player][cursor]].timed) {
        start = cursor;
    } else {
        return -1;
    }
    schedule->cursor[player] = start + 1;
    schedule->active[player] = schedule->order[player][start];
    return schedule->active[player];
}

void stand_schedule_end(stand_schedule_t *schedule, uint8_t player) {
    if (player < BODY_SYNC_PLAYERS_MAX) {
        schedule->active[player] = -1;
    }
}

int32_t stand_schedule_active(const stand_schedule_t *schedule, uint8_t player) {
    return player < BODY_SYNC_PLAYERS_MAX ? schedule->active[player] : -1;
}

bool stand_schedule_done(const stand_schedule_t *schedule) {
    for (uint32_t p = 0; p < BODY_SYNC_PLAYERS_MAX; p++) {
        if (schedule->active[p] >= 0 || schedule->cursor[p] < schedule->count[p]) {
            return false;
        }
    }
    return schedule->net_next >= schedule->scenario->net_count;
}

const body_sync_net_event_t *stand_schedule_net(stand_schedule_t *schedule, uint32_t tick) {
    if (schedule->net_next < schedule->scenario->net_count && schedule->scenario->net[schedule->net_next].tick <= tick) {
        return &schedule->scenario->net[schedule->net_next++];
    }
    return NULL;
}

/* ---- host rules ---- */

static float horizontal_gap(const float a[3], const float b[3]) {
    const float dx = a[0] - b[0], dz = a[2] - b[2];
    return sqrtf(dx * dx + dz * dz);
}

bool stand_rules_grab(const stand_target_t *target, const float claimant[3], float reach) {
    if (!target->live || !target->desc || target->character || target->held || !(target->desc->flags & BODY_SYNC_BODY_GRABBABLE)) {
        return false;
    }
    if (fabsf(target->position[1] - claimant[1]) > 1.5f) {
        return false;
    }
    return horizontal_gap(target->position, claimant) - stand_footprint_radius(target->desc) <= reach + STAND_CLAIM_SLACK_M;
}

bool stand_rules_hit(const stand_target_t *target, const float origin[3], float range, stand_effect_t *effect) {
    *effect = STAND_EFFECT_NONE;
    if (!target->live || !target->desc || target->character || target->desc->motion == BODY_SYNC_MOTION_STATIC) {
        return false;
    }
    if (stand_distance3(target->position, origin) - stand_footprint_radius(target->desc) > range + STAND_CLAIM_SLACK_M) {
        return false;
    }
    switch (target->desc->role) {
    case BODY_SYNC_ROLE_MANNEQUIN:
        if (target->knocked) {
            return false;
        }
        *effect = STAND_EFFECT_KNOCK;
        return true;
    case BODY_SYNC_ROLE_BREAKABLE:
        *effect = (target->desc->flags & BODY_SYNC_BODY_PUNCH_BREAKS) ? STAND_EFFECT_BREAK : STAND_EFFECT_PUSH;
        return true;
    case BODY_SYNC_ROLE_PROP:
        *effect = target->host_simulates && !target->held ? STAND_EFFECT_PUSH : STAND_EFFECT_NONE;
        return true;
    }
    return false;
}

stand_effect_t stand_rules_impact(const stand_target_t *target, float speed) {
    if (!target->live || !target->desc || target->character || !isfinite(speed)) {
        return STAND_EFFECT_NONE;
    }
    switch (target->desc->role) {
    case BODY_SYNC_ROLE_BREAKABLE:
        return speed >= target->desc->break_speed ? STAND_EFFECT_BREAK : STAND_EFFECT_NONE;
    case BODY_SYNC_ROLE_MANNEQUIN:
        return !target->knocked && speed >= target->desc->fall_speed ? STAND_EFFECT_KNOCK : STAND_EFFECT_NONE;
    case BODY_SYNC_ROLE_PROP:
        break;
    }
    return STAND_EFFECT_NONE;
}

bool stand_impact_accepts(float gap, float reach, float approach_max, float claimed_speed) {
    /* NaN fails every comparison, so a non-finite measure refuses. */
    return gap <= reach && claimed_speed <= approach_max * STAND_IMPACT_SPEED_SCALE + STAND_IMPACT_SPEED_SLACK;
}

bool stand_claim_keys_has(const stand_claim_keys_t *keys, stand_claim_key_t key) {
    for (uint32_t i = 0; i < keys->count; i++) {
        const stand_claim_key_t *k = &keys->keys[i];
        if (k->agent == key.agent && k->target == key.target && k->number == key.number && k->seat == key.seat) {
            return true;
        }
    }
    return false;
}

bool stand_claim_keys_add(stand_claim_keys_t *keys, stand_claim_key_t key) {
    if (stand_claim_keys_has(keys, key)) {
        return false;
    }
    keys->keys[keys->next] = key;
    keys->next = (keys->next + 1u) % STAND_CLAIM_KEYS_MAX;
    keys->count += keys->count < STAND_CLAIM_KEYS_MAX ? 1u : 0u;
    return true;
}

void stand_rules_debris(stand_rng_t *rng, const body_sync_body_desc_t *breakable, const float position[3], const float rotation[4],
                        float out_position[3], float out_velocity[3]) {
    float extent[3];
    for (int i = 0; i < 3; i++) {
        extent[i] = breakable->shape == BODY_SYNC_SHAPE_BOX ? breakable->size[i] : breakable->size[0];
    }
    float local[3], direction[3];
    for (int i = 0; i < 3; i++) {
        local[i] = (stand_rng_unit(rng) * 2.0f - 1.0f) * extent[i] * 0.6f;
    }
    for (int i = 0; i < 3; i++) {
        direction[i] = stand_rng_unit(rng) * 2.0f - 1.0f;
    }
    float length = stand_length3(direction);
    if (length < 1e-3f) {
        direction[0] = 1.0f;
        direction[1] = direction[2] = 0.0f;
        length = 1.0f;
    }
    float speed = breakable->debris_speed * (0.5f + 0.5f * stand_rng_unit(rng));
    float world[3];
    stand_quat_rotate(rotation, local, world);
    for (int i = 0; i < 3; i++) {
        out_position[i] = position[i] + world[i];
        out_velocity[i] = direction[i] / length * speed;
    }
    out_velocity[1] += 0.5f;
}

void stand_rules_content(const body_sync_body_desc_t *content, const float position[3], const float rotation[4], float out_position[3],
                         float out_rotation[4]) {
    float offset[3];
    stand_quat_rotate(rotation, content->position, offset);
    for (int i = 0; i < 3; i++) {
        out_position[i] = position[i] + offset[i];
    }
    stand_quat_mul(rotation, content->rotation, out_rotation);
}

/* ---- records ---- */

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

static uint32_t record_size(stand_record_type_t type) {
    switch (type) {
    case STAND_REC_HIT:
        return 2 + 4 + 2 + 12;
    case STAND_REC_IMPACT:
        return 2 + 4 + 4 + 2 + 4 + 4;
    case STAND_REC_VERDICT:
        return 2 + 4 + 2 + 1;
    case STAND_REC_SPAWN:
        return 2 + 4 + 1 + 52;
    case STAND_REC_REMOVE:
        return 2 + 4;
    }
    return 0;
}

uint32_t stand_record_encode(const stand_record_t *r, uint8_t *buffer, uint32_t capacity) {
    uint32_t size = record_size(r->type);
    if (size == 0 || capacity < size) {
        return 0;
    }
    buffer[0] = STAND_FAMILY_STAND;
    buffer[1] = (uint8_t)r->type;
    uint8_t *p = buffer + 2;
    put_u32(p, r->target);
    p += 4;
    switch (r->type) {
    case STAND_REC_HIT:
        put_u16(p, r->action);
        p += 2;
        for (int i = 0; i < 3; i++, p += 4) {
            put_f32(p, r->origin[i]);
        }
        break;
    case STAND_REC_IMPACT:
        put_u32(p, r->body);
        put_u16(p + 4, r->number);
        put_f32(p + 6, r->speed);
        put_u32(p + 10, r->tick);
        break;
    case STAND_REC_VERDICT:
        put_u16(p, r->action);
        p[2] = r->accepted;
        break;
    case STAND_REC_SPAWN:
        *p++ = r->source;
        for (int i = 0; i < 3; i++, p += 4) {
            put_f32(p, r->position[i]);
        }
        for (int i = 0; i < 4; i++, p += 4) {
            put_f32(p, r->rotation[i]);
        }
        for (int i = 0; i < 3; i++, p += 4) {
            put_f32(p, r->velocity[i]);
        }
        for (int i = 0; i < 3; i++, p += 4) {
            put_f32(p, r->angular[i]);
        }
        break;
    case STAND_REC_REMOVE:
        break;
    }
    return size;
}

static bool id_ok(uint32_t id) { return (id & 0xFFFFu) < STAND_BODIES_MAX && (id >> 16) != 0; }

static bool bounded(float v, float limit) { return isfinite(v) && fabsf(v) <= limit; }

static bool inside(const stand_limits_t *limits, const float p[3]) {
    for (int i = 0; i < 3; i++) {
        if (!(p[i] >= limits->bounds_min[i] && p[i] <= limits->bounds_max[i])) {
            return false;
        }
    }
    return true;
}

static bool length_within(const float v[3], float limit) {
    float sum = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
    return isfinite(sum) && sum <= limit * limit;
}

static bool bad(char *error, size_t size, const char *what) {
    if (error && size) {
        snprintf(error, size, "stand record: %s", what);
    }
    return false;
}

bool stand_record_decode(const uint8_t *data, uint32_t size, bool from_host, uint32_t stamp, const stand_limits_t *limits, stand_record_t *out,
                         char *error, size_t error_size) {
    memset(out, 0, sizeof(*out));
    if (!data || size < 1) {
        return bad(error, error_size, "empty");
    }
    uint8_t type = data[0];
    if (type < STAND_REC_HIT || type > STAND_REC_REMOVE) {
        return bad(error, error_size, "unknown type");
    }
    out->type = (stand_record_type_t)type;
    if (size + 1 != record_size(out->type)) {
        return bad(error, error_size, "wrong size");
    }
    bool host_type = out->type == STAND_REC_VERDICT || out->type == STAND_REC_SPAWN || out->type == STAND_REC_REMOVE;
    if (host_type != from_host) {
        return bad(error, error_size, "a type this side may not send");
    }
    const uint8_t *p = data + 1;
    out->target = get_u32(p);
    p += 4;
    if (!id_ok(out->target)) {
        return bad(error, error_size, "body id out of range");
    }
    switch (out->type) {
    case STAND_REC_HIT:
        out->action = get_u16(p);
        p += 2;
        for (int i = 0; i < 3; i++, p += 4) {
            out->origin[i] = get_f32(p);
        }
        if (!inside(limits, out->origin)) {
            return bad(error, error_size, "origin outside the bounds");
        }
        if (out->action >= BODY_SYNC_SCENARIO_ACTIONS_MAX) {
            return bad(error, error_size, "action out of range");
        }
        break;
    case STAND_REC_IMPACT:
        out->body = get_u32(p);
        out->number = get_u16(p + 4);
        out->speed = get_f32(p + 6);
        out->tick = get_u32(p + 10);
        if (!id_ok(out->body)) {
            return bad(error, error_size, "impact agent id out of range");
        }
        if (out->body == out->target) {
            return bad(error, error_size, "impact agent is its target");
        }
        if (!(isfinite(out->speed) && out->speed >= 0.0f && out->speed <= limits->max_speed)) {
            return bad(error, error_size, "impact speed out of range");
        }
        if (stamp - out->tick > STAND_CONTACT_AGE_MAX_TICKS) {
            return bad(error, error_size, "impact tick out of range");
        }
        break;
    case STAND_REC_VERDICT:
        out->action = get_u16(p);
        out->accepted = p[2];
        if (out->action >= BODY_SYNC_SCENARIO_ACTIONS_MAX || out->accepted > 1) {
            return bad(error, error_size, "verdict out of range");
        }
        break;
    case STAND_REC_SPAWN: {
        out->source = *p++;
        if (out->source >= BODY_SYNC_SCENE_TEMPLATES_MAX) {
            return bad(error, error_size, "template out of range");
        }
        float norm = 0.0f;
        for (int i = 0; i < 3; i++, p += 4) {
            out->position[i] = get_f32(p);
        }
        for (int i = 0; i < 4; i++, p += 4) {
            out->rotation[i] = get_f32(p);
        }
        for (int i = 0; i < 3; i++, p += 4) {
            out->velocity[i] = get_f32(p);
        }
        for (int i = 0; i < 3; i++, p += 4) {
            out->angular[i] = get_f32(p);
        }
        if (!inside(limits, out->position) || !length_within(out->velocity, limits->max_speed) || !length_within(out->angular, limits->max_spin)) {
            return bad(error, error_size, "spawn pose out of range");
        }
        for (int i = 0; i < 4; i++) {
            if (!bounded(out->rotation[i], 1.01f)) {
                return bad(error, error_size, "spawn rotation out of range");
            }
            norm += out->rotation[i] * out->rotation[i];
        }
        if (!(fabsf(norm - 1.0f) <= 0.02f)) {
            return bad(error, error_size, "spawn rotation is not a unit quaternion");
        }
        /* Box3D asserts on a rotation that is only nearly unit. */
        float scale = 1.0f / sqrtf(norm);
        for (int i = 0; i < 4; i++) {
            out->rotation[i] *= scale;
        }
        break;
    }
    case STAND_REC_REMOVE:
        break;
    }
    return true;
}

body_sync_stand_kind_t stand_record_kind(stand_record_type_t type) {
    switch (type) {
    case STAND_REC_HIT:
    case STAND_REC_IMPACT:
        return BODY_SYNC_STAND_KIND_CLAIM;
    case STAND_REC_VERDICT:
        return BODY_SYNC_STAND_KIND_VERDICT;
    case STAND_REC_SPAWN:
        return BODY_SYNC_STAND_KIND_SPAWN;
    case STAND_REC_REMOVE:
        return BODY_SYNC_STAND_KIND_REMOVE;
    }
    return BODY_SYNC_STAND_KIND_CLAIM;
}

/* ---- names ---- */

const char *stand_verb_name(body_sync_verb_t verb) {
    static const char *names[] = {"none", "walk", "grab", "throw", "drop", "punch", "wait", "recall", "remove"};
    return (unsigned)verb < sizeof(names) / sizeof(names[0]) ? names[verb] : "none";
}

const char *stand_expect_name(body_sync_expect_kind_t kind) {
    static const char *names[] = {"none", "agree_at_rest", "no_stuck", "effects_once", "result", "one_of", "metric", "left", "moved", "pace", "effect"};
    return (unsigned)kind < sizeof(names) / sizeof(names[0]) ? names[kind] : "none";
}

const char *body_sync_stand_kind_name(body_sync_stand_kind_t kind) {
    static const char *names[] = {"claim", "verdict", "spawn", "remove"};
    return (unsigned)kind < sizeof(names) / sizeof(names[0]) ? names[kind] : "unknown";
}
