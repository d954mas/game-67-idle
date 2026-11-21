#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "features/body_sync/body_sync_stand.h"
#include "stand_testing.h"
#include "unity.h"

/* Each check must fail on a defect planted after a clean run, or it may pass on nothing. */

void setUp(void) {}
void tearDown(void) {}

static body_sync_scene_t scene;
static body_sync_scenario_t scenario;

static const body_sync_scene_entry_t *scene_named(const char *name) {
    uint32_t count = 0;
    const body_sync_scene_entry_t *catalog = body_sync_scene_catalog(&count);
    return body_sync_scene_lookup(catalog, count, name);
}

static body_sync_stand_t *run(const char *text) {
    body_sync_error_t error;
    TEST_ASSERT_TRUE_MESSAGE(body_sync_scenario_parse(text, strlen(text), &scenario, &error), error.text);
    scene_named(scenario.scene)->build(&scene);
    body_sync_stand_config_t config = {.scene = &scene, .scenario = &scenario};
    body_sync_stand_t *stand = body_sync_stand_create(&config, &error);
    TEST_ASSERT_NOT_NULL_MESSAGE(stand, error.text);
    const body_sync_report_t *report = body_sync_stand_run(stand);
    for (uint32_t i = 0; i < report->check_count; i++) {
        TEST_ASSERT_TRUE_MESSAGE(report->checks[i].passed, report->checks[i].detail);
    }
    return stand;
}

/* The check on `line` fails after planting, and its detail says `why`. */
static void fails(body_sync_stand_t *stand, uint32_t line, const char *why) {
    const body_sync_report_t *report = body_sync_stand_report(stand);
    for (uint32_t i = 0; i < report->check_count; i++) {
        if (report->checks[i].line == line) {
            TEST_ASSERT_FALSE_MESSAGE(report->checks[i].passed, why);
            TEST_ASSERT_NOT_NULL_MESSAGE(strstr(report->checks[i].detail, why), report->checks[i].detail);
            TEST_ASSERT_FALSE(report->passed);
            return;
        }
    }
    TEST_FAIL_MESSAGE("no check on that line");
}

static const char *walk_text = "scenario planted\nscene two_chars_item\nguests 1\nnet g1 rtt 60ms\np1 walk -2 -1\n"
                               "expect agree_at_rest 5cm 5deg\nexpect no_stuck\nexpect effects_once\n";

static uint32_t free_slot(stand_world_t *w) { return stand_world_free_slot(w); }

static void test_agree_fails_on_a_body_only_a_guest_has(void) {
    body_sync_stand_t *stand = run(walk_text);
    stand_world_t *g = stand_testing_world(stand, 1);
    uint32_t slot = free_slot(g);
    const float at[3] = {3.0f, 0.2f, 3.0f}, turn[4] = {0, 0, 0, 1}, still[3] = {0, 0, 0};
    TEST_ASSERT_TRUE(stand_world_spawn(g, body_sync_id_make(slot, 1), &scene.bodies[1], at, turn, still, still));
    fails(stand, 6, "only g1 has");
    body_sync_stand_destroy(stand);
}

static void test_agree_fails_on_a_body_a_guest_lacks(void) {
    body_sync_stand_t *stand = run(walk_text);
    stand_world_t *g = stand_testing_world(stand, 1);
    body_sync_id_t box = stand_world_id(g, 1);
    body_sync_unbind(stand_testing_sync(stand, 1), box);
    stand_world_remove(g, box);
    fails(stand, 6, "g1 lacks");
    body_sync_stand_destroy(stand);
}

static void test_agree_fails_on_a_moved_copy(void) {
    body_sync_stand_t *stand = run(walk_text);
    stand_world_t *g = stand_testing_world(stand, 1);
    b3Body_SetTransform(g->slots[1].body, (b3Vec3){0.0f, 0.2f, 1.6f}, b3Quat_identity);
    fails(stand, 6, "off by");
    body_sync_stand_destroy(stand);
}

static void test_no_stuck_fails_on_a_kinematic_body_nobody_holds(void) {
    body_sync_stand_t *stand = run(walk_text);
    b3Body_SetType(stand_testing_world(stand, 0)->slots[1].body, b3_kinematicBody);
    fails(stand, 7, "kinematic with no holder");
    body_sync_stand_destroy(stand);
}

