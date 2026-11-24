#include <stdio.h>
#include <string.h>

#include "features/body_sync/body_sync_scene.h"

/* The stand's own scenes. Only their shape matters to the tests (what touches what, what can
   be grabbed); the sizes and masses are plausible room furniture, not measured game content. */

#define HALF_PI 1.5707964f
#define PLAYER_Y 0.87f /* the default capsule's centre: half its height plus a hair */

static void add_floor(body_sync_scene_t *scene) {
    body_sync_body_desc_t floor = body_sync_body_default(BODY_SYNC_SHAPE_BOX, (const float[3]){25.0f, 0.25f, 25.0f}, 0.0f,
                                                         (const float[3]){0.0f, -0.25f, 0.0f});
    floor.motion = BODY_SYNC_MOTION_STATIC;
    snprintf(floor.name, sizeof(floor.name), "floor");
    body_sync_scene_add(scene, &floor);
    for (int i = 0; i < 3; i++) {
        scene->bounds_min[i] = i == 1 ? -5.0f : -30.0f;
        scene->bounds_max[i] = i == 1 ? 40.0f : 30.0f;
    }
}

static void add_player(body_sync_scene_t *scene, float x, float z, float yaw) {
    body_sync_player_desc_t *player = &scene->players[scene->player_count++];
    player->position[0] = x;
    player->position[1] = PLAYER_Y;
    player->position[2] = z;
    player->yaw = yaw;
    player->model_tag = 100u + scene->player_count;
}

static uint32_t add_box(body_sync_scene_t *scene, const char *name, float hx, float hy, float hz, float mass, float x, float y, float z,
                        uint32_t flags, uint32_t tag) {
    body_sync_body_desc_t box = body_sync_body_default(BODY_SYNC_SHAPE_BOX, (const float[3]){hx, hy, hz}, mass, (const float[3]){x, y, z});
    if (name) {
        snprintf(box.name, sizeof(box.name), "%s", name);
    }
    box.flags = flags;
    box.model_tag = tag;
    return body_sync_scene_add(scene, &box);
}

static uint32_t add_sphere(body_sync_scene_t *scene, const char *name, float radius, float mass, float x, float y, float z, uint32_t flags,
                           uint32_t tag) {
    body_sync_body_desc_t ball = body_sync_body_default(BODY_SYNC_SHAPE_SPHERE, (const float[3]){radius, 0.0f, 0.0f}, mass,
                                                        (const float[3]){x, y, z});
    if (name) {
        snprintf(ball.name, sizeof(ball.name), "%s", name);
    }
    ball.flags = flags;
    ball.model_tag = tag;
    return body_sync_scene_add(scene, &ball);
}

static void build_floor_wall(body_sync_scene_t *s) {
    body_sync_scene_reset(s, "floor_wall");
    add_floor(s);
    uint32_t wall = add_box(s, "wall", 0.25f, 1.5f, 4.0f, 0.0f, 6.0f, 1.5f, 0.0f, 0, 2);
    s->bodies[wall].motion = BODY_SYNC_MOTION_STATIC;
    add_box(s, "crate", 0.2f, 0.2f, 0.2f, 3.0f, 1.0f, 0.2f, 0.0f, BODY_SYNC_BODY_GRABBABLE, 3);
    add_player(s, -3.0f, -2.0f, HALF_PI);
    add_player(s, 0.0f, 0.0f, HALF_PI);
}

/* The second layer's bounce: a crate the guest throws at the wall falls back into light boxes
   standing at its foot, on the guest's side of the throw. */
