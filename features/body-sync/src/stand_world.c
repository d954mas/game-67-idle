#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "features/body_sync/body_sync_stand.h"
#include "stand_world.h"

static b3Quat quat_of(const float q[4]) { return (b3Quat){{q[0], q[1], q[2]}, q[3]}; }

static uint64_t category_of(const stand_slot_t *s) {
    if (s->desc.motion == BODY_SYNC_MOTION_STATIC) {
        return STAND_CAT_STATIC;
    }
    return s->character ? STAND_CAT_CHARACTER : STAND_CAT_BODY;
}

static bool create_body(stand_world_t *w, uint32_t slot, uint16_t generation, const body_sync_body_desc_t *desc, bool character,
                        uint8_t player, const float position[3], const float rotation[4], const float velocity[3], const float angular[3],
                        bool awake) {
    stand_slot_t *s = &w->slots[slot];
    memset(s, 0, sizeof(*s));
    s->desc = *desc;
    s->character = character;
    s->player = player;
    s->generation = generation;

    b3BodyDef def = b3DefaultBodyDef();
    def.type = desc->motion == BODY_SYNC_MOTION_STATIC ? b3_staticBody : desc->motion == BODY_SYNC_MOTION_KINEMATIC ? b3_kinematicBody : b3_dynamicBody;
    def.position = (b3Vec3){position[0], position[1], position[2]};
    def.rotation = quat_of(rotation);
    def.linearVelocity = (b3Vec3){velocity[0], velocity[1], velocity[2]};
    def.angularVelocity = (b3Vec3){angular[0], angular[1], angular[2]};
    def.linearDamping = desc->linear_damping;
    def.angularDamping = desc->angular_damping;
    def.userData = (void *)(uintptr_t)(slot + 1u);
    def.isAwake = def.type != b3_dynamicBody || awake;
    /* The game's capsule never tips; a standing mannequin keeps upright until it is knocked. */
    if (character || (desc->role == BODY_SYNC_ROLE_MANNEQUIN && desc->motion == BODY_SYNC_MOTION_DYNAMIC)) {
        def.motionLocks = (b3MotionLocks){.angularX = true, .angularY = true, .angularZ = true};
    }
    if (character) {
        def.enableSleep = false;
    }
    s->body = b3CreateBody(w->world, &def);
    if (!b3Body_IsValid(s->body)) {
        return false;
    }
    b3ShapeDef shape = b3DefaultShapeDef();
    float volume = stand_shape_volume(desc->shape, desc->size);
    shape.density = desc->mass > 0.0f && volume > 0.0f ? desc->mass / volume : desc->density > 0.0f ? desc->density : 1000.0f;
    shape.baseMaterial.friction = desc->friction;
    shape.baseMaterial.restitution = desc->restitution;
    /* Without it a ball keeps rolling to the floor's edge and a run never comes to rest. */
    shape.baseMaterial.rollingResistance = 0.2f;
    shape.filter.categoryBits = category_of(s);
    shape.filter.maskBits = B3_DEFAULT_MASK_BITS;
    shape.enableContactEvents = true;
    shape.enableHitEvents = def.type != b3_staticBody;
    switch (desc->shape) {
    case BODY_SYNC_SHAPE_BOX: {
        b3BoxHull hull = b3MakeBoxHull(desc->size[0], desc->size[1], desc->size[2]);
        b3CreateHullShape(s->body, &shape, &hull.base);
        break;
    }
    case BODY_SYNC_SHAPE_SPHERE: {
        b3Sphere sphere = {.center = {0.0f, 0.0f, 0.0f}, .radius = desc->size[0]};
        b3CreateSphereShape(s->body, &shape, &sphere);
        break;
    }
    case BODY_SYNC_SHAPE_CAPSULE: {
        b3Capsule capsule = {.center1 = {0.0f, -desc->size[1], 0.0f}, .center2 = {0.0f, desc->size[1], 0.0f}, .radius = desc->size[0]};
        b3CreateCapsuleShape(s->body, &shape, &capsule);
        break;
    }
    }
    s->live = true;
    return true;
}

