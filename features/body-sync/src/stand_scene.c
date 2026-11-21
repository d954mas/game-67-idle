#include <math.h>
#include <stdio.h>
#include <string.h>

#include "features/body_sync/body_sync_scene.h"
#include "stand_logic.h"

body_sync_character_tuning_t body_sync_character_tuning_default(void) {
    return (body_sync_character_tuning_t){
        .radius = 0.3f,
        .height = 1.7f,
        .mass = 60.0f,
        .speed = 3.6f,
        .heavy_speed = 2.0f,
        .reach = 1.65f,
        .hold_forward = 0.6f,
        .hold_up = 0.25f,
        .throw_speed = 11.5f,
        .punch_windup_s = 0.35f,
        .punch_recover_s = 0.57f,
        .punch_range = 1.4f,
        .punch_cone_deg = 50.0f,
        .punch_push = 4.0f,
        .knock_speed = 3.0f,
    };
}

void body_sync_scene_reset(body_sync_scene_t *scene, const char *name) {
    memset(scene, 0, sizeof(*scene));
    if (name) {
        snprintf(scene->name, sizeof(scene->name), "%s", name);
    }
    scene->character = body_sync_character_tuning_default();
    for (int i = 0; i < 3; i++) {
        scene->bounds_min[i] = -50.0f;
        scene->bounds_max[i] = 50.0f;
    }
}

body_sync_body_desc_t body_sync_body_default(body_sync_shape_t shape, const float size[3], float mass, const float position[3]) {
    body_sync_body_desc_t body;
    memset(&body, 0, sizeof(body));
    body.shape = shape;
    for (int i = 0; i < 3; i++) {
        body.size[i] = size ? size[i] : 0.0f;
        body.position[i] = position ? position[i] : 0.0f;
    }
    body.mass = mass;
    body.friction = 0.6f;
    body.rotation[3] = 1.0f;
    body.motion = BODY_SYNC_MOTION_DYNAMIC;
    body.role = BODY_SYNC_ROLE_PROP;
    body.route = BODY_SYNC_ROUTE_NONE;
    body.debris_template = BODY_SYNC_TEMPLATE_NONE;
    return body;
}

uint32_t body_sync_scene_add(body_sync_scene_t *scene, const body_sync_body_desc_t *body) {
    if (scene->body_count >= BODY_SYNC_SCENE_BODIES_MAX) {
        return UINT32_MAX;
    }
    scene->bodies[scene->body_count] = *body;
    return scene->body_count++;
}

static bool finite3(const float v[3]) { return isfinite(v[0]) && isfinite(v[1]) && isfinite(v[2]); }

static bool name_ok(const char *name, bool allow_empty) {
    const char *end = memchr(name, 0, BODY_SYNC_NAME_MAX);
    size_t length = end ? (size_t)(end - name) : BODY_SYNC_NAME_MAX;
    if (length >= BODY_SYNC_NAME_MAX) {
        return false;
    }
    if (length == 0) {
        return allow_empty;
    }
    for (size_t i = 0; i < length; i++) {
        char c = name[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) {
            return false;
        }
    }
    return true;
}

static bool fail(body_sync_error_t *error, uint32_t line, const char *what, const char *detail) {
    if (error) {
        error->line = line;
        snprintf(error->text, sizeof(error->text), "%s%s%s", what, detail ? ": " : "", detail ? detail : "");
    }
    return false;
}

/* Scene errors name the body, never a line: a scene is C data, and a line reads like the
   scenario's. */
static bool fail_body(body_sync_error_t *error, bool is_template, uint32_t index, const char *what, const body_sync_body_desc_t *b) {
    if (error) {
        error->line = 0;
        char name[BODY_SYNC_NAME_MAX];
        const char *end = memchr(b->name, 0, BODY_SYNC_NAME_MAX);
        size_t n = end ? (size_t)(end - b->name) : BODY_SYNC_NAME_MAX - 1;
        memcpy(name, b->name, n);
        name[n] = '\0';
        snprintf(error->text, sizeof(error->text), "%s %u '%s': %s", is_template ? "template" : "body", index, name, what);
    }
    return false;
}