static void build_wall_boxes(body_sync_scene_t *s) {
    body_sync_scene_reset(s, "wall_boxes");
    add_floor(s);
    uint32_t wall = add_box(s, "wall", 0.25f, 1.5f, 4.0f, 0.0f, 6.0f, 1.5f, 0.0f, 0, 2);
    s->bodies[wall].motion = BODY_SYNC_MOTION_STATIC;
    add_box(s, "crate", 0.2f, 0.2f, 0.2f, 3.0f, 1.0f, 0.2f, 0.0f, BODY_SYNC_BODY_GRABBABLE, 3);
    for (int i = 0; i < 6; i++) {
        char name[BODY_SYNC_NAME_MAX];
        snprintf(name, sizeof(name), "light%d", i);
        add_box(s, name, 0.15f, 0.15f, 0.15f, 1.0f, 5.2f - 0.35f * (float)(i / 3), 0.15f, 0.35f * (float)(i % 3 - 1), BODY_SYNC_BODY_GRABBABLE, 4);
    }
    add_player(s, -3.0f, -2.0f, HALF_PI);
    add_player(s, 0.0f, 0.0f, HALF_PI);
}

static void build_stack10(body_sync_scene_t *s) {
    body_sync_scene_reset(s, "stack10");
    add_floor(s);
    for (int i = 0; i < 10; i++) {
        char name[BODY_SYNC_NAME_MAX];
        snprintf(name, sizeof(name), "box%d", i);
        add_box(s, name, 0.25f, 0.25f, 0.25f, 4.0f, 0.0f, 0.25f + 0.5f * (float)i, 0.0f, BODY_SYNC_BODY_GRABBABLE, 3);
    }
    add_player(s, -3.0f, 0.0f, HALF_PI);
    add_player(s, 3.0f, 0.0f, -HALF_PI);
}

static void add_pile60(body_sync_scene_t *s) {
    int n = 0;
    for (int y = 0; y < 3; y++) {
        for (int x = 0; x < 5; x++) {
            for (int z = 0; z < 4; z++) {
                char name[BODY_SYNC_NAME_MAX];
                snprintf(name, sizeof(name), "pile%d", n++);
                add_box(s, name, 0.12f, 0.12f, 0.12f, 0.4f, 2.0f + 0.25f * (float)(x - 2), 0.12f + 0.24f * (float)y, 0.25f * ((float)z - 1.5f),
                        BODY_SYNC_BODY_GRABBABLE, 4);
            }
        }
    }
}

static void build_pile60(body_sync_scene_t *s) {
    body_sync_scene_reset(s, "pile60");
    add_floor(s);
    add_pile60(s);
    add_sphere(s, "ball", 0.15f, 2.0f, -0.6f, 0.15f, 0.0f, BODY_SYNC_BODY_GRABBABLE, 5);
    add_player(s, -3.0f, -2.0f, HALF_PI);
    add_player(s, -1.6f, 0.0f, HALF_PI);
}

/* pile60 for three agents at once: two throwers on its south and north sides, each with a ball
   in reach, and two players west of it on the line a runner takes through its middle. */
static void build_pile60_four(body_sync_scene_t *s) {
    body_sync_scene_reset(s, "pile60_four");
    add_floor(s);
    add_pile60(s);
    add_sphere(s, "ball_s", 0.15f, 2.0f, 2.0f, 0.15f, -1.8f, BODY_SYNC_BODY_GRABBABLE, 5);
    add_sphere(s, "ball_n", 0.15f, 2.0f, 2.0f, 0.15f, 1.8f, BODY_SYNC_BODY_GRABBABLE, 5);
    add_player(s, -3.0f, -1.5f, HALF_PI);
    add_player(s, 2.0f, -2.7f, 0.0f);
    add_player(s, 2.0f, 2.7f, 2.0f * HALF_PI);
    add_player(s, -3.0f, 1.5f, HALF_PI);
}

