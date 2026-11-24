#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "features/body_sync/body_sync.h"
#include "features/body_sync/body_sync_stand.h"
#include "stand_logic.h"
#include "stand_testing.h"
#include "unity.h"

/* Impact claims end to end on the stand: an honest throw's claim applies once under both
   authority modes, a forged or repeated one applies nothing, and a new throw at a target that
   survived claims again. The planted limits (--plant-claim-approach-scale0,
   --plant-claim-reach-unbounded) must fail the cases tests/CMakeLists.txt names. */

void setUp(void) {}
void tearDown(void) {}

static float plant_reach = 1.0f, plant_approach = 1.0f;
static body_sync_scene_t scene;
static body_sync_scenario_t scenario;

enum { MODE_HOST, MODE_INTERACTION };

/* The mannequin scene with a rock beside each player: what a throw at the dummy needs. */
static void add_rocks(body_sync_scene_t *s) {
    const float at[2][3] = {{0.6f, 0.12f, -0.7f}, {-1.0f, 0.12f, 1.1f}};
    const char *names[2] = {"rock_a", "rock_b"};
    for (int i = 0; i < 2; i++) {
        body_sync_body_desc_t rock = body_sync_body_default(BODY_SYNC_SHAPE_SPHERE, (const float[3]){0.12f, 0.0f, 0.0f}, 2.0f, at[i]);
        snprintf(rock.name, sizeof(rock.name), "%s", names[i]);
        rock.flags = BODY_SYNC_BODY_GRABBABLE;
        TEST_ASSERT_TRUE(body_sync_scene_add(s, &rock) != UINT32_MAX);
    }
}

static body_sync_stand_t *make(const char *text, int mode) {
    body_sync_error_t error;
    TEST_ASSERT_TRUE_MESSAGE(body_sync_scenario_parse(text, strlen(text), &scenario, &error), error.text);
    uint32_t count = 0;
    const body_sync_scene_entry_t *catalog = body_sync_scene_catalog(&count);
    const body_sync_scene_entry_t *entry = body_sync_scene_lookup(catalog, count, scenario.scene);
    TEST_ASSERT_NOT_NULL(entry);
    entry->build(&scene);
    if (strcmp(scenario.scene, "mannequin") == 0) {
        add_rocks(&scene);
    }
    body_sync_stand_config_t config = {.scene = &scene, .scenario = &scenario};
    if (mode == MODE_INTERACTION) {
        config.override_authority = true;
        config.authority = BODY_SYNC_AUTHORITY_INTERACTION;
        config.override_light_mass = true;
        config.light_mass_max = 20.0f;
    }
    body_sync_stand_t *stand = body_sync_stand_create(&config, &error);
    TEST_ASSERT_NOT_NULL_MESSAGE(stand, error.text);
    stand_testing_plant_claim_limits(stand, plant_reach, plant_approach);
    return stand;
}

static body_sync_id_t id_of(const char *name) {
    uint32_t index = body_sync_scene_find_body(&scene, name);
    TEST_ASSERT_TRUE(index != UINT32_MAX);
    return body_sync_id_make(index, 1);
}

static const body_sync_guest_metrics_t *guest(body_sync_stand_t *stand) { return &body_sync_stand_report(stand)->guests[0]; }

static void all_pass(const body_sync_report_t *report) {
    for (uint32_t i = 0; i < report->check_count; i++) {
        TEST_ASSERT_TRUE_MESSAGE(report->checks[i].passed, report->checks[i].detail);
    }
}

static bool knocked_on_host(body_sync_stand_t *stand, const char *name) {
    const stand_slot_t *s = stand_world_slot(stand_testing_world(stand, 0), id_of(name));
    return s && s->knocked;
}

static bool on_host(body_sync_stand_t *stand, const char *name) { return stand_world_slot(stand_testing_world(stand, 0), id_of(name)) != NULL; }

/* Ticks until guest 1 flies `body` (local, its hold settling) and returns its contact number. */
static uint16_t tick_to_flight(body_sync_stand_t *stand, body_sync_id_t body) {
    body_sync_t *g = stand_testing_sync(stand, 1);
    for (int i = 0; i < 600; i++) {
        body_sync_authority_t a;
        uint16_t number = 0;
        if (body_sync_is_local(g, body) && body_sync_authority(g, body, &a) && a.hold == BODY_SYNC_HOLD_UNTIL_SETTLED &&
            body_sync_contact_number(g, body, &number)) {
            return number;
        }
        TEST_ASSERT_TRUE_MESSAGE(body_sync_stand_tick(stand), "the run ended before the throw flew");
    }
    TEST_FAIL_MESSAGE("no flight");
    return 0;
}