static bool aabb_of(const body_sync_body_desc_t *b, float lo[3], float hi[3]) {
    float half[3];
    if (b->shape == BODY_SYNC_SHAPE_BOX) {
        /* A rotated box's extent on each axis: |R| times its half extents. */
        const float *q = b->rotation;
        const float r[3][3] = {
            {1 - 2 * (q[1] * q[1] + q[2] * q[2]), 2 * (q[0] * q[1] - q[2] * q[3]), 2 * (q[0] * q[2] + q[1] * q[3])},
            {2 * (q[0] * q[1] + q[2] * q[3]), 1 - 2 * (q[0] * q[0] + q[2] * q[2]), 2 * (q[1] * q[2] - q[0] * q[3])},
            {2 * (q[0] * q[2] - q[1] * q[3]), 2 * (q[1] * q[2] + q[0] * q[3]), 1 - 2 * (q[0] * q[0] + q[1] * q[1])},
        };
        for (int i = 0; i < 3; i++) {
            half[i] = fabsf(r[i][0]) * b->size[0] + fabsf(r[i][1]) * b->size[1] + fabsf(r[i][2]) * b->size[2];
        }
    } else {
        float reach = b->size[0] + (b->shape == BODY_SYNC_SHAPE_CAPSULE ? b->size[1] : 0.0f);
        half[0] = half[1] = half[2] = reach;
    }
    for (int i = 0; i < 3; i++) {
        lo[i] = b->position[i] - half[i];
        hi[i] = b->position[i] + half[i];
    }
    return true;
}

static bool inside_bounds(const body_sync_scene_t *scene, const float p[3]) {
    for (int i = 0; i < 3; i++) {
        if (!(p[i] >= scene->bounds_min[i] && p[i] <= scene->bounds_max[i])) {
            return false;
        }
    }
    return true;
}

stand_limits_t stand_limits_of(const body_sync_scene_t *scene) {
    stand_limits_t l;
    for (int i = 0; i < 3; i++) {
        l.bounds_min[i] = scene->bounds_min[i] - STAND_BOUNDS_MARGIN;
        l.bounds_max[i] = scene->bounds_max[i] + STAND_BOUNDS_MARGIN;
    }
    l.max_speed = STAND_MAX_SPEED;
    l.max_spin = STAND_MAX_SPIN;
    return l;
}

