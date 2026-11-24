#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "body_sync_plant.h"
#include "features/body_sync/body_sync.h"
#include "features/body_sync/body_sync_stand.h"
#include "stand_logic.h"
#include "stand_testing.h"
#include "stub_wire.h"
#include "unity.h"

/* Impact claims end to end on the stand: an honest throw's claim applies once under both
   authority modes, a forged or repeated one applies nothing, and a new throw at a target that
   survived claims again. The planted limits (--plant-claim-approach-scale0,
   --plant-claim-reach-unbounded) must fail the cases tests/CMakeLists.txt names. */

void setUp(void) {}
void tearDown(void) {}

static float plant_reach = 1.0f, plant_approach = 1.0f;
static bool plant_take_reach; /* --plant-take-reach-unbounded: the host grants every take's evidence */
static bool plant_grows;      /* --plant-evidence-grows-over-gaps: the host's evidence grows over its gaps */
static bool plant_sleep;      /* --plant-transform-sleep: guest 1 stops sending a body it carries asleep */
static body_sync_scene_t scene;
static body_sync_scenario_t scenario;
static bool third_player; /* the next make() gives the breakables a third player and a rock for it */

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
    if (third_player) {
        scene.players[scene.player_count++] = (body_sync_player_desc_t){.position = {0.2f, 0.87f, -1.2f}, .yaw = 1.5707963f};
        body_sync_body_desc_t rock = body_sync_body_default(BODY_SYNC_SHAPE_SPHERE, (const float[3]){0.12f, 0.0f, 0.0f}, 2.0f,
                                                            (const float[3]){0.8f, 0.12f, -1.2f});
        snprintf(rock.name, sizeof(rock.name), "rock2");
        rock.flags = BODY_SYNC_BODY_GRABBABLE;
        TEST_ASSERT_TRUE(body_sync_scene_add(&scene, &rock) != UINT32_MAX);
        third_player = false;
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
    if (plant_take_reach) {
        body_sync_plant_take_reach_unbounded(stand_testing_sync(stand, 0));
    }
    if (plant_grows) {
        body_sync_plant_evidence_grows_over_gaps(stand_testing_sync(stand, 0));
    }
    if (plant_sleep) {
        body_sync_plant_transform_sleep(stand_testing_sync(stand, 1));
    }
    return stand;
}

static body_sync_id_t id_of(const char *name) {
    uint32_t index = body_sync_scene_find_body(&scene, name);
    TEST_ASSERT_TRUE(index != UINT32_MAX);
    return body_sync_id_make(index, 1);
}

static const body_sync_guest_metrics_t *guest(body_sync_stand_t *stand) { return &body_sync_stand_report(stand)->guests[0]; }

/* Every claim a seat's host parsed lands in exactly one bucket. */
static void buckets_add_up(const body_sync_report_t *report) {
    for (uint32_t g = 0; g < report->guest_count; g++) {
        const body_sync_guest_metrics_t *m = &report->guests[g];
        TEST_ASSERT_EQUAL_UINT32(m->claims, m->claims_overflow + m->claims_repeat + m->claims_rejected + m->claims_idle + m->claim_effects);
    }
}