static void test_no_stuck_fails_on_a_holder_that_holds_nothing(void) {
    body_sync_stand_t *stand = run(walk_text);
    stand_world_t *h = stand_testing_world(stand, 0);
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_set_holder(stand_testing_sync(stand, 0), stand_world_id(h, 1), stand_world_id(h, 2)));
    fails(stand, 7, "names a holder that does not hold it");
    body_sync_stand_destroy(stand);
}

static void test_no_stuck_fails_on_a_body_under_a_seat_that_left(void) {
    body_sync_stand_t *stand = run("scenario planted\nscene two_chars_item\nguests 1\np1 walk box\np1 grab box as g\n"
                                   "expect result g granted\nexpect no_stuck\n");
    stand_testing_mark_left(stand, 1);
    fails(stand, 7, "under a seat that has left");
    body_sync_stand_destroy(stand);
}

/* A guest that shows one knockdown twice (the knocked bit drops and rises again) fails, as the
   host would. */
static void test_effects_fail_on_a_knock_shown_twice_on_a_guest(void) {
    const char *text = "scenario planted\nscene mannequin\nguests 1\nend 6s\np0 walk 1 0\np0 punch dummy as hit\nexpect effects_once\n";
    body_sync_error_t error;
    TEST_ASSERT_TRUE_MESSAGE(body_sync_scenario_parse(text, strlen(text), &scenario, &error), error.text);
    scene_named(scenario.scene)->build(&scene);
    body_sync_stand_config_t config = {.scene = &scene, .scenario = &scenario};
    body_sync_stand_t *stand = body_sync_stand_create(&config, &error);
    TEST_ASSERT_NOT_NULL_MESSAGE(stand, error.text);
    stand_world_t *h = stand_testing_world(stand, 0);
    body_sync_t *sync = stand_testing_sync(stand, 0);
    body_sync_id_t dummy = stand_world_id(h, 1);
    uint32_t knocked_at = 0;
    while (body_sync_stand_tick(stand)) {
        uint32_t now = body_sync_stand_now(stand);
        if (!knocked_at && h->slots[1].knocked) {
            knocked_at = now;
        }
        if (knocked_at && now == knocked_at + 60) {
            body_sync_set_flags(sync, dummy, 0);
        }
        if (knocked_at && now == knocked_at + 80) {
            body_sync_set_flags(sync, dummy, 1);
        }
    }
    TEST_ASSERT_TRUE(knocked_at > 0);
    fails(stand, 7, "shown 2 times");
    body_sync_stand_destroy(stand);
}

static void test_effects_fail_on_a_knock_applied_twice(void) {
    body_sync_stand_t *stand = run("scenario planted\nscene mannequin\nguests 1\np0 walk 1 0\np0 punch dummy as hit\n"
                                   "expect result hit hit\nexpect effects_once\n");
    stand_world_t *h = stand_testing_world(stand, 0);
    TEST_ASSERT_TRUE(stand_testing_effect(stand, STAND_EFFECT_KNOCK, stand_world_id(h, 1)));
    fails(stand, 7, "knocked 2 times by 2 effects");
    body_sync_stand_destroy(stand);
}

static const char *break_text = "scenario planted\nscene breakables\nguests 1\nnet g1 rtt 60ms\np0 punch crate as hit\n"
                                "expect result hit hit\nexpect effects_once\n";

static void test_effects_fail_on_a_broken_body_back_on_a_guest(void) {
    body_sync_stand_t *stand = run(break_text);
    stand_world_t *g = stand_testing_world(stand, 1);
    body_sync_id_t crate = body_sync_id_make(2, 1);
    TEST_ASSERT_NULL(stand_world_slot(g, crate));
    const float at[3] = {-2.0f, 0.3f, 1.5f}, turn[4] = {0, 0, 0, 1}, still[3] = {0, 0, 0};
    g->base = 0; /* plant the crate back into its scene slot */
    TEST_ASSERT_TRUE(stand_world_spawn(g, crate, &scene.bodies[2], at, turn, still, still));
    fails(stand, 7, "still there");
    body_sync_stand_destroy(stand);
}

static void test_effects_fail_on_a_spawn_missing_on_a_guest(void) {
    body_sync_stand_t *stand = run(break_text);
    stand_world_t *g = stand_testing_world(stand, 1);
    uint32_t slot = g->base;
    while (slot < STAND_BODIES_MAX && !g->slots[slot].live) {
        slot++;
    }
    TEST_ASSERT_TRUE(slot < STAND_BODIES_MAX);
    body_sync_id_t id = stand_world_id(g, slot);
    body_sync_unbind(stand_testing_sync(stand, 1), id);
    stand_world_remove(g, id);
    fails(stand, 7, "not there exactly once");
    body_sync_stand_destroy(stand);
}

