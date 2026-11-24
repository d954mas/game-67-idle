#ifndef FEATURES_BODY_SYNC_BODY_SYNC_SCENE_H
#define FEATURES_BODY_SYNC_BODY_SYNC_SCENE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* A stand scene as plain data: the bodies every endpoint builds its world from, the players'
   spawn points, the test character's numbers, the routes of walking mannequins and the bodies
   a breakable leaves behind. No Box3D type appears here, so a game adds its own scenes and a
   drawer maps model tags to its models without knowing the physics.

   Y is up, metres, kilograms, seconds. Rotations are unit quaternions x, y, z, w. A scene is
   built identically on every endpoint: body i of the scene takes slot i with generation 1, and
   player p's character takes slot body_count + p. */

enum {
    BODY_SYNC_NAME_MAX = 32, /* names of scenes, scenarios and bodies, with the terminator */
    BODY_SYNC_SCENE_BODIES_MAX = 512,
    BODY_SYNC_SCENE_TEMPLATES_MAX = 16,
    BODY_SYNC_SCENE_ROUTES_MAX = 4,
    BODY_SYNC_ROUTE_POINTS_MAX = 8,
    BODY_SYNC_PLAYERS_MAX = 4, /* player p is controlled by endpoint p; p0 is the host's */
    BODY_SYNC_ROUTE_NONE = 0xFF,
    BODY_SYNC_TEMPLATE_NONE = 0xFF,
    BODY_SYNC_ERROR_TEXT_MAX = 160,
};

/* A refusal: what was wrong, and the 1-based line of the text it came from (0 when not from a
   text). Every refusal of the stand fills one; none asserts. */
typedef struct body_sync_error {
    uint32_t line;
    char text[BODY_SYNC_ERROR_TEXT_MAX];
} body_sync_error_t;

typedef enum body_sync_shape {
    BODY_SYNC_SHAPE_BOX = 1,     /* size: half extents x, y, z */
    BODY_SYNC_SHAPE_SPHERE,      /* size[0]: radius */
    BODY_SYNC_SHAPE_CAPSULE,     /* size[0]: radius, size[1]: half length of the axis segment, along Y */
} body_sync_shape_t;

typedef enum body_sync_motion {
    BODY_SYNC_MOTION_STATIC = 0,
    BODY_SYNC_MOTION_DYNAMIC,
    BODY_SYNC_MOTION_KINEMATIC, /* moved only by stand rules: a walking mannequin */
} body_sync_motion_t;

/* What the host rules of the stand do with a body. */
typedef enum body_sync_role {
    BODY_SYNC_ROLE_PROP = 0,
    BODY_SYNC_ROLE_MANNEQUIN, /* a person: a hit claim knocks it down (dynamic, rotation free) */
    BODY_SYNC_ROLE_BREAKABLE, /* a hit claim or an impact claim breaks it: removed, debris spawned */
} body_sync_role_t;

enum {
    BODY_SYNC_BODY_GRABBABLE = 1u << 0,
    BODY_SYNC_BODY_HEAVY = 1u << 1,        /* carried at heavy speed; its copy blocks a character */
    BODY_SYNC_BODY_START_AWAKE = 1u << 2,  /* dynamic bodies start asleep otherwise */
    BODY_SYNC_BODY_PUNCH_BREAKS = 1u << 3, /* breakable: a punch breaks it, not only an impact */
};

typedef struct body_sync_body_desc {
    char name[BODY_SYNC_NAME_MAX]; /* scenarios refer to it; may be empty; unique when set */
    body_sync_shape_t shape;
    float size[3];
    /* kg, > 0, or zero to take density (kg/m^3, > 0) times the shape's volume. */
    float mass;
    float density;
    float friction;    /* 0..2 */
    float restitution; /* 0..1 */
    float linear_damping;  /* 1/s, >= 0, as Box3D's body damping; 0 by default */
    float angular_damping; /* 1/s, >= 0 */
    float position[3]; /* a template's is relative to the breakable that spawns it */
    float rotation[4];
    body_sync_motion_t motion;
    body_sync_role_t role;
    uint32_t flags;     /* BODY_SYNC_BODY_* */
    uint32_t model_tag; /* opaque here: a drawer maps it to a model */
    uint8_t route;      /* mannequin: index into routes, BODY_SYNC_ROUTE_NONE stands */
    /* Breakable: templates placed at their relative poses when it breaks (the contents), then
       debris_count copies of the debris template scattered by the host's random draws inside
       the breakable's bounds, flying out at up to debris_speed. */
    uint8_t contents_first;
    uint8_t contents_count;
    uint8_t debris_template;
    uint8_t debris_count;
    float debris_speed; /* m/s */
    float break_speed;  /* m/s of an impact claim that breaks it */
    /* Mannequin: m/s of an impact that knocks it down (a thrown body, not a nudge); (0, 100]. */
    float fall_speed;
} body_sync_body_desc_t;

