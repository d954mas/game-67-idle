#ifndef BODY_SYNC_STAND_WORLD_H
#define BODY_SYNC_STAND_WORLD_H

/* One endpoint's Box3D world built from a scene, with the slot table that names its bodies,
   and the test character that acts in it. The runner owns the order of calls. */

#include <stdbool.h>
#include <stdint.h>

#include "box3d/box3d.h"
#include "features/body_sync/body_sync.h"
#include "stand_logic.h"

enum {
    STAND_CAT_STATIC = 1u << 0,
    STAND_CAT_BODY = 1u << 1,
    STAND_CAT_CHARACTER = 1u << 2,
    STAND_CAT_HELD = 1u << 3,
    STAND_HITS_MAX = 256,
    STAND_SUBSTEPS = 4,
};

typedef struct stand_slot {
    bool live;
    uint16_t generation; /* the current body's, or the last one's once removed */
    b3BodyId body;
    body_sync_body_desc_t desc; /* a character's is its capsule */
    bool character;
    uint8_t player;
    bool knocked;
    uint32_t knock_events; /* knockdowns applied to this body: effects_once wants exactly one */
    bool held_filter;
    float route_s; /* walking mannequin: metres along its route there and back */
} stand_slot_t;

typedef struct stand_hit {
    uint32_t slot_a, slot_b;
    float speed;
} stand_hit_t;

typedef struct stand_world {
    b3WorldId world;
    const body_sync_scene_t *scene;
    uint32_t base; /* the first slot a spawn may take: scene bodies and players come before */
    stand_slot_t slots[STAND_BODIES_MAX];
    uint32_t hit_count;
    stand_hit_t hits[STAND_HITS_MAX];
} stand_world_t;

/* Builds the scene's bodies and the characters of players below `players`. False with `error`
   filled when Box3D refuses a body. The scene must outlive the world. */
bool stand_world_build(stand_world_t *world, const body_sync_scene_t *scene, uint32_t players, body_sync_error_t *error);
void stand_world_destroy(stand_world_t *world);
/* One fixed step; impacts of the step land in hits. */
void stand_world_step(stand_world_t *world);
uint64_t stand_world_hash(const stand_world_t *world);

body_sync_id_t stand_world_id(const stand_world_t *world, uint32_t slot);
/* The live slot under `id` (generation included), or NULL. */
stand_slot_t *stand_world_slot(stand_world_t *world, body_sync_id_t id);
/* The lowest free slot from base, or UINT32_MAX. */
uint32_t stand_world_free_slot(const stand_world_t *world);
bool stand_world_spawn(stand_world_t *world, body_sync_id_t id, const body_sync_body_desc_t *desc, const float position[3],
                       const float rotation[4], const float velocity[3], const float angular[3]);
void stand_world_remove(stand_world_t *world, body_sync_id_t id);

void stand_world_pose(const stand_world_t *world, uint32_t slot, float position[3], float rotation[4]);
float stand_world_speed(const stand_world_t *world, uint32_t slot);
/* Carried bodies touch nothing, on every endpoint, like the game's. */
void stand_world_set_held(stand_world_t *world, uint32_t slot, bool held);
void stand_world_knock(stand_world_t *world, uint32_t slot, const float from[3], float speed);
void stand_world_push(stand_world_t *world, uint32_t slot, const float from[3], float speed);
/* Host: walking mannequins one step along their routes. */
void stand_world_walk_routes(stand_world_t *world, body_sync_t *sync);
void stand_world_wake_all(stand_world_t *world);

/* ---- the test character ---- */

typedef enum stand_char_mode {
    STAND_CHAR_IDLE = 0,
    STAND_CHAR_WALK,
    STAND_CHAR_PUNCH,
    STAND_CHAR_WAIT,
} stand_char_mode_t;

typedef enum stand_char_status {
    STAND_CHAR_BUSY = 0,
    STAND_CHAR_DONE,
    STAND_CHAR_FAILED,
    STAND_CHAR_LANDED, /* a punch reached its contact this tick; the action goes on */
} stand_char_status_t;

typedef struct stand_character {
    bool present;
    uint8_t player;
    uint32_t slot;
    float yaw;
    stand_char_mode_t mode;
    float target[2];
    body_sync_id_t walk_body; /* walk to a body: done within reach of it */
    float speed;
    float best;
    uint32_t stall;
    body_sync_id_t held;
    bool held_heavy;
    uint32_t punch_land, punch_end, now;
    body_sync_id_t punch_target;
    uint32_t wait_left;
} stand_character_t;

void stand_character_init(stand_character_t *c, const stand_world_t *world, uint8_t player);
void stand_character_walk(stand_character_t *c, const stand_world_t *world, float x, float z, body_sync_id_t body, float speed);
void stand_character_wait(stand_character_t *c, uint32_t ticks);
void stand_character_punch(stand_character_t *c, stand_world_t *world, body_sync_id_t target);
/* Stops whatever the character was doing; carrying goes on. */
void stand_character_stop(stand_character_t *c, stand_world_t *world);
/* Before the step: velocity, carrying, the punch's timing. LANDED once per punch. */
stand_char_status_t stand_character_tick(stand_character_t *c, stand_world_t *world);
/* The nearest body in the punch cone, or BODY_SYNC_ID_NONE; a named target wins when it is in. */
body_sync_id_t stand_character_punch_target(const stand_character_t *c, const stand_world_t *world);
bool stand_character_in_reach(const stand_character_t *c, const stand_world_t *world, uint32_t slot);
void stand_character_attach(stand_character_t *c, stand_world_t *world, body_sync_id_t id);
/* Lets go: `velocity` NULL drops in place. Returns the id let go. */
body_sync_id_t stand_character_release(stand_character_t *c, stand_world_t *world, const float velocity[3]);
void stand_character_position(const stand_character_t *c, const stand_world_t *world, float out[3]);

#endif
