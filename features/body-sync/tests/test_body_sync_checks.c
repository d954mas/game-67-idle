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

/* A character's flag bits are its owner's: a guest toggling bit 0 on its own character is no
   knockdown, so effects_once does not count it. */
static void test_effects_ignore_a_characters_own_flag_bits(void) {
    const char *text = "scenario planted\nscene two_chars_item\nguests 1\nend 4s\np1 walk -2 -1\nexpect effects_once\n";
    body_sync_error_t error;
    TEST_ASSERT_TRUE_MESSAGE(body_sync_scenario_parse(text, strlen(text), &scenario, &error), error.text);
    scene_named(scenario.scene)->build(&scene);
    body_sync_stand_config_t config = {.scene = &scene, .scenario = &scenario};
    body_sync_stand_t *stand = body_sync_stand_create(&config, &error);
    TEST_ASSERT_NOT_NULL_MESSAGE(stand, error.text);
    body_sync_t *sync = stand_testing_sync(stand, 1);
    body_sync_id_t me = stand_world_id(stand_testing_world(stand, 1), scene.body_count + 1u);
    while (body_sync_stand_tick(stand)) {
        uint32_t now = body_sync_stand_now(stand);
        if (now % 20u == 0) {
            body_sync_set_flags(sync, me, (uint8_t)(body_sync_flags(sync, me) ^ 1u));
        }
    }
    const body_sync_report_t *report = body_sync_stand_report(stand);
    TEST_ASSERT_EQUAL_UINT32(1, report->check_count);
    TEST_ASSERT_TRUE_MESSAGE(report->checks[0].passed, report->checks[0].detail);
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
            stand_world_set_held(b, 2, true, 0);
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

/* The lag-aligned gap sees a sync that shows its owner a tick early and a guest that draws its own
   bodies off what it sends: each planted from the start of a run whose bound holds without it. */
static body_sync_stand_t *run_planted(const char *text, uint32_t shift, float drift) {
    body_sync_error_t error;
    TEST_ASSERT_TRUE_MESSAGE(body_sync_scenario_parse(text, strlen(text), &scenario, &error), error.text);
    scene_named(scenario.scene)->build(&scene);
    body_sync_stand_config_t config = {.scene = &scene, .scenario = &scenario};
    body_sync_stand_t *stand = body_sync_stand_create(&config, &error);
    TEST_ASSERT_NOT_NULL_MESSAGE(stand, error.text);
    stand_testing_plant_shift(stand, shift);
    stand_testing_plant_drift(stand, drift);
    body_sync_stand_run(stand);
    return stand;
}


static uint32_t aligned_line(body_sync_stand_t *stand) {
    const body_sync_report_t *r = body_sync_stand_report(stand);
    return r->checks[r->check_count - 1].line;
}

/* Walking characters, played as copies both ways. The host's corrected copy of a walking guest
   trails its target by up to a tick of the walk (3.6 m/s: 6 cm); the bound adds a quarter. */
static const char *walk_aligned_text = "scenario planted\nscene two_chars_item\nseed 7\nguests 1\nsettle 3s\nnet g1 rtt 100ms jitter 10ms loss 2%\n"
                                       "p0 walk 2 1\np0 wait 500ms\np0 walk -2 0\np1 walk -2 -1 speed 2.5\np1 walk 2 0\n"
                                       "expect aligned_gap.max <= 7.5cm\n";
/* A pile the guest owns by interaction, which the host shows: measured p95 10 cm clean, 19 cm
   with the guest drawing its bodies 10 cm off. */
static const char *pile_aligned_text = "scenario planted\nscene pile60\nseed 52\nguests 1\nend 8s\nlight_mass 20kg\nauthority interaction\n"
                                       "net g1 rtt 100ms jitter 20ms loss 2%\np1 grab ball\np1 wait 300ms\np1 throw pile31 speed 12\n"
                                       "expect aligned_gap.p95 <= 15cm\n";

static void test_aligned_gap_fails_on_states_shown_a_tick_early(void) {
    body_sync_stand_t *clean = run_planted(walk_aligned_text, 0, 0.0f);
    body_sync_stand_t *shifted = run_planted(walk_aligned_text, 1, 0.0f);
    TEST_ASSERT_TRUE(body_sync_stand_report(clean)->passed);
    fails(shifted, aligned_line(shifted), "aligned_gap.max");
    body_sync_stand_destroy(clean);
    body_sync_stand_destroy(shifted);
}

static void test_aligned_gap_fails_on_a_guest_drawing_its_bodies_off_what_it_sends(void) {
    body_sync_stand_t *clean = run_planted(pile_aligned_text, 0, 0.0f);
    body_sync_stand_t *drifted = run_planted(pile_aligned_text, 0, 0.1f);
    TEST_ASSERT_TRUE(body_sync_stand_report(clean)->passed);
    /* The pile is the host's and the guest predicts it: its pulled copies are in the gap too, or a
       prediction that strays once its pause is over is measured by nothing. */
    TEST_ASSERT_TRUE_MESSAGE(body_sync_stand_report(clean)->guests[0].aligned_predicted_samples > 0u, "no predicted copy was measured");
    fails(drifted, aligned_line(drifted), "aligned_gap.p95");
    body_sync_stand_destroy(clean);
    body_sync_stand_destroy(drifted);
}

/* A predicted copy in its pause is the guest's own answer to its touch, drawn ahead of the tick it
   plays: the lead leaves aligned_gap as it was and shows in prediction_gap. The pile is the host's,
   so the guest predicts it, and the ball's hit pauses copies at rest. The plant moves no body, so
   the gap equals the clean run's to the bit. A 12.5 m lead puts a sample past 7 m: the unplanted
   copy lies within 5 m of the host's picture (the snap's 1 m, its offset's 2 m and the host's own
   offset's 2 m), and the clean run stays under 7 m. */
static const char *host_pile_text = "scenario planted\nscene pile60\nseed 52\nguests 1\nend 6s\nlight_mass 20kg\nauthority host\n"
                                    "net g1 rtt 100ms jitter 20ms loss 2%\np1 grab ball\np1 wait 300ms\np1 throw pile31 speed 12\n";

static void test_a_paused_predictions_lead_stays_out_of_aligned_gap(void) {
    body_sync_stand_t *clean = run_planted(host_pile_text, 0, 0.0f);
    body_sync_error_t error;
    TEST_ASSERT_TRUE_MESSAGE(body_sync_scenario_parse(host_pile_text, strlen(host_pile_text), &scenario, &error), error.text);
    scene_named(scenario.scene)->build(&scene);
    body_sync_stand_config_t config = {.scene = &scene, .scenario = &scenario};
    body_sync_stand_t *led = body_sync_stand_create(&config, &error);
    TEST_ASSERT_NOT_NULL_MESSAGE(led, error.text);
    stand_testing_plant_lead(led, 12.5f);
    body_sync_stand_run(led);
    const body_sync_guest_metrics_t *c = &body_sync_stand_report(clean)->guests[0], *l = &body_sync_stand_report(led)->guests[0];
    TEST_ASSERT_TRUE_MESSAGE(c->aligned_samples > 0, "no aligned sample: the case is not met");
    TEST_ASSERT_TRUE_MESSAGE(c->aligned_gap_max_m == l->aligned_gap_max_m && c->aligned_gap_p95_m == l->aligned_gap_p95_m, "a paused prediction's lead raised aligned_gap");
    TEST_ASSERT_TRUE_MESSAGE(l->prediction_gap_max_m > 7.0, "a paused prediction's lead from rest is missing from prediction_gap");
    TEST_ASSERT_TRUE_MESSAGE(c->prediction_gap_max_m < 7.0, "the clean run already reads the lead");
    body_sync_stand_destroy(clean);
    body_sync_stand_destroy(led);
}

/* A move of a body the guest simulates that its own step did not make, and a copy knocked off
   its rest once the world rested: each planted at the end of a tick of a run whose bound holds
   without it. */
static body_sync_stand_t *run_jolted(const char *text, uint32_t tick, float metres, bool local) {
    body_sync_error_t error;
    TEST_ASSERT_TRUE_MESSAGE(body_sync_scenario_parse(text, strlen(text), &scenario, &error), error.text);
    scene_named(scenario.scene)->build(&scene);
    body_sync_stand_config_t config = {.scene = &scene, .scenario = &scenario};
    body_sync_stand_t *stand = body_sync_stand_create(&config, &error);
    TEST_ASSERT_NOT_NULL_MESSAGE(stand, error.text);
    stand_testing_plant_jolt(stand, tick, metres, local);
    body_sync_stand_run(stand);
    return stand;
}

static const char *owned_pile_text = "scenario planted\nscene pile60\nseed 52\nguests 1\nend 6s\nlight_mass 20kg\nauthority interaction\n"
                                     "net g1 rtt 100ms jitter 20ms loss 2%\np1 grab ball\np1 wait 300ms\np1 throw pile31 speed 12\n"
                                     "expect local_correction.max <= 1cm\n";
static const char *resting_pile_text = "scenario planted\nscene pile60\nseed 52\nguests 1\nend 6s\nlight_mass 20kg\n"
                                       "net g1 rtt 100ms jitter 20ms loss 2%\np1 wait 100ms\nexpect rest_breaks.count <= 0\n";

static void test_local_correction_fails_on_a_seam_move_of_a_local_body(void) {
    body_sync_stand_t *clean = run_jolted(owned_pile_text, 0, 0.0f, true);
    body_sync_stand_t *jolted = run_jolted(owned_pile_text, 60, 0.05f, true);
    TEST_ASSERT_TRUE(body_sync_stand_report(clean)->passed);
    fails(jolted, aligned_line(jolted), "local_correction.max");
    body_sync_stand_destroy(clean);
    body_sync_stand_destroy(jolted);
}

static void test_rest_breaks_fails_on_a_copy_knocked_after_rest(void) {
    body_sync_stand_t *clean = run_jolted(resting_pile_text, 0, 0.0f, false);
    body_sync_stand_t *jolted = run_jolted(resting_pile_text, 240, 0.05f, false);
    TEST_ASSERT_TRUE(body_sync_stand_report(clean)->passed);
    fails(jolted, aligned_line(jolted), "rest_breaks.count");
    body_sync_stand_destroy(clean);
    body_sync_stand_destroy(jolted);
}

/* A held copy of a pile kept awake by the heroine standing in it, churned back and forth by 4 mm
   every other tick: each return is under one tick's snap (a step of the copy grid closed at
   once), but kept up it is motion, and the driven body's rest window must see it. One such
   return alone is the grid's step and rests. */
static const char *standing_pile_text = "scenario planted\nscene pile60\nseed 53\nguests 1\nend 14s\nlight_mass 20kg\n"
                                        "net g1 rtt 100ms jitter 20ms loss 2%\np1 walk 0.5 0\np1 walk 3.6 0 speed 2.5\nexpect rest_breaks.count <= 0\n";
static void test_rest_breaks_fails_on_a_held_copy_churned_in_steps(void) {
    body_sync_error_t error;
    body_sync_stand_t *runs[2];
    for (int k = 0; k < 2; k++) {
        TEST_ASSERT_TRUE_MESSAGE(body_sync_scenario_parse(standing_pile_text, strlen(standing_pile_text), &scenario, &error), error.text);
        scene_named(scenario.scene)->build(&scene);
        body_sync_stand_config_t config = {.scene = &scene, .scenario = &scenario};
        runs[k] = body_sync_stand_create(&config, &error);
        TEST_ASSERT_NOT_NULL_MESSAGE(runs[k], error.text);
        stand_testing_plant_jitter(runs[k], 700, 0.004f, 2, k == 0 ? 1u : 30u);
        body_sync_stand_run(runs[k]);
    }
    TEST_ASSERT_TRUE_MESSAGE(body_sync_stand_report(runs[0])->passed, "one step's return broke the rest");
    fails(runs[1], aligned_line(runs[1]), "rest_breaks.count");
    body_sync_stand_destroy(runs[0]);
    body_sync_stand_destroy(runs[1]);
}

/* A guest walks through the pile, its boxes the host's copies under the second layer; her copy of
   a box in her path held kinematic is a stall the sync added (the local reference pushes it
   aside), and the pace check must see it. */
static const char *pace_text = "scenario planted\nscene pile60\nseed 53\nguests 1\nsettle 5s\nlight_mass 20kg\n"
                               "net g1 rtt 100ms jitter 20ms loss 2%\np1 walk 0.5 0\np1 walk 3.6 0 speed 2.5 as run\nexpect pace run\n";
static void test_pace_fails_on_a_copy_held_in_the_walkers_path(void) {
    body_sync_error_t error;
    body_sync_stand_t *runs[2];
    for (int k = 0; k < 2; k++) {
        TEST_ASSERT_TRUE_MESSAGE(body_sync_scenario_parse(pace_text, strlen(pace_text), &scenario, &error), error.text);
        scene_named(scenario.scene)->build(&scene);
        body_sync_stand_config_t config = {.scene = &scene, .scenario = &scenario};
        runs[k] = body_sync_stand_create(&config, &error);
        TEST_ASSERT_NOT_NULL_MESSAGE(runs[k], error.text);
        /* The pile's near face, two boxes high across her path: pile0..3 and pile20..23. */
        for (int b = 0; k == 1 && b < 8; b++) {
            char name[16];
            snprintf(name, sizeof(name), "pile%d", b < 4 ? b : 16 + b);
            stand_testing_plant_wall(runs[k], 1, name);
        }
        body_sync_stand_run(runs[k]);
    }
    const body_sync_report_t *clean = body_sync_stand_report(runs[0]);
    TEST_ASSERT_TRUE_MESSAGE(clean->passed, clean->checks[0].detail);
    fails(runs[1], aligned_line(runs[1]), "behind the reference");
    body_sync_stand_destroy(runs[0]);
    body_sync_stand_destroy(runs[1]);
}

/* The host runs (4 m/s from x -2) to x 5.6, 0.15 m beyond what the wall at 5.75 lets her
   capsule (0.3 m) reach, so on time she arrives within the 0.2 m of a walk. She meets the crate
   (x 4.5..5.3) about 1.6 s into the walk, at 2.6 s; the guest's ball, thrown at 2.2 s over 1.8 m
   at 11.5 m/s, has knocked it out of the lane by 2.4 s. The cut holds the throw back on the host
   until after 3.0 s: she pins the crate to the wall and stops its depth short, 0.6 m or so, which
   no time lag absorbs. The lag (two one-way paths of 60 ms, 15 ticks of playback delay and the
   cut's 60) spans the throw's delay, so a reference with the guest late keeps her pace; the
   on-time reference alone fails her. */
static const char *late_throw_text = "scenario planted\nscene wall_plank\nseed 7\nguests 1\nsettle 5s\nnet g1 rtt 100ms jitter 10ms loss 2%\n"
                                     "at 2000ms cut g1 for 1s\np1 grab ball\nat 2200ms p1 throw 4.8 0.4 0.4 speed 11.5\n"
                                     "at 1000ms p0 walk 5.6 0 speed 4 as run\nexpect pace run\n";
static void test_pace_holds_a_walk_to_the_others_played_late(void) {
    body_sync_error_t error;
    body_sync_stand_t *runs[2];
    for (int k = 0; k < 2; k++) {
        TEST_ASSERT_TRUE_MESSAGE(body_sync_scenario_parse(late_throw_text, strlen(late_throw_text), &scenario, &error), error.text);
        scene_named(scenario.scene)->build(&scene);
        body_sync_stand_config_t config = {.scene = &scene, .scenario = &scenario};
        runs[k] = body_sync_stand_create(&config, &error);
        TEST_ASSERT_NOT_NULL_MESSAGE(runs[k], error.text);
        if (k == 1) {
            stand_testing_plant_pace_on_time(runs[k]);
        }
        body_sync_stand_run(runs[k]);
    }
    const body_sync_report_t *clean = body_sync_stand_report(runs[0]);
    TEST_ASSERT_TRUE_MESSAGE(clean->passed, clean->checks[0].detail);
    fails(runs[1], aligned_line(runs[1]), "behind the reference");
    body_sync_stand_destroy(runs[0]);
    body_sync_stand_destroy(runs[1]);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_pace_fails_on_a_copy_held_in_the_walkers_path);
    RUN_TEST(test_pace_holds_a_walk_to_the_others_played_late);
    RUN_TEST(test_local_correction_fails_on_a_seam_move_of_a_local_body);
    RUN_TEST(test_rest_breaks_fails_on_a_copy_knocked_after_rest);
    RUN_TEST(test_rest_breaks_fails_on_a_held_copy_churned_in_steps);
    RUN_TEST(test_aligned_gap_fails_on_states_shown_a_tick_early);
    RUN_TEST(test_aligned_gap_fails_on_a_guest_drawing_its_bodies_off_what_it_sends);
    RUN_TEST(test_a_paused_predictions_lead_stays_out_of_aligned_gap);
    RUN_TEST(test_agree_fails_on_a_body_only_a_guest_has);
    RUN_TEST(test_agree_fails_on_a_body_a_guest_lacks);
    RUN_TEST(test_agree_fails_on_a_moved_copy);
    RUN_TEST(test_no_stuck_fails_on_a_kinematic_body_nobody_holds);
    RUN_TEST(test_no_stuck_fails_on_a_holder_that_holds_nothing);
    RUN_TEST(test_no_stuck_fails_on_a_body_under_a_seat_that_left);
    RUN_TEST(test_effects_fail_on_a_knock_shown_twice_on_a_guest);
    RUN_TEST(test_effects_ignore_a_characters_own_flag_bits);
    RUN_TEST(test_effects_fail_on_a_knock_applied_twice);
    RUN_TEST(test_effects_fail_on_a_broken_body_back_on_a_guest);
    RUN_TEST(test_effects_fail_on_a_spawn_missing_on_a_guest);
    RUN_TEST(test_effects_fail_on_a_break_that_lost_its_spawns);
    RUN_TEST(test_the_world_hash_sees_what_it_names);
    return UNITY_END();
}