static bool body_ok(const body_sync_scene_t *scene, const body_sync_body_desc_t *b, bool is_template, uint32_t line, body_sync_error_t *error) {
    if (!name_ok(b->name, true)) {
        return fail_body(error, is_template, line, "body name must be [a-z0-9_], at most 31 characters", b);
    }
    if (b->shape < BODY_SYNC_SHAPE_BOX || b->shape > BODY_SYNC_SHAPE_CAPSULE) {
        return fail_body(error, is_template, line, "unknown shape", b);
    }
    if (!finite3(b->size) || !finite3(b->position) || !isfinite(b->mass) || !isfinite(b->density) || !isfinite(b->friction) ||
        !isfinite(b->restitution) || !isfinite(b->debris_speed) || !isfinite(b->break_speed)) {
        return fail_body(error, is_template, line, "a number is not finite", b);
    }
    int sized = b->shape == BODY_SYNC_SHAPE_BOX ? 3 : b->shape == BODY_SYNC_SHAPE_SPHERE ? 1 : 2;
    for (int i = 0; i < sized; i++) {
        if (!(b->size[i] > 0.001f && b->size[i] <= 50.0f)) {
            return fail_body(error, is_template, line, "size must be in (0.001, 50] m", b);
        }
    }
    for (int i = 0; i < 3; i++) {
        if (fabsf(b->position[i]) > 1000.0f) {
            return fail_body(error, is_template, line, "position beyond 1000 m", b);
        }
    }
    float norm = 0.0f;
    for (int i = 0; i < 4; i++) {
        if (!isfinite(b->rotation[i])) {
            return fail_body(error, is_template, line, "rotation is not finite", b);
        }
        norm += b->rotation[i] * b->rotation[i];
    }
    if (fabsf(sqrtf(norm) - 1.0f) > 1e-3f) {
        return fail_body(error, is_template, line, "rotation must be a unit quaternion", b);
    }
    if (b->motion > BODY_SYNC_MOTION_KINEMATIC || b->role > BODY_SYNC_ROLE_BREAKABLE) {
        return fail_body(error, is_template, line, "unknown motion or role", b);
    }
    if (b->motion != BODY_SYNC_MOTION_STATIC) {
        if (!(b->mass > 0.0f && b->mass <= 10000.0f) && !(b->mass == 0.0f && b->density > 0.0f && b->density <= 100000.0f)) {
            return fail_body(error, is_template, line, "a moving body needs mass in (0, 10000] kg or a positive density", b);
        }
    }
    if (!(b->friction >= 0.0f && b->friction <= 2.0f) || !(b->restitution >= 0.0f && b->restitution <= 1.0f)) {
        return fail_body(error, is_template, line, "friction must be 0..2 and restitution 0..1", b);
    }
    if (b->motion == BODY_SYNC_MOTION_STATIC && (b->role != BODY_SYNC_ROLE_PROP || (b->flags & BODY_SYNC_BODY_GRABBABLE))) {
        return fail_body(error, is_template, line, "a static body must be a plain prop", b);
    }
    if (b->flags & ~(uint32_t)(BODY_SYNC_BODY_GRABBABLE | BODY_SYNC_BODY_HEAVY | BODY_SYNC_BODY_START_AWAKE | BODY_SYNC_BODY_PUNCH_BREAKS)) {
        return fail_body(error, is_template, line, "unknown flags", b);
    }
    if (is_template && b->role != BODY_SYNC_ROLE_PROP) {
        return fail_body(error, is_template, line, "a template must be a prop", b);
    }
    if (is_template && b->motion != BODY_SYNC_MOTION_DYNAMIC) {
        return fail_body(error, is_template, line, "a template must be dynamic", b);
    }
    if (b->role == BODY_SYNC_ROLE_MANNEQUIN) {
        if (b->shape != BODY_SYNC_SHAPE_CAPSULE) {
            return fail_body(error, is_template, line, "a mannequin is a capsule", b);
        }
        if (b->route != BODY_SYNC_ROUTE_NONE && (b->route >= scene->route_count || b->motion != BODY_SYNC_MOTION_KINEMATIC)) {
            return fail_body(error, is_template, line, "a walking mannequin is kinematic on a route of the scene", b);
        }
        if (b->route == BODY_SYNC_ROUTE_NONE && b->motion != BODY_SYNC_MOTION_DYNAMIC) {
            return fail_body(error, is_template, line, "a standing mannequin is dynamic", b);
        }
    } else if (b->motion == BODY_SYNC_MOTION_KINEMATIC) {
        return fail_body(error, is_template, line, "only a walking mannequin is kinematic", b);
    }
    if (b->role == BODY_SYNC_ROLE_BREAKABLE) {
        if (b->motion != BODY_SYNC_MOTION_DYNAMIC) {
            return fail_body(error, is_template, line, "a breakable is dynamic", b);
        }
        if ((uint32_t)b->contents_first + b->contents_count > scene->template_count) {
            return fail_body(error, is_template, line, "contents beyond the templates", b);
        }
        if (b->debris_count > 0 && b->debris_template >= scene->template_count) {
            return fail_body(error, is_template, line, "debris template out of range", b);
        }
        if (b->debris_count > 32 || !(b->debris_speed >= 0.0f && b->debris_speed <= 30.0f) ||
            !(b->break_speed > 0.0f && b->break_speed <= 100.0f)) {
            return fail_body(error, is_template, line, "debris count 0..32, debris speed 0..30, break speed (0, 100]", b);
        }
    }
    return true;
}