static body_sync_body_desc_t character_desc(const body_sync_scene_t *scene, uint32_t player) {
    const body_sync_character_tuning_t *t = &scene->character;
    body_sync_body_desc_t d = body_sync_body_default(BODY_SYNC_SHAPE_CAPSULE, (const float[3]){t->radius, 0.5f * t->height - t->radius, 0.0f},
                                                     t->mass, scene->players[player].position);
    d.friction = 0.0f;
    d.model_tag = scene->players[player].model_tag;
    snprintf(d.name, sizeof(d.name), "p%u", player);
    return d;
}

bool stand_world_build(stand_world_t *w, const body_sync_scene_t *scene, uint32_t players, body_sync_error_t *error) {
    memset(w, 0, sizeof(*w));
    w->scene = scene;
    b3WorldDef def = b3DefaultWorldDef();
    def.gravity = (b3Vec3){0.0f, -STAND_GRAVITY, 0.0f};
    w->world = b3CreateWorld(&def);
    if (!b3World_IsValid(w->world)) {
        if (error) {
            snprintf(error->text, sizeof(error->text), "Box3D refused a world");
        }
        return false;
    }
    const float zero[3] = {0.0f, 0.0f, 0.0f};
    for (uint32_t i = 0; i < scene->body_count; i++) {
        const body_sync_body_desc_t *b = &scene->bodies[i];
        if (!create_body(w, i, 1, b, false, 0, b->position, b->rotation, zero, zero, (b->flags & BODY_SYNC_BODY_START_AWAKE) != 0)) {
            if (error) {
                error->line = 0;
                snprintf(error->text, sizeof(error->text), "body %u '%.31s': Box3D refused it", i, b->name);
            }
            return false;
        }
    }
    for (uint32_t p = 0; p < scene->player_count && p < players; p++) {
        body_sync_body_desc_t d = character_desc(scene, p);
        float rotation[4];
        stand_quat_yaw(scene->players[p].yaw, rotation);
        if (!create_body(w, scene->body_count + p, 1, &d, true, (uint8_t)p, d.position, rotation, zero, zero, true)) {
            if (error) {
                snprintf(error->text, sizeof(error->text), "Box3D refused player %u", p);
            }
            return false;
        }
    }
    w->base = scene->body_count + scene->player_count;
    return true;
}

void stand_world_destroy(stand_world_t *w) {
    if (b3World_IsValid(w->world)) {
        b3DestroyWorld(w->world);
    }
    memset(w, 0, sizeof(*w));
}

static uint32_t slot_of_shape(b3ShapeId shape) {
    if (!b3Shape_IsValid(shape)) {
        return UINT32_MAX;
    }
    uintptr_t tag = (uintptr_t)b3Body_GetUserData(b3Shape_GetBody(shape));
    return tag == 0 || tag > STAND_BODIES_MAX ? UINT32_MAX : (uint32_t)(tag - 1u);
}

void stand_world_step(stand_world_t *w) {
    b3World_Step(w->world, STAND_DT, STAND_SUBSTEPS);
    w->hit_count = 0;
    b3ContactEvents events = b3World_GetContactEvents(w->world);
    for (int i = 0; i < events.hitCount && w->hit_count < STAND_HITS_MAX; i++) {
        uint32_t a = slot_of_shape(events.hitEvents[i].shapeIdA), b = slot_of_shape(events.hitEvents[i].shapeIdB);
        if (a != UINT32_MAX && b != UINT32_MAX) {
            w->hits[w->hit_count++] = (stand_hit_t){.slot_a = a, .slot_b = b, .speed = events.hitEvents[i].approachSpeed};
        }
    }
    /* A touch that begins too slow for a hit event (a walk into a pile) is a hit of speed 0: it
       breaks nothing, and the second layer's response counts it. */
    for (int i = 0; i < events.beginCount && w->hit_count < STAND_HITS_MAX; i++) {
        if (!b3Shape_IsValid(events.beginEvents[i].shapeIdA) || !b3Shape_IsValid(events.beginEvents[i].shapeIdB)) {
            continue;
        }
        uint32_t a = slot_of_shape(events.beginEvents[i].shapeIdA), b = slot_of_shape(events.beginEvents[i].shapeIdB);
        if (a != UINT32_MAX && b != UINT32_MAX) {
            w->hits[w->hit_count++] = (stand_hit_t){.slot_a = a, .slot_b = b, .speed = 0.0f};
        }
    }
}