static void all_pass(const body_sync_report_t *report) {
    for (uint32_t i = 0; i < report->check_count; i++) {
        TEST_ASSERT_TRUE_MESSAGE(report->checks[i].passed, report->checks[i].detail);
    }
    buckets_add_up(report);
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

/* The host's measure of guest 1's claim of `agent` against `target` at the contact tick that comes
   nearest within its reach, of the guest's last 40 ticks: what a forger free to name any tick
   gets. Its tick in *tick. */
static body_sync_contact_verdict_t best_contact(body_sync_stand_t *stand, body_sync_id_t agent, uint16_t number, body_sync_id_t target,
                                                body_sync_contact_t *out, uint32_t *tick) {
    uint32_t now = stand_testing_tick(stand, 1);
    body_sync_contact_verdict_t best = BODY_SYNC_CONTACT_STALE;
    *out = (body_sync_contact_t){.gap = INFINITY};
    *tick = now;
    for (uint32_t back = 0; back <= 40; back++) {
        body_sync_contact_t c;
        body_sync_contact_verdict_t v = body_sync_check_contact(stand_testing_sync(stand, 0), 1, agent, number, target, now - back, &c);
        if (v != BODY_SYNC_CONTACT_STALE && (isinf(out->gap) || c.gap - c.reach < out->gap - out->reach)) {
            *out = c;
            *tick = now - back;
            best = v;
        }
        out->approach_max = c.approach_max > out->approach_max ? c.approach_max : out->approach_max;
    }
    return best;
}

static void send_claim_at(body_sync_stand_t *stand, body_sync_id_t agent, uint16_t number, body_sync_id_t target, float speed, uint32_t tick) {
    stand_record_t r = {.type = STAND_REC_IMPACT, .target = target, .body = agent, .number = number, .speed = speed, .tick = tick};
    uint8_t bytes[STAND_RECORD_MAX];
    uint32_t size = stand_record_encode(&r, bytes, sizeof(bytes));
    TEST_ASSERT_TRUE(size > 0);
    TEST_ASSERT_TRUE(stand_testing_record(stand, 1, bytes, size));
}

/* A claim naming guest 1's tick now as the contact's. */
static void send_claim(body_sync_stand_t *stand, body_sync_id_t agent, uint16_t number, body_sync_id_t target, float speed) {
    stand_record_t r = {.type = STAND_REC_IMPACT, .target = target, .body = agent, .number = number, .speed = speed,
                        .tick = stand_testing_tick(stand, 1)};
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

/* A guest that really flings its flying rock straight up at 40 m/s (its states then declare that
   speed, and the host sees the rock rise) still cannot claim the crate metres to its side: the
   contact must lie on the path the host's own taken states show. */
static void flung_far_claim_is_rejected(int mode) {
    body_sync_stand_t *stand = make(glass_text, mode);
    body_sync_id_t rock = id_of("rock"), crate = id_of("crate");
    uint16_t number = tick_to_flight(stand, rock);
    for (int i = 0; i < 4; i++) {
        TEST_ASSERT_TRUE(body_sync_stand_tick(stand));
    }
    b3BodyId body = stand_testing_world(stand, 1)->slots[body_sync_id_slot(rock)].body;
    b3Vec3 v = b3Body_GetLinearVelocity(body);
    b3Body_SetLinearVelocity(body, (b3Vec3){v.x, v.y + 40.0f, v.z});
    body_sync_contact_t c = {.approach_max = 0.0f};
    uint32_t tick = 0;
    for (int i = 0; i < 30 && !(c.approach_max > 20.0f); i++) {
        TEST_ASSERT_TRUE(body_sync_stand_tick(stand));
        best_contact(stand, rock, number, crate, &c, &tick);
    }
    /* The speed bound alone would pass the claim now. */
    TEST_ASSERT_TRUE_MESSAGE(c.approach_max > 20.0f, "the host did not see the fling");
    send_claim_at(stand, rock, number, crate, 6.5f, tick);
    TEST_ASSERT_TRUE(body_sync_stand_tick(stand));
    TEST_ASSERT_TRUE_MESSAGE(on_host(stand, "crate"), "a flung rock's claim broke the crate");
    TEST_ASSERT_EQUAL_UINT32(1, guest(stand)->claims_rejected);
    buckets_add_up(body_sync_stand_report(stand));
    body_sync_stand_destroy(stand);
}
static void test_flung_far_claim_is_rejected_host(void) { flung_far_claim_is_rejected(MODE_HOST); }
static void test_flung_far_claim_is_rejected_interaction(void) { flung_far_claim_is_rejected(MODE_INTERACTION); }

/* A claim of the glass the rock is about to meet, at a speed no throw the host saw gives: refused
   by the speed bound alone (the gap is within the reach then). */
static void test_inflated_claim_speed_is_rejected(void) {
    body_sync_stand_t *stand = make(glass_text, MODE_HOST);
    body_sync_id_t rock = id_of("rock"), glass = id_of("glass");
    uint16_t number = tick_to_flight(stand, rock);
    body_sync_contact_t c = {.gap = INFINITY};
    uint32_t tick = 0;
    for (int i = 0; i < 120 && !(c.gap <= c.reach); i++) {
        TEST_ASSERT_TRUE(body_sync_stand_tick(stand));
        TEST_ASSERT_EQUAL(BODY_SYNC_CONTACT_OK, best_contact(stand, rock, number, glass, &c, &tick));
    }
    TEST_ASSERT_TRUE_MESSAGE(c.gap <= c.reach, "the rock never came within reach of the glass");
    send_claim_at(stand, rock, number, glass, 60.0f, tick);
    TEST_ASSERT_TRUE(body_sync_stand_tick(stand));
    TEST_ASSERT_TRUE_MESSAGE(on_host(stand, "glass"), "a claim at 60 m/s broke the glass");
    TEST_ASSERT_EQUAL_UINT32(1, guest(stand)->claims_rejected);
    body_sync_stand_destroy(stand);
}

static const char *two_seats_text = "scenario claims\nscene breakables\nseed 17\nguests 2\nsettle 10s\n"
                                    "net g1 rtt 100ms jitter 20ms loss 2%\nnet g2 rtt 100ms jitter 20ms loss 2%\n"
                                    "p1 grab rock as lift\np1 wait 400ms\np1 throw glass speed 7 as throw\n"
                                    "p2 grab rock2 as lift2\np2 wait 400ms\np2 throw glass speed 7 as throw2\n"
                                    "expect effects_once\nexpect no_stuck\n";

/* Two seats throw at one glass: it breaks once, whichever claim the host takes first. */
static void test_two_seats_claim_one_target(void) {
    third_player = true;
    body_sync_stand_t *stand = make(two_seats_text, MODE_HOST);
    const body_sync_report_t *report = body_sync_stand_run(stand);
    all_pass(report);
    TEST_ASSERT_FALSE(on_host(stand, "glass"));
    TEST_ASSERT_EQUAL_UINT32(1, report->guests[0].claim_effects + report->guests[1].claim_effects);
    body_sync_stand_destroy(stand);
}

/* Seat 2 floods the host with claims every tick; seat 1's honest throw still breaks the glass,
   and only seat 2's overflow counts. */
static void test_a_flood_starves_only_its_own_seat(void) {
    third_player = true;
    body_sync_stand_t *stand = make("scenario claims\nscene breakables\nseed 17\nguests 2\nsettle 10s\n"
                                    "net g1 rtt 100ms jitter 20ms loss 2%\nnet g2 rtt 100ms jitter 20ms loss 2%\n"
                                    "p1 grab rock as lift\np1 wait 400ms\np1 throw glass speed 7 as throw\n",
                                    MODE_HOST);
    body_sync_id_t glass = id_of("glass"), rock2 = id_of("rock2");
    tick_to_flight(stand, id_of("rock"));
    bool applied = false;
    for (int t = 0; t < 300 && !applied; t++) {
        stand_record_t r = {.type = STAND_REC_IMPACT, .target = glass, .body = rock2, .speed = 6.5f};
        uint8_t bytes[STAND_RECORD_MAX];
        for (uint16_t n = 0; n < 300; n++) {
            r.number = (uint16_t)(t * 300 + n);
            uint32_t size = stand_record_encode(&r, bytes, sizeof(bytes));
            TEST_ASSERT_TRUE(stand_testing_record(stand, 2, bytes, size));
        }
        TEST_ASSERT_TRUE(body_sync_stand_tick(stand));
        applied = body_sync_stand_report(stand)->guests[0].claim_effects >= 1;
    }
    const body_sync_report_t *report = body_sync_stand_report(stand);
    TEST_ASSERT_TRUE_MESSAGE(applied, "seat 1's claim was starved");
    TEST_ASSERT_EQUAL_UINT32(0, report->guests[0].claims_overflow);
    TEST_ASSERT_TRUE(report->guests[1].claims_overflow > 0);
    buckets_add_up(report);
    body_sync_stand_destroy(stand);
}

/* A guest declares its character 60 m/s faster than it moves, then claims a take of the box two
   metres off: the take's evidence is what the host took of the character, which never came
   near, so the host refuses it. */
static void test_inflated_far_take_is_refused(void) {
    body_sync_stand_t *stand = make("scenario take\nscene two_chars_item\nseed 5\nguests 1\nend 6s\nnet g1 rtt 60ms\np1 wait 3s\n",
                                    MODE_INTERACTION);
    body_sync_t *host = stand_testing_sync(stand, 0);
    body_sync_id_t box = id_of("box"), me = body_sync_id_make(scene.body_count + 1u, 1);
    for (int i = 0; i < 30; i++) {
        TEST_ASSERT_TRUE(body_sync_stand_tick(stand));
    }
    const float add[3] = {60.0f, 0.0f, 0.0f};
    stand_testing_plant_declared_velocity(stand, 1, me, add, 40);
    for (int i = 0; i < 20; i++) {
        TEST_ASSERT_TRUE(body_sync_stand_tick(stand));
    }
    body_sync_traffic_t before, after;
    body_sync_traffic(host, 1, &before);
    stub_record_t take = {.type = STUB_REC_CLAIM, .id = box, .hold = BODY_SYNC_HOLD_UNTIL_REST, .sequence = 0x7001};
    uint8_t bytes[STUB_RECORD_BYTES_MAX];
    uint32_t size = stub_wire_put_record(&take, bytes, sizeof(bytes));
    TEST_ASSERT_TRUE(size > 0);
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_receive_record(host, 1, body_sync_stand_now(stand), bytes, size));
    TEST_ASSERT_TRUE(body_sync_stand_tick(stand));
    body_sync_authority_t a;
    TEST_ASSERT_TRUE(body_sync_authority(host, box, &a));
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(BODY_SYNC_SEAT_HOST, a.seat, "a take two metres off its agent was granted");
    body_sync_traffic(host, 1, &after);
    TEST_ASSERT_EQUAL_UINT32(before.take_refusals + 1u, after.take_refusals);
    body_sync_stand_destroy(stand);
}

/* A forger watches the host's measure of its flying rock against the crate every tick from the
   throw on and claims the crate the first tick the measure would take it. Under `text`'s network
   the host's evidence has gaps (a long round trip, a cut link); a gap is no evidence, so the
   measure never comes within reach and the crate stands. */
static void forger_never_fits(const char *text, int mode) {
    body_sync_stand_t *stand = make(text, mode);
    body_sync_id_t rock = id_of("rock"), crate = id_of("crate");
    uint16_t number = tick_to_flight(stand, rock);
    float best = INFINITY;
    bool sent = false;
    for (int i = 0; i < 400 && !sent && body_sync_stand_tick(stand); i++) {
        body_sync_contact_t c;
        uint32_t tick;
        if (best_contact(stand, rock, number, crate, &c, &tick) != BODY_SYNC_CONTACT_OK || isinf(c.gap)) {
            continue;
        }
        best = c.gap - c.reach < best ? c.gap - c.reach : best;
        if (c.gap <= c.reach && c.approach_max * STAND_IMPACT_SPEED_SCALE + STAND_IMPACT_SPEED_SLACK >= 6.5f) {
            send_claim_at(stand, rock, number, crate, 6.5f, tick);
            sent = true;
        }
    }
    body_sync_stand_tick(stand); /* judges a claim sent on the last tick; the run may have ended */
    char why[96];
    snprintf(why, sizeof(why), "the forger's measure came within reach (gap less reach %.2f m)", (double)best);
    TEST_ASSERT_TRUE_MESSAGE(best > 0.0f, why);
    TEST_ASSERT_TRUE_MESSAGE(on_host(stand, "crate"), "a forged claim broke the crate");
    buckets_add_up(body_sync_stand_report(stand));
    body_sync_stand_destroy(stand);
}

static const char *rtt600_text = "scenario claims\nscene breakables\nseed 17\nguests 1\nsettle 10s\nnet g1 rtt 600ms\n"
                                 "p1 grab rock as lift\np1 wait 400ms\np1 throw glass speed 7 as throw\n";
static void test_forger_at_600ms_never_fits_host(void) { forger_never_fits(rtt600_text, MODE_HOST); }
static void test_forger_at_600ms_never_fits_interaction(void) { forger_never_fits(rtt600_text, MODE_INTERACTION); }

/* The guest's link cut for 500 ms two ticks into the throw's flight. */
static void forger_after_a_cut(int mode) {
    body_sync_stand_t *stand = make(glass_text, mode);
    tick_to_flight(stand, id_of("rock"));
    uint32_t flight = body_sync_stand_now(stand);
    body_sync_stand_destroy(stand);
    char text[512];
    snprintf(text, sizeof(text),
             "scenario claims\nscene breakables\nseed 17\nguests 1\nsettle 10s\nnet g1 rtt 100ms jitter 20ms loss 2%%\n"
             "at %ums cut g1 for 500ms\np1 grab rock as lift\np1 wait 400ms\np1 throw glass speed 7 as throw\n",
             (flight + 2u) * 1000u / 60u);
    forger_never_fits(text, mode);
}
static void test_forger_after_a_cut_never_fits_host(void) { forger_after_a_cut(MODE_HOST); }
static void test_forger_after_a_cut_never_fits_interaction(void) { forger_after_a_cut(MODE_INTERACTION); }

/* A hit the host accepts on the rock guest 1 carries does nothing (the carrier moves it): the
   claim counts as idle, not as an effect. */
static void test_an_accepted_hit_that_does_nothing_is_idle(void) {
    body_sync_stand_t *stand = make(glass_text, MODE_HOST);
    body_sync_id_t rock = id_of("rock");
    body_sync_t *host = stand_testing_sync(stand, 0);
    body_sync_authority_t a = {.holder = BODY_SYNC_ID_NONE};
    for (int i = 0; i < 600 && a.holder == BODY_SYNC_ID_NONE; i++) {
        TEST_ASSERT_TRUE(body_sync_stand_tick(stand));
        TEST_ASSERT_TRUE(body_sync_authority(host, rock, &a));
    }
    TEST_ASSERT_TRUE_MESSAGE(a.holder != BODY_SYNC_ID_NONE, "the guest never carried the rock");
    const stand_slot_t *slot = stand_world_slot(stand_testing_world(stand, 0), rock);
    TEST_ASSERT_NOT_NULL(slot);
    b3Vec3 at = b3Body_GetPosition(slot->body);
    stand_record_t r = {.type = STAND_REC_HIT, .target = rock, .action = BODY_SYNC_SCENARIO_ACTIONS_MAX - 1, .origin = {at.x, at.y, at.z}};
    uint8_t bytes[STAND_RECORD_MAX];
    uint32_t size = stand_record_encode(&r, bytes, sizeof(bytes));
    TEST_ASSERT_TRUE(size > 0);
    const body_sync_guest_metrics_t before = *guest(stand);
    TEST_ASSERT_TRUE(stand_testing_record(stand, 1, bytes, size));
    TEST_ASSERT_TRUE(body_sync_stand_tick(stand));
    const body_sync_guest_metrics_t *m = guest(stand);
    TEST_ASSERT_EQUAL_UINT32(before.claims + 1u, m->claims);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(before.claims_idle + 1u, m->claims_idle, "an accepted hit that did nothing was not idle");
    TEST_ASSERT_EQUAL_UINT32(before.claim_effects, m->claim_effects);
    TEST_ASSERT_EQUAL_UINT32(before.claims_rejected, m->claims_rejected);
    buckets_add_up(body_sync_stand_report(stand));
    body_sync_stand_destroy(stand);
}

/* Guest 1 lifts the rock, waits until its rest pose is acknowledged, walks with it and throws
   it: Box3D keeps the carried rock asleep while the carry moves it by transform, and the host
   must still show it at its carrier's hold point (one tick of the walk and the copy grid). */
static void carried_rock_follows_its_carrier(int mode) {
    body_sync_stand_t *stand = make("scenario carry\nscene breakables\nseed 17\nguests 1\nsettle 10s\nnet g1 rtt 100ms jitter 20ms loss 2%\n"
                                    "p1 grab rock as lift\np1 wait 400ms\np1 walk 2.3 0\np1 throw glass speed 7 as throw\n"
                                    "expect result lift granted\nexpect carry_gap.max <= 0.15m\nexpect effects_once\n",
                                    mode);
    const body_sync_report_t *report = body_sync_stand_run(stand);
    all_pass(report);
    TEST_ASSERT_TRUE(report->guests[0].carry_ticks >= 30); /* the walk, 50 ticks measured */
    body_sync_stand_destroy(stand);
}
static void test_a_carried_rock_follows_its_carrier_host(void) { carried_rock_follows_its_carrier(MODE_HOST); }
static void test_a_carried_rock_follows_its_carrier_interaction(void) { carried_rock_follows_its_carrier(MODE_INTERACTION); }

/* Honest throws under loss and jitter: the host accepts their claims except where its states of
   the agent leave a gap at the contact (a missed effect, README "Contact claims"): of each mode's
   and walk's throws over `seeds` seeds at least `sent_min` claim, and at most `missed_max` of
   those claims miss. At 25 % loss, 80 of 89 apply over 24 seeds; the misses are states lost for
   more than a chord's spacing before the contact, and a claimed speed above the fastest step the
   host still holds. */
static void loss_sweep(const char *net, int seeds, uint32_t sent_min, uint32_t missed_max) {
    for (int mode = MODE_HOST; mode <= MODE_INTERACTION; mode++) {
        for (int walk = 0; walk < 2; walk++) {
            uint32_t claims = 0, effects = 0;
            for (int seed = 1; seed <= seeds; seed++) {
                char text[512];
                snprintf(text, sizeof(text),
                         "scenario sweep\nscene breakables\nseed %d\nguests 1\nsettle 10s\nnet g1 %s\n"
                         "p1 grab rock as lift\np1 wait 400ms\n%sp1 throw glass speed 7 as throw\nexpect result lift granted\n",
                         seed, net, walk ? "p1 walk 2.3 0\n" : "");
                body_sync_stand_t *stand = make(text, mode);
                const body_sync_report_t *report = body_sync_stand_run(stand);
                buckets_add_up(report);
                claims += report->guests[0].claims;
                effects += report->guests[0].claim_effects;
                body_sync_stand_destroy(stand);
            }
            char why[128];
            snprintf(why, sizeof(why), "%s %s walk %d: %u of %u claims applied", net, mode ? "interaction" : "host", walk, effects, claims);
            printf("%s\n", why);
            TEST_ASSERT_TRUE_MESSAGE(claims >= sent_min && effects + missed_max >= claims, why);
        }
    }
}
static void test_loss_sweep_100ms(void) { loss_sweep("rtt 100ms jitter 20ms loss 2%", 12, 11, 1); }
static void test_loss_sweep_150ms(void) { loss_sweep("rtt 150ms jitter 60ms loss 10%", 12, 11, 1); }
static void test_loss_sweep_200ms(void) { loss_sweep("rtt 200ms jitter 80ms loss 25%", 24, 20, 4); }

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
    bool sweep = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--plant-claim-approach-scale0") == 0) {
            plant_approach = 0.0f;
        } else if (strcmp(argv[i], "--plant-claim-reach-unbounded") == 0) {
            plant_reach = INFINITY;
        } else if (strcmp(argv[i], "--plant-claim-approach-unbounded") == 0) {
            plant_approach = INFINITY;
        } else if (strcmp(argv[i], "--plant-take-reach-unbounded") == 0) {
            plant_take_reach = true;
        } else if (strcmp(argv[i], "--plant-evidence-grows-over-gaps") == 0) {
            plant_grows = true;
        } else if (strcmp(argv[i], "--plant-transform-sleep") == 0) {
            plant_sleep = true;
        } else if (strcmp(argv[i], "--sweep") == 0) {
            sweep = true;
        }
    }
    UNITY_BEGIN();
    if (sweep) {
        RUN_TEST(test_loss_sweep_100ms);
        RUN_TEST(test_loss_sweep_150ms);
        RUN_TEST(test_loss_sweep_200ms);
        return UNITY_END();
    }
    RUN_TEST(test_guest_throw_breaks_glass_host);
    RUN_TEST(test_guest_throw_breaks_glass_interaction);
    RUN_TEST(test_guest_throw_knocks_dummy_host);
    RUN_TEST(test_guest_throw_knocks_dummy_interaction);
    RUN_TEST(test_host_throw_knocks_dummy);
    RUN_TEST(test_second_throw_claims_again);
    RUN_TEST(test_forged_far_claim_is_rejected_host);
    RUN_TEST(test_forged_far_claim_is_rejected_interaction);
    RUN_TEST(test_resent_claim_applies_once);
    RUN_TEST(test_flung_far_claim_is_rejected_host);
    RUN_TEST(test_flung_far_claim_is_rejected_interaction);
    RUN_TEST(test_inflated_claim_speed_is_rejected);
    RUN_TEST(test_two_seats_claim_one_target);
    RUN_TEST(test_a_flood_starves_only_its_own_seat);
    RUN_TEST(test_inflated_far_take_is_refused);
    RUN_TEST(test_forger_at_600ms_never_fits_host);
    RUN_TEST(test_forger_at_600ms_never_fits_interaction);
    RUN_TEST(test_forger_after_a_cut_never_fits_host);
    RUN_TEST(test_forger_after_a_cut_never_fits_interaction);
    RUN_TEST(test_an_accepted_hit_that_does_nothing_is_idle);
    RUN_TEST(test_a_carried_rock_follows_its_carrier_host);
    RUN_TEST(test_a_carried_rock_follows_its_carrier_interaction);
    return UNITY_END();
}