static void build_cabinet_mug(body_sync_scene_t *s) {
    body_sync_scene_reset(s, "cabinet_mug");
    add_floor(s);
    add_box(s, "cabinet", 0.5f, 1.0f, 0.3f, 80.0f, 2.0f, 1.0f, 0.0f, BODY_SYNC_BODY_GRABBABLE | BODY_SYNC_BODY_HEAVY, 6);
    add_box(s, "mug", 0.05f, 0.06f, 0.05f, 0.3f, 2.0f, 2.06f, 0.0f, BODY_SYNC_BODY_GRABBABLE, 7);
    add_box(s, "brick", 0.12f, 0.06f, 0.06f, 3.0f, -0.4f, 0.06f, 0.6f, BODY_SYNC_BODY_GRABBABLE, 8);
    add_player(s, -3.0f, -1.5f, HALF_PI);
    add_player(s, -1.2f, 0.6f, HALF_PI);
}

static void build_mannequin(body_sync_scene_t *s) {
    body_sync_scene_reset(s, "mannequin");
    add_floor(s);
    body_sync_route_t *route = &s->routes[s->route_count++];
    route->point_count = 2;
    route->points[0][0] = -3.0f;
    route->points[0][1] = 3.0f;
    route->points[1][0] = 3.0f;
    route->points[1][1] = 3.0f;
    route->speed = 1.2f;
    route->loop = false;
    body_sync_body_desc_t person = body_sync_body_default(BODY_SYNC_SHAPE_CAPSULE, (const float[3]){0.3f, 0.55f, 0.0f}, 60.0f,
                                                          (const float[3]){2.0f, PLAYER_Y, 0.0f});
    person.role = BODY_SYNC_ROLE_MANNEQUIN;
    person.model_tag = 9;
    snprintf(person.name, sizeof(person.name), "dummy");
    body_sync_scene_add(s, &person);
    person.position[0] = -3.0f;
    person.position[2] = 3.0f;
    person.motion = BODY_SYNC_MOTION_KINEMATIC;
    person.route = 0;
    snprintf(person.name, sizeof(person.name), "walker");
    body_sync_scene_add(s, &person);
    add_player(s, -1.0f, 1.8f, HALF_PI);
    add_player(s, 0.6f, 0.0f, HALF_PI);
}

static void build_two_chars_item(body_sync_scene_t *s) {
    body_sync_scene_reset(s, "two_chars_item");
    add_floor(s);
    add_box(s, "box", 0.2f, 0.2f, 0.2f, 2.0f, 0.0f, 0.2f, 1.5f, BODY_SYNC_BODY_GRABBABLE, 3);
    add_player(s, -2.0f, 0.0f, HALF_PI);
    add_player(s, 2.0f, 0.0f, -HALF_PI);
}

/* A pass by throw between guests: the thrower beside the box, two catchers either side of the line
   it flies along, each within reach of that line and clear of the box, and the host's player well
   off it, since the host's touch would take the flight back. */
static void build_four_chars_item(body_sync_scene_t *s) {
    body_sync_scene_reset(s, "four_chars_item");
    add_floor(s);
    add_box(s, "box", 0.2f, 0.2f, 0.2f, 2.0f, -1.6f, 0.2f, 0.0f, BODY_SYNC_BODY_GRABBABLE, 3);
    add_player(s, 0.0f, -5.0f, 0.0f);
    add_player(s, -2.6f, 0.0f, HALF_PI);
    add_player(s, 2.2f, -1.0f, -HALF_PI);
    add_player(s, 2.2f, 1.0f, -HALF_PI);
}