static void fnv(uint64_t *h, const void *data, size_t size) {
    const uint8_t *p = data;
    for (size_t i = 0; i < size; i++) {
        *h = (*h ^ p[i]) * 0x100000001B3ull;
    }
}

uint64_t stand_world_hash(const stand_world_t *w) {
    uint64_t h = 0xCBF29CE484222325ull;
    for (uint32_t slot = 0; slot < STAND_BODIES_MAX; slot++) {
        const stand_slot_t *s = &w->slots[slot];
        if (!s->live) {
            continue;
        }
        b3WorldTransform t = b3Body_GetTransform(s->body);
        b3Vec3 v = b3Body_GetLinearVelocity(s->body);
        b3Vec3 a = b3Body_GetAngularVelocity(s->body);
        int type = (int)b3Body_GetType(s->body);
        uint8_t awake = b3Body_IsAwake(s->body) ? 1 : 0;
        b3MotionLocks locks = b3Body_GetMotionLocks(s->body);
        uint8_t lock_bits = (uint8_t)(locks.linearX | locks.linearY << 1 | locks.linearZ << 2 | locks.angularX << 3 | locks.angularY << 4 |
                                      locks.angularZ << 5);
        uint64_t filter[2] = {0, 0};
        b3ShapeId shapes[1];
        if (b3Body_GetShapes(s->body, shapes, 1) == 1) {
            b3Filter f = b3Shape_GetFilter(shapes[0]);
            filter[0] = f.categoryBits;
            filter[1] = f.maskBits;
        }
        fnv(&h, &slot, sizeof(slot));
        fnv(&h, &lock_bits, 1);
        fnv(&h, filter, sizeof(filter));
        fnv(&h, &s->generation, sizeof(s->generation));
        fnv(&h, &type, sizeof(type));
        fnv(&h, &s->desc.shape, sizeof(s->desc.shape));
        fnv(&h, s->desc.size, sizeof(s->desc.size));
        fnv(&h, &s->desc.mass, sizeof(s->desc.mass));
        float damping[2] = {b3Body_GetLinearDamping(s->body), b3Body_GetAngularDamping(s->body)};
        fnv(&h, damping, sizeof(damping));
        fnv(&h, &t, sizeof(t));
        fnv(&h, &v, sizeof(v));
        fnv(&h, &a, sizeof(a));
        fnv(&h, &awake, 1);
    }
    return h;
}

body_sync_id_t stand_world_id(const stand_world_t *w, uint32_t slot) {
    return slot < STAND_BODIES_MAX && w->slots[slot].live ? body_sync_id_make(slot, w->slots[slot].generation) : BODY_SYNC_ID_NONE;
}

stand_slot_t *stand_world_slot(stand_world_t *w, body_sync_id_t id) {
    uint32_t slot = body_sync_id_slot(id);
    if (slot >= STAND_BODIES_MAX || !w->slots[slot].live || w->slots[slot].generation != body_sync_id_generation(id)) {
        return NULL;
    }
    return &w->slots[slot];
}

uint32_t stand_world_free_slot(const stand_world_t *w) {
    for (uint32_t slot = w->base; slot < STAND_BODIES_MAX; slot++) {
        if (!w->slots[slot].live) {
            return slot;
        }
    }
    return UINT32_MAX;
}

bool stand_world_spawn(stand_world_t *w, body_sync_id_t id, const body_sync_body_desc_t *desc, const float position[3],
                       const float rotation[4], const float velocity[3], const float angular[3]) {
    uint32_t slot = body_sync_id_slot(id);
    if (slot < w->base || slot >= STAND_BODIES_MAX || w->slots[slot].live || body_sync_id_generation(id) == 0) {
        return false;
    }
    return create_body(w, slot, (uint16_t)body_sync_id_generation(id), desc, false, 0, position, rotation, velocity, angular, true);
}

void stand_world_remove(stand_world_t *w, body_sync_id_t id) {
    stand_slot_t *s = stand_world_slot(w, id);
    if (s) {
        b3DestroyBody(s->body);
        s->live = false;
        s->body = b3_nullBodyId;
    }
}