static void test_effects_fail_on_a_break_that_lost_its_spawns(void) {
    body_sync_stand_t *stand = run("scenario planted\nscene breakables\nguests 1\np0 wait 10t\nexpect effects_once\n");
    stand_world_t *h = stand_testing_world(stand, 0);
    const float turn[4] = {0, 0, 0, 1}, still[3] = {0, 0, 0};
    for (uint32_t slot = stand_world_free_slot(h); slot != UINT32_MAX; slot = stand_world_free_slot(h)) {
        const float at[3] = {10.0f + (float)(slot % 20), 0.5f, (float)(slot / 20) - 25.0f};
        TEST_ASSERT_TRUE(stand_world_spawn(h, body_sync_id_make(slot, 1), &scene.templates[0], at, turn, still, still));
    }
    TEST_ASSERT_TRUE(stand_testing_effect(stand, STAND_EFFECT_BREAK, body_sync_id_make(2, 1)));
    fails(stand, 5, "lost");
    body_sync_stand_destroy(stand);
}

/* The hash sees every part it names: the transform, velocities, type, locks and filter. */
static void test_the_world_hash_sees_what_it_names(void) {
    scene_named("floor_wall")->build(&scene);
    stand_world_t *a = malloc(sizeof(*a)), *b = malloc(sizeof(*b));
    body_sync_error_t error;
    for (int change = 0; change < 6; change++) {
        TEST_ASSERT_TRUE(stand_world_build(a, &scene, 2, &error));
        TEST_ASSERT_TRUE(stand_world_build(b, &scene, 2, &error));
        /* Both awake first, so a change is seen by its own part of the hash, not the awake bit. */
        stand_world_wake_all(a);
        stand_world_wake_all(b);
        TEST_ASSERT_EQUAL_UINT64(stand_world_hash(a), stand_world_hash(b));
        b3BodyId crate = b->slots[2].body;
        switch (change) {
        case 0:
            b3Body_SetTransform(crate, (b3Vec3){1.0f, 0.2f, 0.001f}, b3Quat_identity);
            break;
        case 1:
            b3Body_SetLinearVelocity(crate, (b3Vec3){0.0f, 0.0f, 0.01f});
            break;
        case 2:
            b3Body_SetAngularVelocity(crate, (b3Vec3){0.0f, 0.01f, 0.0f});
            break;
        case 3:
            b3Body_SetType(crate, b3_kinematicBody);
            break;
        case 4:
            b3Body_SetMotionLocks(crate, (b3MotionLocks){.angularX = true});
            break;
        case 5:
            stand_world_set_held(b, 2, true);
            break;
        }
        char message[64];
        snprintf(message, sizeof(message), "change %d left the hash equal", change);
        TEST_ASSERT_TRUE_MESSAGE(stand_world_hash(a) != stand_world_hash(b), message);
        stand_world_destroy(a);
        stand_world_destroy(b);
    }
    TEST_ASSERT_TRUE(stand_world_build(a, &scene, 2, &error));
    scene_named("stack10")->build(&scene);
    TEST_ASSERT_TRUE(stand_world_build(b, &scene, 2, &error));
    TEST_ASSERT_TRUE(stand_world_hash(a) != stand_world_hash(b));
    stand_world_destroy(a);
    stand_world_destroy(b);
    free(a);
    free(b);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_agree_fails_on_a_body_only_a_guest_has);
    RUN_TEST(test_agree_fails_on_a_body_a_guest_lacks);
    RUN_TEST(test_agree_fails_on_a_moved_copy);
    RUN_TEST(test_no_stuck_fails_on_a_kinematic_body_nobody_holds);
    RUN_TEST(test_no_stuck_fails_on_a_holder_that_holds_nothing);
    RUN_TEST(test_no_stuck_fails_on_a_body_under_a_seat_that_left);
    RUN_TEST(test_effects_fail_on_a_knock_shown_twice_on_a_guest);
    RUN_TEST(test_effects_fail_on_a_knock_applied_twice);
    RUN_TEST(test_effects_fail_on_a_broken_body_back_on_a_guest);
    RUN_TEST(test_effects_fail_on_a_spawn_missing_on_a_guest);
    RUN_TEST(test_effects_fail_on_a_break_that_lost_its_spawns);
    RUN_TEST(test_the_world_hash_sees_what_it_names);
    return UNITY_END();
}