bool body_sync_scene_validate(const body_sync_scene_t *scene, body_sync_error_t *error) {
    if (!scene) {
        return fail(error, 0, "no scene", NULL);
    }
    if (!name_ok(scene->name, false)) {
        return fail(error, 0, "scene name must be [a-z0-9_], 1..31 characters", NULL);
    }
    const body_sync_character_tuning_t *t = &scene->character;
    const float knobs[] = {t->radius, t->height, t->mass, t->speed, t->heavy_speed, t->reach, t->hold_forward, t->throw_speed,
                           t->punch_windup_s, t->punch_recover_s, t->punch_range, t->punch_cone_deg};
    for (size_t i = 0; i < sizeof(knobs) / sizeof(knobs[0]); i++) {
        if (!(isfinite(knobs[i]) && knobs[i] > 0.0f && knobs[i] <= 1000.0f)) {
            return fail(error, 0, "character tuning: every size, speed and time must be positive", NULL);
        }
    }
    if (!(t->height > 2.0f * t->radius) || !(t->punch_cone_deg <= 180.0f) || !isfinite(t->hold_up) || fabsf(t->hold_up) > 5.0f ||
        !(t->punch_push >= 0.0f && t->punch_push <= 100.0f) || !(t->knock_speed >= 0.0f && t->knock_speed <= 100.0f)) {
        return fail(error, 0, "character tuning out of range", NULL);
    }
    for (int i = 0; i < 3; i++) {
        if (!(isfinite(scene->bounds_min[i]) && isfinite(scene->bounds_max[i]) && scene->bounds_min[i] < scene->bounds_max[i])) {
            return fail(error, 0, "bounds must be finite with min below max", NULL);
        }
    }
    if (scene->player_count > BODY_SYNC_PLAYERS_MAX || scene->body_count > BODY_SYNC_SCENE_BODIES_MAX ||
        scene->template_count > BODY_SYNC_SCENE_TEMPLATES_MAX || scene->route_count > BODY_SYNC_SCENE_ROUTES_MAX) {
        return fail(error, 0, "a count is over its limit", NULL);
    }
    for (uint32_t p = 0; p < scene->player_count; p++) {
        const body_sync_player_desc_t *player = &scene->players[p];
        if (!finite3(player->position) || !isfinite(player->yaw) || fabsf(player->position[0]) > 1000.0f ||
            fabsf(player->position[1]) > 1000.0f || fabsf(player->position[2]) > 1000.0f) {
            return fail(error, 0, "a player's spawn is out of range", NULL);
        }
    }
    for (uint32_t r = 0; r < scene->route_count; r++) {
        const body_sync_route_t *route = &scene->routes[r];
        if (route->point_count < 2 || route->point_count > BODY_SYNC_ROUTE_POINTS_MAX || !(route->speed > 0.0f && route->speed <= 20.0f)) {
            return fail(error, 0, "a route needs 2..8 points and a speed in (0, 20] m/s", NULL);
        }
        for (uint32_t i = 0; i < route->point_count; i++) {
            if (!isfinite(route->points[i][0]) || !isfinite(route->points[i][1]) || fabsf(route->points[i][0]) > 1000.0f ||
                fabsf(route->points[i][1]) > 1000.0f) {
                return fail(error, 0, "a route point is out of range", NULL);
            }
        }
    }
    for (uint32_t i = 0; i < scene->template_count; i++) {
        if (!body_ok(scene, &scene->templates[i], true, i, error)) {
            return false;
        }
    }
    /* Every break may spawn its contents and debris; the worst case must fit the free slots. */
    uint32_t spawns = 0;
    for (uint32_t i = 0; i < scene->body_count; i++) {
        const body_sync_body_desc_t *b = &scene->bodies[i];
        if (!body_ok(scene, b, false, i, error)) {
            return false;
        }
        float lo[3], hi[3];
        aabb_of(b, lo, hi);
        if (!inside_bounds(scene, lo) || !inside_bounds(scene, hi)) {
            return fail_body(error, false, i, "reaches outside the scene's bounds", b);
        }
        if (b->name[0]) {
            for (uint32_t j = 0; j < i; j++) {
                if (strncmp(scene->bodies[j].name, b->name, BODY_SYNC_NAME_MAX) == 0) {
                    return fail_body(error, false, i, "body names must be unique", b);
                }
            }
        }
        if (b->role == BODY_SYNC_ROLE_BREAKABLE) {
            uint32_t made = (uint32_t)b->contents_count + b->debris_count;
            if (made > STAND_BREAK_SPAWNS_MAX) {
                return fail_body(error, false, i, "a break spawns more than 40 bodies", b);
            }
            spawns += made;
        }
    }
    if (spawns > STAND_BODIES_MAX - scene->body_count - scene->player_count) {
        return fail(error, 0, "the breaks could spawn more bodies than there are free slots", NULL);
    }
    for (uint32_t p = 0; p < scene->player_count; p++) {
        const body_sync_player_desc_t *player = &scene->players[p];
        if (!inside_bounds(scene, player->position)) {
            char what[64];
            snprintf(what, sizeof(what), "player %u spawns outside the scene's bounds", p);
            return fail(error, 0, what, NULL);
        }
        /* The capsule's box against each body's: a player spawned inside a body is pushed out
           at once, differently on every endpoint that builds it. */
        float lo[3], hi[3];
        const float half[3] = {scene->character.radius, 0.5f * scene->character.height, scene->character.radius};
        uint32_t first = UINT32_MAX, containing = UINT32_MAX;
        for (uint32_t i = 0; i < scene->body_count; i++) {
            aabb_of(&scene->bodies[i], lo, hi);
            bool overlap = true, centre = true;
            for (int k = 0; k < 3; k++) {
                overlap = overlap && player->position[k] + half[k] > lo[k] && player->position[k] - half[k] < hi[k];
                centre = centre && player->position[k] > lo[k] && player->position[k] < hi[k];
            }
            first = overlap && first == UINT32_MAX ? i : first;
            containing = centre && containing == UINT32_MAX ? i : containing;
        }
        /* Name the body the player stands in, not merely the first one it touches. */
        uint32_t named = containing != UINT32_MAX ? containing : first;
        if (named != UINT32_MAX) {
            char what[64];
            snprintf(what, sizeof(what), "player %u spawns inside it", p);
            return fail_body(error, false, named, what, &scene->bodies[named]);
        }
        for (uint32_t q = 0; q < p; q++) {
            const float *a = scene->players[q].position, *b = player->position;
            float dx = a[0] - b[0], dz = a[2] - b[2];
            if (dx * dx + dz * dz < 4.0f * scene->character.radius * scene->character.radius && fabsf(a[1] - b[1]) < scene->character.height) {
                char what[64];
                snprintf(what, sizeof(what), "players %u and %u spawn on one spot", q, p);
                return fail(error, 0, what, NULL);
            }
        }
    }
    return true;
}

uint32_t body_sync_scene_find_body(const body_sync_scene_t *scene, const char *name) {
    if (!name || !name[0]) {
        return UINT32_MAX;
    }
    for (uint32_t i = 0; i < scene->body_count && i < BODY_SYNC_SCENE_BODIES_MAX; i++) {
        if (strncmp(scene->bodies[i].name, name, BODY_SYNC_NAME_MAX) == 0) {
            return i;
        }
    }
    return UINT32_MAX;
}

const body_sync_scene_entry_t *body_sync_scene_lookup(const body_sync_scene_entry_t *entries, uint32_t count, const char *name) {
    if (!entries || !name) {
        return NULL;
    }
    for (uint32_t i = 0; i < count; i++) {
        if (entries[i].name && strcmp(entries[i].name, name) == 0) {
            return &entries[i];
        }
    }
    return NULL;
}