void stand_world_pose(const stand_world_t *w, uint32_t slot, float position[3], float rotation[4]) {
    b3WorldTransform t = b3Body_GetTransform(w->slots[slot].body);
    position[0] = t.p.x;
    position[1] = t.p.y;
    position[2] = t.p.z;
    rotation[0] = t.q.v.x;
    rotation[1] = t.q.v.y;
    rotation[2] = t.q.v.z;
    rotation[3] = t.q.s;
}

float stand_world_speed(const stand_world_t *w, uint32_t slot) {
    b3Vec3 v = b3Body_GetLinearVelocity(w->slots[slot].body);
    return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

void stand_world_set_held(stand_world_t *w, uint32_t slot, bool held, uint64_t clear) {
    stand_slot_t *s = &w->slots[slot];
    if (!s->live) {
        return;
    }
    s->held_filter = held;
    b3ShapeId shapes[1];
    if (b3Body_GetShapes(s->body, shapes, 1) == 1) {
        b3Filter filter = b3Shape_GetFilter(shapes[0]);
        uint64_t category = held ? STAND_CAT_HELD : category_of(s), mask = (held ? 0 : B3_DEFAULT_MASK_BITS) & ~clear;
        /* Compared with the shape, not a cached state: the seam restores a mask on its own. */
        if (filter.categoryBits != category || filter.maskBits != mask) {
            filter.categoryBits = category;
            filter.maskBits = mask;
            b3Shape_SetFilter(shapes[0], filter, true);
        }
    }
}

static void away(const stand_world_t *w, uint32_t slot, const float from[3], float out[3]) {
    float at[3], rot[4];
    stand_world_pose(w, slot, at, rot);
    out[0] = at[0] - from[0];
    out[1] = 0.0f;
    out[2] = at[2] - from[2];
    float length = sqrtf(out[0] * out[0] + out[2] * out[2]);
    if (length < 1e-4f) {
        out[0] = 1.0f;
        out[2] = 0.0f;
        length = 1.0f;
    }
    out[0] /= length;
    out[2] /= length;
}

void stand_world_knock(stand_world_t *w, uint32_t slot, const float from[3], float speed) {
    stand_slot_t *s = &w->slots[slot];
    if (!s->live) {
        return;
    }
    float d[3];
    away(w, slot, from, d);
    b3Body_SetType(s->body, b3_dynamicBody);
    b3Body_SetMotionLocks(s->body, (b3MotionLocks){0});
    b3Body_SetLinearVelocity(s->body, (b3Vec3){d[0] * speed, 1.5f, d[2] * speed});
    /* Spin about the axis across the push so the capsule falls over away from the puncher. */
    b3Body_SetAngularVelocity(s->body, (b3Vec3){d[2] * 4.0f, 0.0f, -d[0] * 4.0f});
    s->knocked = true;
    s->knock_events++;
}

void stand_world_push(stand_world_t *w, uint32_t slot, const float from[3], float speed) {
    stand_slot_t *s = &w->slots[slot];
    if (!s->live || b3Body_GetType(s->body) != b3_dynamicBody) {
        return;
    }
    float d[3];
    away(w, slot, from, d);
    b3Body_SetLinearVelocity(s->body, (b3Vec3){d[0] * speed, 1.0f, d[2] * speed});
}

static void route_point(const body_sync_route_t *route, float s, float out[2]) {
    uint32_t segments = route->loop ? route->point_count : route->point_count - 1;
    for (uint32_t i = 0; i < segments; i++) {
        const float *a = route->points[i];
        const float *b = route->points[(i + 1) % route->point_count];
        float dx = b[0] - a[0], dz = b[1] - a[1];
        float length = sqrtf(dx * dx + dz * dz);
        if (s <= length || i + 1 == segments) {
            float u = length > 1e-6f ? fminf(s / length, 1.0f) : 0.0f;
            out[0] = a[0] + dx * u;
            out[1] = a[1] + dz * u;
            return;
        }
        s -= length;
    }
    out[0] = route->points[0][0];
    out[1] = route->points[0][1];
}

static float route_length(const body_sync_route_t *route) {
    uint32_t segments = route->loop ? route->point_count : route->point_count - 1;
    float total = 0.0f;
    for (uint32_t i = 0; i < segments; i++) {
        const float *a = route->points[i];
        const float *b = route->points[(i + 1) % route->point_count];
        total += sqrtf((b[0] - a[0]) * (b[0] - a[0]) + (b[1] - a[1]) * (b[1] - a[1]));
    }
    return total;
}

void stand_world_walk_routes(stand_world_t *w, body_sync_t *sync) {
    for (uint32_t slot = 0; slot < w->scene->body_count; slot++) {
        stand_slot_t *s = &w->slots[slot];
        if (!s->live || s->knocked || s->desc.role != BODY_SYNC_ROLE_MANNEQUIN || s->desc.route == BODY_SYNC_ROUTE_NONE ||
            !body_sync_is_local(sync, stand_world_id(w, slot))) {
            continue;
        }
        const body_sync_route_t *route = &w->scene->routes[s->desc.route];
        float length = route_length(route);
        if (length < 1e-4f) {
            continue;
        }
        float period = route->loop ? length : 2.0f * length;
        s->route_s = fmodf(s->route_s + route->speed * STAND_DT, period);
        float along = route->loop || s->route_s <= length ? s->route_s : period - s->route_s;
        float at[2];
        route_point(route, along, at);
        b3WorldTransform target = {{at[0], s->desc.position[1], at[1]}, {{0.0f, 0.0f, 0.0f}, 1.0f}};
        b3Body_SetTargetTransform(s->body, target, STAND_DT, true);
    }
}

void stand_world_wake_all(stand_world_t *w) {
    for (uint32_t slot = 0; slot < STAND_BODIES_MAX; slot++) {
        if (w->slots[slot].live && b3Body_GetType(w->slots[slot].body) == b3_dynamicBody) {
            b3Body_SetAwake(w->slots[slot].body, true);
        }
    }
}

/* ---- the test character ---- */

static const body_sync_character_tuning_t *tuning(const stand_world_t *w) { return &w->scene->character; }

void stand_character_init(stand_character_t *c, const stand_world_t *w, uint8_t player) {
    memset(c, 0, sizeof(*c));
    c->player = player;
    c->slot = w->scene->body_count + player;
    c->present = player < w->scene->player_count && c->slot < STAND_BODIES_MAX && w->slots[c->slot].live;
    c->yaw = player < w->scene->player_count ? w->scene->players[player].yaw : 0.0f;
}

void stand_character_position(const stand_character_t *c, const stand_world_t *w, float out[3]) {
    float rot[4];
    stand_world_pose(w, c->slot, out, rot);
}

void stand_character_walk(stand_character_t *c, const stand_world_t *w, float x, float z, body_sync_id_t body, float speed) {
    (void)w;
    c->mode = STAND_CHAR_WALK;
    c->target[0] = x;
    c->target[1] = z;
    c->walk_body = body;
    c->speed = speed;
    c->best = 1e9f;
    c->stall = 0;
}

void stand_character_threw(stand_character_t *c) { c->throw_shown_until = c->now + 1u + (uint32_t)lroundf(0.3f * (float)BODY_SYNC_TICK_HZ); }

uint8_t stand_character_activity(const stand_character_t *c) {
    if (c->mode == STAND_CHAR_PUNCH) {
        return BODY_SYNC_ACTIVITY_PUNCH;
    }
    if (c->now < c->throw_shown_until) {
        return BODY_SYNC_ACTIVITY_THROW;
    }
    if (c->held) {
        return BODY_SYNC_ACTIVITY_CARRY;
    }
    return c->mode == STAND_CHAR_WALK ? BODY_SYNC_ACTIVITY_WALK : BODY_SYNC_ACTIVITY_IDLE;
}

void stand_character_wait(stand_character_t *c, uint32_t ticks) {
    c->mode = STAND_CHAR_WAIT;
    c->wait_left = ticks;
}

static void face(stand_character_t *c, const stand_world_t *w, uint32_t slot) {
    float me[3], at[3], rot[4];
    stand_character_position(c, w, me);
    stand_world_pose(w, slot, at, rot);
    float dx = at[0] - me[0], dz = at[2] - me[2];
    if (dx * dx + dz * dz > 1e-6f) {
        c->yaw = atan2f(dx, dz);
    }
}

void stand_character_punch(stand_character_t *c, stand_world_t *w, body_sync_id_t target) {
    const body_sync_character_tuning_t *t = tuning(w);
    c->mode = STAND_CHAR_PUNCH;
    c->punch_target = target;
    stand_slot_t *s = target ? stand_world_slot(w, target) : NULL;
    if (s) {
        face(c, w, body_sync_id_slot(target));
    }
    c->punch_land = c->now + 1u + (uint32_t)lroundf(t->punch_windup_s * (float)BODY_SYNC_TICK_HZ);
    c->punch_end = c->punch_land + (uint32_t)lroundf(t->punch_recover_s * (float)BODY_SYNC_TICK_HZ);
}

void stand_character_stop(stand_character_t *c, stand_world_t *w) {
    (void)w;
    c->mode = STAND_CHAR_IDLE;
}

bool stand_character_in_reach(const stand_character_t *c, const stand_world_t *w, uint32_t slot) {
    if (slot >= STAND_BODIES_MAX || !w->slots[slot].live) {
        return false;
    }
    float me[3], at[3], rot[4];
    stand_character_position(c, w, me);
    stand_world_pose(w, slot, at, rot);
    float gap = sqrtf((at[0] - me[0]) * (at[0] - me[0]) + (at[2] - me[2]) * (at[2] - me[2])) - stand_footprint_radius(&w->slots[slot].desc);
    return gap <= tuning(w)->reach && fabsf(at[1] - me[1]) <= 1.5f;
}

stand_char_status_t stand_character_tick(stand_character_t *c, stand_world_t *w) {
    const body_sync_character_tuning_t *t = tuning(w);
    stand_char_status_t status = STAND_CHAR_BUSY;
    c->now++;
    stand_slot_t *me = &w->slots[c->slot];
    if (!me->live) {
        return STAND_CHAR_FAILED;
    }
    b3Vec3 v = b3Body_GetLinearVelocity(me->body);
    float vx = 0.0f, vz = 0.0f;
    switch (c->mode) {
    case STAND_CHAR_WALK: {
        float pos[3];
        stand_character_position(c, w, pos);
        float tx = c->target[0], tz = c->target[1];
        stand_slot_t *goal = c->walk_body ? stand_world_slot(w, c->walk_body) : NULL;
        if (c->walk_body && !goal) {
            c->mode = STAND_CHAR_IDLE;
            status = STAND_CHAR_FAILED;
            break;
        }
        if (goal) {
            float at[3], rot[4];
            stand_world_pose(w, body_sync_id_slot(c->walk_body), at, rot);
            tx = at[0];
            tz = at[2];
            if (stand_character_in_reach(c, w, body_sync_id_slot(c->walk_body))) {
                c->mode = STAND_CHAR_IDLE;
                status = STAND_CHAR_DONE;
                break;
            }
        }
        float dx = tx - pos[0], dz = tz - pos[2];
        float distance = sqrtf(dx * dx + dz * dz);
        if (!goal && distance < STAND_ARRIVE_M) {
            c->mode = STAND_CHAR_IDLE;
            status = STAND_CHAR_DONE;
            break;
        }
        if (distance < c->best - 0.01f) {
            c->best = distance;
            c->stall = 0;
        } else if (++c->stall >= 2u * BODY_SYNC_TICK_HZ) {
            c->mode = STAND_CHAR_IDLE;
            status = STAND_CHAR_FAILED;
            break;
        }
        float speed = c->speed > 0.0f ? c->speed : (c->held_heavy ? t->heavy_speed : t->speed);
        speed = fminf(speed, distance / STAND_DT);
        c->yaw = atan2f(dx, dz);
        vx = dx / distance * speed;
        vz = dz / distance * speed;
        break;
    }
    case STAND_CHAR_WAIT:
        if (c->wait_left == 0 || --c->wait_left == 0) {
            c->mode = STAND_CHAR_IDLE;
            status = STAND_CHAR_DONE;
        }
        break;
    case STAND_CHAR_PUNCH:
        if (c->now == c->punch_land) {
            status = STAND_CHAR_LANDED;
        } else if (c->now >= c->punch_end) {
            c->mode = STAND_CHAR_IDLE;
            status = STAND_CHAR_DONE;
        }
        break;
    case STAND_CHAR_IDLE:
        break;
    }
    b3Body_SetLinearVelocity(me->body, (b3Vec3){vx, v.y, vz});
    /* The facing is the body's rotation, so it travels with the pose; the angular lock keeps
       physics from turning it, and a yaw does not change what an upright capsule collides with. */
    if (!me->knocked) {
        float facing[4];
        stand_quat_yaw(c->yaw, facing);
        b3Quat q = b3Body_GetRotation(me->body);
        if (fabsf(q.v.y - facing[1]) > 1e-6f || fabsf(q.s - facing[3]) > 1e-6f || q.v.x != 0.0f || q.v.z != 0.0f) {
            b3Body_SetTransform(me->body, b3Body_GetPosition(me->body), quat_of(facing));
        }
    }
    if (c->held) {
        stand_slot_t *held = stand_world_slot(w, c->held);
        if (!held) {
            c->held = BODY_SYNC_ID_NONE;
        } else {
            b3Vec3 p = b3Body_GetPosition(me->body);
            float forward[3] = {sinf(c->yaw), 0.0f, cosf(c->yaw)};
            float rot[4];
            stand_quat_yaw(c->yaw, rot);
            b3Body_SetTransform(held->body, (b3Vec3){p.x + forward[0] * t->hold_forward, p.y + t->hold_up, p.z + forward[2] * t->hold_forward},
                                quat_of(rot));
            b3Body_SetLinearVelocity(held->body, (b3Vec3){0.0f, 0.0f, 0.0f});
            b3Body_SetAngularVelocity(held->body, (b3Vec3){0.0f, 0.0f, 0.0f});
        }
    }
    return status;
}

body_sync_id_t stand_character_punch_target(const stand_character_t *c, const stand_world_t *w) {
    const body_sync_character_tuning_t *t = tuning(w);
    float me[3];
    stand_character_position(c, w, me);
    float forward[2] = {sinf(c->yaw), cosf(c->yaw)};
    float cone = cosf(t->punch_cone_deg * 0.017453292f);
    body_sync_id_t best = BODY_SYNC_ID_NONE;
    float best_gap = 1e9f;
    for (uint32_t slot = 0; slot < STAND_BODIES_MAX; slot++) {
        const stand_slot_t *s = &w->slots[slot];
        if (!s->live || slot == c->slot || s->desc.motion == BODY_SYNC_MOTION_STATIC || stand_world_id(w, slot) == c->held) {
            continue;
        }
        float at[3], rot[4];
        stand_world_pose(w, slot, at, rot);
        float dx = at[0] - me[0], dz = at[2] - me[2];
        float length = sqrtf(dx * dx + dz * dz);
        float gap = length - stand_footprint_radius(&s->desc);
        if (gap > t->punch_range || fabsf(at[1] - me[1]) > t->height) {
            continue;
        }
        if (length > 1e-4f && (dx * forward[0] + dz * forward[1]) / length < cone) {
            continue;
        }
        body_sync_id_t id = stand_world_id(w, slot);
        if (id == c->punch_target) {
            return id;
        }
        if (gap < best_gap) {
            best_gap = gap;
            best = id;
        }
    }
    return best;
}

void stand_character_attach(stand_character_t *c, stand_world_t *w, body_sync_id_t id) {
    stand_slot_t *s = stand_world_slot(w, id);
    if (!s) {
        return;
    }
    b3Body_SetType(s->body, b3_kinematicBody);
    stand_world_set_held(w, body_sync_id_slot(id), true, 0);
    c->held = id;
    c->held_heavy = (s->desc.flags & BODY_SYNC_BODY_HEAVY) != 0;
}

body_sync_id_t stand_character_release(stand_character_t *c, stand_world_t *w, const float velocity[3]) {
    body_sync_id_t id = c->held;
    stand_slot_t *s = id ? stand_world_slot(w, id) : NULL;
    c->held = BODY_SYNC_ID_NONE;
    c->held_heavy = false;
    if (!s) {
        return BODY_SYNC_ID_NONE;
    }
    b3Body_SetType(s->body, b3_dynamicBody);
    stand_world_set_held(w, body_sync_id_slot(id), false, 0);
    if (velocity) {
        b3Body_SetLinearVelocity(s->body, (b3Vec3){velocity[0], velocity[1], velocity[2]});
        b3Body_SetAngularVelocity(s->body, (b3Vec3){2.5f, 3.0f, 1.5f});
    } else {
        b3Body_SetLinearVelocity(s->body, (b3Vec3){0.0f, 0.0f, 0.0f});
    }
    return id;
}