static void build_breakables(body_sync_scene_t *s) {
    body_sync_scene_reset(s, "breakables");
    add_floor(s);
    body_sync_body_desc_t t = body_sync_body_default(BODY_SYNC_SHAPE_BOX, (const float[3]){0.06f, 0.06f, 0.015f}, 0.1f, NULL);
    snprintf(t.name, sizeof(t.name), "shard");
    t.model_tag = 20;
    s->templates[s->template_count++] = t;
    t = body_sync_body_default(BODY_SYNC_SHAPE_SPHERE, (const float[3]){0.06f, 0.0f, 0.0f}, 0.2f, NULL);
    snprintf(t.name, sizeof(t.name), "coin");
    t.model_tag = 21;
    s->templates[s->template_count++] = t;
    t = body_sync_body_default(BODY_SYNC_SHAPE_BOX, (const float[3]){0.05f, 0.05f, 0.05f}, 0.2f, (const float[3]){0.12f, 0.0f, 0.0f});
    snprintf(t.name, sizeof(t.name), "gem");
    t.model_tag = 22;
    s->templates[s->template_count++] = t;
    t = body_sync_body_default(BODY_SYNC_SHAPE_BOX, (const float[3]){0.25f, 0.03f, 0.05f}, 0.3f, NULL);
    snprintf(t.name, sizeof(t.name), "plank");
    t.model_tag = 23;
    s->templates[s->template_count++] = t;

    uint32_t glass = add_box(s, "glass", 0.03f, 0.6f, 0.4f, 2.0f, 3.0f, 0.6f, 0.0f, BODY_SYNC_BODY_PUNCH_BREAKS, 10);
    s->bodies[glass].role = BODY_SYNC_ROLE_BREAKABLE;
    s->bodies[glass].debris_template = 0;
    s->bodies[glass].debris_count = 6;
    s->bodies[glass].debris_speed = 2.0f;
    s->bodies[glass].break_speed = 3.0f;
    uint32_t crate = add_box(s, "crate", 0.3f, 0.3f, 0.3f, 6.0f, -2.0f, 0.3f, 1.5f, BODY_SYNC_BODY_PUNCH_BREAKS, 11);
    s->bodies[crate].role = BODY_SYNC_ROLE_BREAKABLE;
    s->bodies[crate].contents_first = 1;
    s->bodies[crate].contents_count = 2;
    s->bodies[crate].debris_template = 3;
    s->bodies[crate].debris_count = 4;
    s->bodies[crate].debris_speed = 1.5f;
    s->bodies[crate].break_speed = 6.0f;
    add_sphere(s, "rock", 0.12f, 2.0f, 0.8f, 0.12f, 0.0f, BODY_SYNC_BODY_GRABBABLE, 12);
    add_player(s, -2.0f, 0.3f, 0.0f);
    add_player(s, 0.2f, 0.0f, HALF_PI);
}

static void build_load300(body_sync_scene_t *s) {
    body_sync_scene_reset(s, "load300");
    add_floor(s);
    int n = 0;
    for (int y = 0; y < 3; y++) {
        for (int x = 0; x < 10; x++) {
            for (int z = 0; z < 10; z++) {
                char name[BODY_SYNC_NAME_MAX];
                snprintf(name, sizeof(name), "load%d", n++);
                add_box(s, name, 0.15f, 0.15f, 0.15f, 1.0f, 0.32f * ((float)x - 4.5f), 0.15f + 0.3f * (float)y, 0.32f * ((float)z - 4.5f),
                        BODY_SYNC_BODY_GRABBABLE, 4);
            }
        }
    }
    add_player(s, -4.0f, -4.0f, 0.7853982f);
    add_player(s, 4.0f, -4.0f, -0.7853982f);
    add_player(s, -4.0f, 4.0f, 2.3561945f);
    add_player(s, 4.0f, 4.0f, -2.3561945f);
}

/* A runner's lane to the wall, a light crate across it near the wall and a ball for a thrower
   north of the lane: the ball sent along the crate's width clears the lane; a runner who meets the
   crate first pins it to the wall and stops its depth (0.8 m) short. */
static void build_wall_plank(body_sync_scene_t *s) {
    body_sync_scene_reset(s, "wall_plank");
    add_floor(s);
    uint32_t wall = add_box(s, "wall", 0.25f, 1.5f, 4.0f, 0.0f, 6.0f, 1.5f, 0.0f, 0, 2);
    s->bodies[wall].motion = BODY_SYNC_MOTION_STATIC;
    add_box(s, "plank", 0.4f, 0.4f, 0.5f, 3.0f, 4.9f, 0.4f, 0.0f, BODY_SYNC_BODY_GRABBABLE, 3);
    add_sphere(s, "ball", 0.15f, 8.0f, 4.95f, 0.15f, 2.2f, BODY_SYNC_BODY_GRABBABLE, 5);
    add_player(s, -2.0f, 0.0f, HALF_PI);
    add_player(s, 4.95f, 2.9f, 2.0f * HALF_PI);
}