typedef struct body_sync_route {
    uint32_t point_count; /* 2..BODY_SYNC_ROUTE_POINTS_MAX */
    float points[BODY_SYNC_ROUTE_POINTS_MAX][2]; /* x, z on the floor */
    float speed; /* m/s, > 0 */
    bool loop;   /* false: walks back along the points */
} body_sync_route_t;

typedef struct body_sync_player_desc {
    float position[3]; /* the capsule's centre */
    float yaw;         /* radians about +Y; zero faces +Z */
    uint32_t model_tag;
} body_sync_player_desc_t;

/* The test character mirrors the game's mechanics; these are its knobs, which the game's scenes
   set to the game's own values. */
typedef struct body_sync_character_tuning {
    float radius, height, mass;   /* the dynamic capsule, rotation locked */
    float speed, heavy_speed;     /* m/s set as velocity on the floor plane */
    float reach;                  /* grab: the nearest grabbable within this of the capsule's axis */
    float hold_forward, hold_up;  /* the hold point from the capsule's centre, facing yaw */
    float throw_speed;            /* m/s when a throw names none */
    float punch_windup_s, punch_recover_s;
    float punch_range, punch_cone_deg; /* the cone query in front; no physics shape */
    float punch_push;             /* m/s given to a dynamic item that is punched */
    float knock_speed;            /* m/s a knocked mannequin is sent off with */
} body_sync_character_tuning_t;

/* The game's measured numbers at the time the stand was written. */
body_sync_character_tuning_t body_sync_character_tuning_default(void);

typedef struct body_sync_scene {
    char name[BODY_SYNC_NAME_MAX];
    body_sync_character_tuning_t character;
    /* A body outside these bounds counts as lost to the no_stuck check. */
    float bounds_min[3];
    float bounds_max[3];
    uint32_t player_count;
    body_sync_player_desc_t players[BODY_SYNC_PLAYERS_MAX];
    uint32_t body_count;
    body_sync_body_desc_t bodies[BODY_SYNC_SCENE_BODIES_MAX];
    uint32_t template_count;
    body_sync_body_desc_t templates[BODY_SYNC_SCENE_TEMPLATES_MAX];
    uint32_t route_count;
    body_sync_route_t routes[BODY_SYNC_SCENE_ROUTES_MAX];
} body_sync_scene_t;

/* Empties the scene and sets its name, the default tuning and 50 m bounds. */
void body_sync_scene_reset(body_sync_scene_t *scene, const char *name);

/* A dynamic prop of that shape with friction 0.6, restitution 0, unit rotation, no name. */
body_sync_body_desc_t body_sync_body_default(body_sync_shape_t shape, const float size[3], float mass, const float position[3]);

/* Appends a body and returns its index, or UINT32_MAX when the scene is full. */
uint32_t body_sync_scene_add(body_sync_scene_t *scene, const body_sync_body_desc_t *body);

/* True when every value is in range: counts within the limits, finite numbers, positive
   sizes and masses, rotations unit within 1e-3, names unique, route and template indices in
   range, templates neither breakable nor mannequins, static bodies plain props. False fills
   `error` with the first problem (its line is the body index plus one). A scene the stand
   builds is always validated first. */
bool body_sync_scene_validate(const body_sync_scene_t *scene, body_sync_error_t *error);

/* Index of the body named `name`, or UINT32_MAX. */
uint32_t body_sync_scene_find_body(const body_sync_scene_t *scene, const char *name);

/* ---- catalog ---- */

typedef void (*body_sync_scene_build_fn)(body_sync_scene_t *out);

typedef struct body_sync_scene_entry {
    const char *name;
    const char *summary; /* one line: what the scene tests */
    body_sync_scene_build_fn build;
} body_sync_scene_entry_t;

/* The stand's own scenes: floor_wall, stack10, pile60, cabinet_mug, mannequin, two_chars_item,
   breakables, load300. A caller adds its own by passing another table beside this one. */
const body_sync_scene_entry_t *body_sync_scene_catalog(uint32_t *count);

/* The entry named `name` in `entries`, or NULL. */
const body_sync_scene_entry_t *body_sync_scene_lookup(const body_sync_scene_entry_t *entries, uint32_t count, const char *name);

#ifdef __cplusplus
}
#endif

#endif