static void send_claim(body_sync_stand_t *stand, body_sync_id_t agent, uint16_t number, body_sync_id_t target, float speed) {
    stand_record_t r = {.type = STAND_REC_IMPACT, .target = target, .body = agent, .number = number, .speed = speed};
    uint8_t bytes[STAND_RECORD_MAX];
    uint32_t size = stand_record_encode(&r, bytes, sizeof(bytes));
    TEST_ASSERT_TRUE(size > 0);
    TEST_ASSERT_TRUE(stand_testing_record(stand, 1, bytes, size));
}

static const char *glass_text = "scenario claims\nscene breakables\nseed 17\nguests 1\nsettle 10s\nnet g1 rtt 100ms jitter 20ms loss 2%\n"
                                "p1 grab rock as lift\np1 wait 400ms\np1 throw glass speed 7 as throw\n"
                                "expect result lift granted\nexpect effects_once\nexpect no_stuck\n";

static void guest_throw_breaks_glass(int mode) {
    body_sync_stand_t *stand = make(glass_text, mode);
    const body_sync_report_t *report = body_sync_stand_run(stand);
    all_pass(report);
    TEST_ASSERT_FALSE_MESSAGE(on_host(stand, "glass"), "the glass survived an honest throw");
    TEST_ASSERT_TRUE(report->guests[0].claim_effects >= 1);
    TEST_ASSERT_EQUAL_UINT32(0, report->guests[0].claims_format);
    body_sync_stand_destroy(stand);
}
static void test_guest_throw_breaks_glass_host(void) { guest_throw_breaks_glass(MODE_HOST); }
static void test_guest_throw_breaks_glass_interaction(void) { guest_throw_breaks_glass(MODE_INTERACTION); }

static const char *dummy_text = "scenario claims\nscene mannequin\nseed 13\nguests 1\nend 6s\nnet g1 rtt 120ms jitter 20ms loss 2%\n"
                                "p1 grab rock_a as lift\np1 wait 300ms\np1 throw dummy speed 9 as throw\n"
                                "expect result lift granted\nexpect result throw done\nexpect effects_once\nexpect no_stuck\n";

static void guest_throw_knocks_dummy(int mode) {
    body_sync_stand_t *stand = make(dummy_text, mode);
    const body_sync_report_t *report = body_sync_stand_run(stand);
    all_pass(report);
    TEST_ASSERT_TRUE_MESSAGE(knocked_on_host(stand, "dummy"), "a thrown rock did not fell the dummy");
    TEST_ASSERT_EQUAL_UINT32(1, report->guests[0].claim_effects);
    body_sync_stand_destroy(stand);
}
static void test_guest_throw_knocks_dummy_host(void) { guest_throw_knocks_dummy(MODE_HOST); }
static void test_guest_throw_knocks_dummy_interaction(void) { guest_throw_knocks_dummy(MODE_INTERACTION); }

/* The host's own throw is judged on the host by the same rule: no claim crosses. */
static void test_host_throw_knocks_dummy(void) {
    body_sync_stand_t *stand = make("scenario claims\nscene mannequin\nseed 13\nguests 1\nend 6s\nnet g1 rtt 120ms\n"
                                    "p0 grab rock_b as lift\np0 wait 300ms\np0 throw dummy speed 9 as throw\n"
                                    "expect result throw done\nexpect effects_once\nexpect no_stuck\n",
                                    MODE_HOST);
    const body_sync_report_t *report = body_sync_stand_run(stand);
    all_pass(report);
    TEST_ASSERT_TRUE(knocked_on_host(stand, "dummy"));
    TEST_ASSERT_EQUAL_UINT32(0, report->guests[0].claims);
    body_sync_stand_destroy(stand);
}

/* The first throw's claim is refused (its approach read as 0), the dummy stands; the second
   throw flies under a new number and claims it again. */
static void test_second_throw_claims_again(void) {
    body_sync_stand_t *stand = make("scenario claims\nscene mannequin\nseed 13\nguests 1\nend 12s\nnet g1 rtt 120ms jitter 20ms loss 2%\n"
                                    "p1 grab rock_a as lift\np1 wait 300ms\np1 throw dummy speed 9 as throw\np1 wait 1500ms\n"
                                    "p1 walk rock_a\np1 grab rock_a as lift2\np1 wait 300ms\np1 throw dummy speed 9 as throw2\n"
                                    "expect result lift2 granted\nexpect effects_once\nexpect no_stuck\n",
                                    MODE_HOST);
    stand_testing_plant_claim_limits(stand, plant_reach, 0.0f);
    bool refused = false;
    for (int i = 0; i < 400 && !refused; i++) {
        TEST_ASSERT_TRUE(body_sync_stand_tick(stand));
        refused = guest(stand)->claims_rejected >= 1;
    }
    TEST_ASSERT_TRUE_MESSAGE(refused, "the first throw sent no claim");
    TEST_ASSERT_FALSE(knocked_on_host(stand, "dummy"));
    stand_testing_plant_claim_limits(stand, plant_reach, plant_approach);
    const body_sync_report_t *report = body_sync_stand_run(stand);
    all_pass(report);
    TEST_ASSERT_TRUE_MESSAGE(knocked_on_host(stand, "dummy"), "the second throw did not claim the dummy");
    TEST_ASSERT_TRUE(report->guests[0].claims >= 2);
    TEST_ASSERT_EQUAL_UINT32(1, report->guests[0].claim_effects);
    body_sync_stand_destroy(stand);
}