static uint32_t add_mannequin(body_sync_scene_t *s, const char *name, float x, float z, uint8_t route) {
    body_sync_body_desc_t person = body_sync_body_default(BODY_SYNC_SHAPE_CAPSULE, (const float[3]){0.3f, 0.55f, 0.0f}, 60.0f,
                                                          (const float[3]){x, PLAYER_Y, z});
    person.role = BODY_SYNC_ROLE_MANNEQUIN;
    person.model_tag = 9;
    /* Written out, not left to the default: the throw scenarios' checks are derived from it (a
       thrown rock at 9 m/s fells it, a lob at 3.5 m/s does not). */
    person.fall_speed = 5.0f;
    if (route != BODY_SYNC_ROUTE_NONE) {
        person.motion = BODY_SYNC_MOTION_KINEMATIC;
        person.route = route;
    }
    snprintf(person.name, sizeof(person.name), "%s", name);
    return body_sync_scene_add(s, &person);
}

static void add_rock(body_sync_scene_t *s, const char *name, float x, float z) {
    add_sphere(s, name, 0.12f, 2.0f, x, 0.12f, z, BODY_SYNC_BODY_GRABBABLE, 12);
}

/* Throws into the wall and into a standing dummy: the thrower's own body flies with no delay, and
   the dummy falls for everyone by the thrower's impact claim. The dummy stands off the wall's
   line so a rock coming back off the wall does not reach it. */
static void build_dummy_wall(body_sync_scene_t *s) {
    body_sync_scene_reset(s, "dummy_wall");
    add_floor(s);
    uint32_t wall = add_box(s, "wall", 0.25f, 1.5f, 4.0f, 0.0f, 6.0f, 1.5f, 0.0f, 0, 2);
    s->bodies[wall].motion = BODY_SYNC_MOTION_STATIC;
    add_mannequin(s, "dummy", 3.0f, -1.8f, BODY_SYNC_ROUTE_NONE);
    add_rock(s, "rock_a", 0.5f, -0.4f);
    add_rock(s, "rock_b", 0.5f, 0.4f);
    add_player(s, -3.0f, -2.5f, HALF_PI);
    add_player(s, 0.0f, 0.0f, HALF_PI);
}

/* A mannequin walking across a thrower's line at the mannequin scene's pace: a hit claim on a
   target the host moves, which the guest sees a one-way trip and a playback delay late. */
static void build_walker_lane(body_sync_scene_t *s) {
    body_sync_scene_reset(s, "walker_lane");
    add_floor(s);
    body_sync_route_t *route = &s->routes[s->route_count++];
    route->point_count = 2;
    route->points[0][0] = 2.5f;
    route->points[0][1] = -4.0f;
    route->points[1][0] = 2.5f;
    route->points[1][1] = 4.0f;
    route->speed = 1.2f;
    route->loop = false;
    add_mannequin(s, "walker", 2.5f, -4.0f, 0);
    add_rock(s, "rock", 0.5f, -0.4f);
    add_player(s, -3.0f, -2.5f, HALF_PI);
    add_player(s, 0.0f, 0.0f, HALF_PI);
}

/* Glass and a door a guest throws at. The door is the heavy case: dynamic like every breakable,
   too heavy for a guest to take, so a guest's rock that meets it returns to the host on contact
   and its release goes out before the claim. A slab 16 cm deep stands up to a rock's push that
   does not break it; it breaks at a thrown rock's speed, past a punched item's or a walk's. */
