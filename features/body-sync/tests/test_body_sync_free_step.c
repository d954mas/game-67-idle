#include <stdio.h>
#include <stdlib.h>

#include "stand_world.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

/* Two worlds from one scene, every body awake and the characters pushed, stepped with no sync:
   Box3D must keep them equal to the bit, or no divergence the stand reports means anything. */
static void test_worlds_stay_bit_identical_over_free_steps(void) {
    uint32_t count = 0;
    const body_sync_scene_entry_t *catalog = body_sync_scene_catalog(&count);
    body_sync_scene_t *scene = malloc(sizeof(*scene));
    stand_world_t *a = malloc(sizeof(*a)), *b = malloc(sizeof(*b));
    for (uint32_t i = 0; i < count; i++) {
        catalog[i].build(scene);
        body_sync_error_t error;
        TEST_ASSERT_TRUE(stand_world_build(a, scene, BODY_SYNC_PLAYERS_MAX, &error));
        TEST_ASSERT_TRUE(stand_world_build(b, scene, BODY_SYNC_PLAYERS_MAX, &error));
        stand_world_t *both[2] = {a, b};
        for (int w = 0; w < 2; w++) {
            stand_world_wake_all(both[w]);
            for (uint32_t p = 0; p < scene->player_count; p++) {
                uint32_t slot = scene->body_count + p;
                b3Body_SetLinearVelocity(both[w]->slots[slot].body, (b3Vec3){2.0f, 0.0f, 1.0f});
            }
        }
        for (int tick = 0; tick < 180; tick++) {
            stand_world_step(a);
            stand_world_step(b);
            if (stand_world_hash(a) != stand_world_hash(b)) {
                char message[128];
                snprintf(message, sizeof(message), "%s: worlds differ at tick %d", catalog[i].name, tick);
                TEST_FAIL_MESSAGE(message);
            }
        }
        stand_world_destroy(a);
        stand_world_destroy(b);
    }
    free(a);
    free(b);
    free(scene);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_worlds_stay_bit_identical_over_free_steps);
    return UNITY_END();
}