/* A claim under the flying rock's own number, of the crate metres from it: the gate says OK
   (the seat flies the rock), the reach refuses it. The speed is within the approach the host
   sees, so only the reach stands between the forgery and the crate. */
static void forged_far_claim_is_rejected(int mode) {
    body_sync_stand_t *stand = make(glass_text, mode);
    body_sync_id_t rock = id_of("rock");
    uint16_t number = tick_to_flight(stand, rock);
    for (int i = 0; i < 4; i++) {
        TEST_ASSERT_TRUE(body_sync_stand_tick(stand));
    }
    send_claim(stand, rock, number, id_of("crate"), 6.5f);
    TEST_ASSERT_TRUE(body_sync_stand_tick(stand));
    TEST_ASSERT_TRUE_MESSAGE(on_host(stand, "crate"), "a forged far claim broke the crate");
    TEST_ASSERT_EQUAL_UINT32(1, guest(stand)->claims_rejected);
    const body_sync_report_t *report = body_sync_stand_run(stand);
    all_pass(report);
    TEST_ASSERT_TRUE(on_host(stand, "crate"));
    TEST_ASSERT_FALSE(on_host(stand, "glass"));
    body_sync_stand_destroy(stand);
}
static void test_forged_far_claim_is_rejected_host(void) { forged_far_claim_is_rejected(MODE_HOST); }
static void test_forged_far_claim_is_rejected_interaction(void) { forged_far_claim_is_rejected(MODE_INTERACTION); }

/* The honest claim, sent twice more under its key once it applied: counted as repeats, applied
   once. A claim the parser refuses is counted and costs the seat nothing. */
static void test_resent_claim_applies_once(void) {
    body_sync_stand_t *stand = make(glass_text, MODE_HOST);
    body_sync_id_t rock = id_of("rock"), glass = id_of("glass");
    uint16_t number = tick_to_flight(stand, rock);
    bool applied = false;
    for (int i = 0; i < 300 && !applied; i++) {
        TEST_ASSERT_TRUE(body_sync_stand_tick(stand));
        applied = guest(stand)->claim_effects >= 1;
    }
    TEST_ASSERT_TRUE_MESSAGE(applied, "the honest claim never applied");
    send_claim(stand, rock, number, glass, 6.5f);
    send_claim(stand, rock, number, glass, 6.5f);
    stand_record_t bad = {.type = STAND_REC_IMPACT, .target = glass, .body = rock, .number = number, .speed = NAN};
    uint8_t bytes[STAND_RECORD_MAX];
    uint32_t size = stand_record_encode(&bad, bytes, sizeof(bytes));
    TEST_ASSERT_TRUE(stand_testing_record(stand, 1, bytes, size));
    TEST_ASSERT_TRUE(body_sync_stand_tick(stand));
    const body_sync_guest_metrics_t *m = guest(stand);
    TEST_ASSERT_EQUAL_UINT32(2, m->claims_repeat);
    TEST_ASSERT_EQUAL_UINT32(1, m->claim_effects);
    TEST_ASSERT_EQUAL_UINT32(1, m->claims_format);
    const body_sync_report_t *report = body_sync_stand_run(stand);
    all_pass(report);
    TEST_ASSERT_EQUAL_UINT32(1, report->guests[0].claim_effects);
    body_sync_stand_destroy(stand);
}

int main(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--plant-claim-approach-scale0") == 0) {
            plant_approach = 0.0f;
        } else if (strcmp(argv[i], "--plant-claim-reach-unbounded") == 0) {
            plant_reach = INFINITY;
        }
    }
    UNITY_BEGIN();
    RUN_TEST(test_guest_throw_breaks_glass_host);
    RUN_TEST(test_guest_throw_breaks_glass_interaction);
    RUN_TEST(test_guest_throw_knocks_dummy_host);
    RUN_TEST(test_guest_throw_knocks_dummy_interaction);
    RUN_TEST(test_host_throw_knocks_dummy);
    RUN_TEST(test_second_throw_claims_again);
    RUN_TEST(test_forged_far_claim_is_rejected_host);
    RUN_TEST(test_forged_far_claim_is_rejected_interaction);
    RUN_TEST(test_resent_claim_applies_once);
    return UNITY_END();
}