static void build_glass_door(body_sync_scene_t *s) {
    body_sync_scene_reset(s, "glass_door");
    add_floor(s);
    body_sync_body_desc_t t = body_sync_body_default(BODY_SYNC_SHAPE_BOX, (const float[3]){0.06f, 0.06f, 0.015f}, 0.1f, NULL);
    snprintf(t.name, sizeof(t.name), "shard");
    t.model_tag = 20;
    s->templates[s->template_count++] = t;
    t = body_sync_body_default(BODY_SYNC_SHAPE_BOX, (const float[3]){0.25f, 0.03f, 0.05f}, 0.3f, NULL);
    snprintf(t.name, sizeof(t.name), "plank");
    t.model_tag = 23;
    s->templates[s->template_count++] = t;

    uint32_t glass = add_box(s, "glass", 0.03f, 0.6f, 0.4f, 2.0f, 3.0f, 0.6f, -1.0f, 0, 10);
    s->bodies[glass].role = BODY_SYNC_ROLE_BREAKABLE;
    s->bodies[glass].debris_template = 0;
    s->bodies[glass].debris_count = 6;
    s->bodies[glass].debris_speed = 2.0f;
    s->bodies[glass].break_speed = 3.0f;
    uint32_t door = add_box(s, "door", 0.08f, 1.0f, 0.45f, 120.0f, 3.0f, 1.0f, 1.2f, BODY_SYNC_BODY_HEAVY, 13);
    s->bodies[door].role = BODY_SYNC_ROLE_BREAKABLE;
    s->bodies[door].debris_template = 1;
    s->bodies[door].debris_count = 6;
    s->bodies[door].debris_speed = 1.5f;
    s->bodies[door].break_speed = 6.0f;
    add_rock(s, "rock_a", 0.5f, -0.4f);
    add_rock(s, "rock_b", 0.5f, 0.4f);
    add_player(s, -2.5f, -2.5f, HALF_PI);
    add_player(s, 0.0f, 0.0f, HALF_PI);
}

static const body_sync_scene_entry_t catalog[] = {
    {"floor_wall", "floor and wall with one body: bounce, fast throw, tunnelling", build_floor_wall},
    {"stack10", "a stack of 10 boxes: rest, divergence, jitter", build_stack10},
    {"pile60", "a pile of 60 light bodies: budget, priorities, chain hits", build_pile60},
    {"cabinet_mug", "a heavy cabinet and a mug: mass when a guest's body hits the host's world", build_cabinet_mug},
    {"mannequin", "a mannequin that stands and one that walks a route: hit claims on moving targets", build_mannequin},
    {"two_chars_item", "two characters and one item: grab dispute, passing by throw", build_two_chars_item},
    {"breakables", "glass and a box with contents: spawns, removals, slot generations", build_breakables},
    {"load300", "300 bodies and four players: the load ceiling", build_load300},
    {"wall_boxes", "floor and wall, a crate and six light boxes at the wall's foot: a bounce into light bodies", build_wall_boxes},
    {"pile60_four", "pile60 with four players and two balls: two throws at once and a runner", build_pile60_four},
    {"wall_plank", "a crate across a runner's lane to a wall and a ball to clear it: a late throw leaves her debris", build_wall_plank},
    {"dummy_wall", "a wall, a standing dummy and two rocks: a guest's throw flies at once, the dummy falls by its claim", build_dummy_wall},
    {"walker_lane", "a mannequin walking across a thrower's line and a rock: an impact claim on a moving host target", build_walker_lane},
    {"glass_door", "glass, a heavy door and two rocks: breaks by a guest's impact claim, the door ending the flight", build_glass_door},
    {"four_chars_item", "four characters and one item: a guest catches another guest's throw", build_four_chars_item},
};

const body_sync_scene_entry_t *body_sync_scene_catalog(uint32_t *count) {
    if (count) {
        *count = (uint32_t)(sizeof(catalog) / sizeof(catalog[0]));
    }
    return catalog;
}
