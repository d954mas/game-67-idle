#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stand_logic.h"
#include "unity.h"

#define ID(slot, generation) ((uint32_t)(slot) | ((uint32_t)(generation) << 16))

void setUp(void) {}
void tearDown(void) {}

static const char *const shipped[] = {"two_walk",       "stack_punch", "pile_throw", "cabinet_carry", "mannequin_hits", "grab_dispute",
                                      "breakables_break", "load_four",  "edge_throw_cut", "edge_stall", "edge_deaf",
                                      "throw_dummy",      "throw_glass_door", "throw_walker",
                                      "recall_flight",    "recall_carry",     "remove_owned"};

static body_sync_scenario_t scenario;
static body_sync_scene_t scene;
static const stand_limits_t limits = {.bounds_min = {-40.0f, -15.0f, -40.0f}, .bounds_max = {40.0f, 50.0f, 40.0f}, .max_speed = 100.0f, .max_spin = 200.0f};

static const char *two_walk_text =
    "scenario two_walk\nscene two_chars_item\nseed 7\nguests 1\nsettle 3s\n"
    "net g1 rtt 100ms jitter 10ms loss 2%\n"
    "p0 walk 2 1\np0 wait 500ms\np0 walk -2 0 as p0_home\np1 walk -2 -1 speed 2.5\np1 walk 2 0 as p1_home\n"
    "at 6s p1 walk box as p1_box\n"
    "expect result p0_home done\nexpect agree_at_rest 5cm 3deg\nexpect no_stuck\nexpect host_ticks.max <= 30t\n";

static bool parse(const char *text, body_sync_error_t *error) { return body_sync_scenario_parse(text, strlen(text), &scenario, error); }

static void test_shipped_scenarios_parse(void) {
    for (size_t i = 0; i < sizeof(shipped) / sizeof(shipped[0]); i++) {
        char path[512];
        snprintf(path, sizeof(path), "%s/%s.scn", BODY_SYNC_SCENARIOS_DIR, shipped[i]);
        body_sync_error_t error;
        if (!body_sync_scenario_load(path, &scenario, &error)) {
            char message[300];
            snprintf(message, sizeof(message), "%s:%u: %s", shipped[i], error.line, error.text);
            TEST_FAIL_MESSAGE(message);
        }
        TEST_ASSERT_TRUE(scenario.action_count > 0);
        TEST_ASSERT_TRUE(scenario.expect_count > 0);
    }
}

static void test_parse_reads_the_grammar(void) {
    body_sync_error_t error;
    TEST_ASSERT_TRUE_MESSAGE(parse(two_walk_text, &error), error.text);
    TEST_ASSERT_EQUAL_STRING("two_walk", scenario.name);
    TEST_ASSERT_EQUAL_UINT64(7, scenario.seed);
    TEST_ASSERT_EQUAL_UINT8(1, scenario.guests);
    TEST_ASSERT_EQUAL_UINT32(6, scenario.action_count);
    TEST_ASSERT_TRUE(scenario.actions[5].timed);
    TEST_ASSERT_EQUAL_UINT32(360, scenario.actions[5].tick);
    TEST_ASSERT_TRUE(scenario.actions[5].target.is_body);
    TEST_ASSERT_EQUAL_UINT32(30, scenario.actions[1].wait_ticks);
    TEST_ASSERT_TRUE(fabsf(scenario.actions[3].speed - 2.5f) < 1e-6f);
    TEST_ASSERT_EQUAL_UINT32(1, scenario.net_count);
    /* rtt is split between the two directions */
    TEST_ASSERT_TRUE(fabsf(scenario.net[0].conditions.delay_ms - 50.0f) < 1e-3f);
    TEST_ASSERT_TRUE(fabsf(scenario.net[0].conditions.loss - 0.02f) < 1e-6f);
    TEST_ASSERT_EQUAL_UINT32(4, scenario.expect_count);
    TEST_ASSERT_EQUAL(BODY_SYNC_EXPECT_METRIC_AT_MOST, scenario.expects[3].kind);
    TEST_ASSERT_TRUE(scenario.expects[3].limit > 29.0 && scenario.expects[3].limit < 31.0);
    TEST_ASSERT_TRUE(fabsf(scenario.expects[1].tolerance_m - 0.05f) < 1e-6f);
}

static void test_host_rules_parse(void) {
    body_sync_error_t error;
    TEST_ASSERT_TRUE_MESSAGE(parse("scenario a\nscene b\nat 2s p0 recall crate to 1 0.5 -2 as back\np0 recall crate\np0 remove crate as gone\n"
                                   "expect result back done\n",
                                   &error),
                             error.text);
    TEST_ASSERT_EQUAL_UINT32(3, scenario.action_count);
    const body_sync_action_t *a = &scenario.actions[0];
    TEST_ASSERT_EQUAL(BODY_SYNC_VERB_RECALL, a->verb);
    TEST_ASSERT_TRUE(a->timed && a->has_target && a->target.is_body && a->has_home);
    TEST_ASSERT_EQUAL_STRING("crate", a->target.body);
    TEST_ASSERT_TRUE(fabsf(a->home[0] - 1.0f) < 1e-6f && fabsf(a->home[1] - 0.5f) < 1e-6f && fabsf(a->home[2] + 2.0f) < 1e-6f);
    TEST_ASSERT_FALSE(scenario.actions[1].has_home);
    TEST_ASSERT_EQUAL(BODY_SYNC_VERB_REMOVE, scenario.actions[2].verb);
    TEST_ASSERT_EQUAL_STRING("gone", scenario.actions[2].label);
    TEST_ASSERT_EQUAL_STRING("recall", stand_verb_name(BODY_SYNC_VERB_RECALL));
    TEST_ASSERT_EQUAL_STRING("remove", stand_verb_name(BODY_SYNC_VERB_REMOVE));
}

static void test_net_items_sorted_by_tick(void) {
    body_sync_error_t error;
    TEST_ASSERT_TRUE_MESSAGE(parse("scenario a\nscene b\nguests 2\nat 3s cut g1 for 1s\nnet g2 to_host loss 5%\nat 1s stall host for 200ms\n"
                                   "at 2s deaf g2 for 90t\np0 wait 1s\nexpect no_stuck\n",
                                   &error),
                             error.text);
    TEST_ASSERT_EQUAL_UINT32(4, scenario.net_count);
    for (uint32_t i = 1; i < scenario.net_count; i++) {
        TEST_ASSERT_TRUE(scenario.net[i - 1].tick <= scenario.net[i].tick);
    }
    TEST_ASSERT_EQUAL(BODY_SYNC_NET_CONDITIONS, scenario.net[0].kind);
    TEST_ASSERT_EQUAL_UINT8(BODY_SYNC_TO_HOST, scenario.net[0].directions);
    TEST_ASSERT_EQUAL_UINT32(90, scenario.net[2].duration_ticks);
}

typedef struct refusal {
    const char *text;
    uint32_t line;
} refusal_t;

static void test_refusals_name_their_line(void) {
    const refusal_t cases[] = {
        {"scene a\n", 0},
        {"scenario a\nscene b\nscenario c\n", 3},
        {"scenario a\nscene b\nseed 18446744073709551616\n", 3},
        {"scenario a\nscene b\nguests 4\n", 3},
        {"scenario a\nscene b\np0 walk 1\n", 3},
        {"scenario a\nscene b\np0 walk 1 2 speed 0\n", 3},
        {"scenario a\nscene b\np0 fly 1 2\n", 3},
        {"scenario a\nscene b\nat 1e3s p0 drop\n", 3},
        {"scenario a\nscene b\nat 601s p0 drop\n", 3},
        {"scenario a\nscene b\nat 1.5 p0 drop\n", 3},
        {"scenario a\nscene b\nnet g1 loss 120%\n", 3},
        {"scenario a\nscene b\nnet g1 to_host rtt 100ms\n", 3},
        {"scenario a\nscene b\nnet g1 loss 1% loss 2%\n", 3},
        {"scenario a\nscene b\ncut g1 for 1s\n", 3},
        {"scenario a\nscene b\nat 1s cut host for 1s\n", 3},
        {"scenario a\nscene b\nexpect result nolabel done\n", 0},
        {"scenario a\nscene b\nexpect divergence.p99 <= 1m\n", 3},
        {"scenario a\nscene b\nexpect divergence.max <= 1\n", 3},
        {"scenario a\nscene b\np0 drop as x\np0 drop as x\n", 4},
        {"scenario a\nscene b\nguests 1\np2 drop\n", 0},
        {"scenario a\nscene b\np0 grab mug\xC3\xA9\n", 3},
        {"scenario a\nscene b\np0 grab NAME\n", 3},
        {"scenario a\nscene b\nexpect one_of done x\n", 3},
        {"scenario a\nscene b\np0 punch as x\np0 punch as y\nexpect one_of hit x x\n", 5},
        {"scenario a\nscene b\nnet g1 rate 0.5B/s\n", 3},
        {"scenario a\nscene b\nnet g1 rate 0B/s\n", 3},
        {"scenario a\nscene b\np0 wait 1ms\n", 3},
        {"scenario a\nscene b\nat 590s p0 wait 20s\n", 3},
        {"scenario a\nscene b\nat 595s cut g1 for 10s\n", 3},
        {"scenario a\nscene b\nnet g1 delay 20s\n", 3},
        {"scenario a\nscene b\nguests 0\nexpect agree_at_rest 5cm\n", 4},
        {"scenario a\nscene b\nexpect divergence.max <= 1m\nguests 0\n", 3},
        {"scenario a\nscene b\nguests 0\nexpect left g1\n", 4},
        {"scenario a\nscene b\nexpect left host\n", 3},
        {"scenario a\nscene b\nexpect effect\n", 3},
        {"scenario a\nscene b\nexpect effect dummy glass\n", 3},
        {"scenario a\nscene b\nat 600s p0 walk 1 1\n", 3},
        {"scenario a\nscene b\nat 600s net g1 loss 2%\n", 3},
        {"scenario a\nscene b\np0 wait 590s\nsettle 20s\n", 4},
        {"scenario a\nscene b\nguests 1\np1 recall crate\n", 4},
        {"scenario a\nscene b\nguests 1\np1 remove crate\n", 4},
        {"scenario a\nscene b\np0 recall\n", 3},
        {"scenario a\nscene b\np0 recall crate to 1 2\n", 3},
        {"scenario a\nscene b\np0 recall crate 1 2 3\n", 3},
        {"scenario a\nscene b\np0 recall crate to 1 2 3000\n", 3},
        {"scenario a\nscene b\np0 remove crate to 1 2 3\n", 3},
        {"scenario a\nscene b\np0 remove NAME\n", 3},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        body_sync_error_t error;
        char message[300];
        bool ok = parse(cases[i].text, &error);
        snprintf(message, sizeof(message), "case %u: line %u '%s'", (unsigned)i, error.line, error.text);
        TEST_ASSERT_FALSE_MESSAGE(ok, message);
        TEST_ASSERT_TRUE_MESSAGE(error.text[0] != '\0', message);
        if (cases[i].line) {
            TEST_ASSERT_EQUAL_UINT32_MESSAGE(cases[i].line, error.line, message);
        }
    }
    body_sync_error_t error;
    char long_line[400];
    memset(long_line, 'a', sizeof(long_line));
    memcpy(long_line, "scenario ", 9);
    long_line[sizeof(long_line) - 1] = '\0';
    TEST_ASSERT_FALSE(parse(long_line, &error));
    char with_nul[] = "scenario a\nscene b\0\n";
    TEST_ASSERT_FALSE(body_sync_scenario_parse(with_nul, sizeof(with_nul) - 1, &scenario, &error));
    TEST_ASSERT_FALSE(body_sync_scenario_load("no/such/file.scn", &scenario, &error));
}

/* Truncations, bit flips and random bytes: every input is refused or parsed, never a crash. */
static void test_parser_fuzz(void) {
    size_t n = strlen(two_walk_text);
    char *buffer = malloc(n + 64);
    stand_rng_t rng;
    stand_rng_seed(&rng, 99);
    body_sync_error_t error;
    for (size_t cut = 0; cut <= n; cut++) {
        memcpy(buffer, two_walk_text, cut);
        (void)body_sync_scenario_parse(buffer, cut, &scenario, &error);
    }
    for (int i = 0; i < 4000; i++) {
        memcpy(buffer, two_walk_text, n);
        int flips = 1 + (int)(stand_rng_next(&rng) % 3);
        for (int f = 0; f < flips; f++) {
            size_t at = (size_t)(stand_rng_next(&rng) % n);
            buffer[at] = (char)(buffer[at] ^ (1 << (stand_rng_next(&rng) % 8)));
        }
        bool ok = body_sync_scenario_parse(buffer, n, &scenario, &error);
        if (ok) {
            TEST_ASSERT_TRUE(scenario.action_count <= BODY_SYNC_SCENARIO_ACTIONS_MAX);
            TEST_ASSERT_TRUE(scenario.guests <= 3);
        }
    }
    for (int i = 0; i < 2000; i++) {
        size_t size = (size_t)(stand_rng_next(&rng) % (n + 64));
        for (size_t k = 0; k < size; k++) {
            buffer[k] = (char)(stand_rng_next(&rng) & 0xFF);
        }
        (void)body_sync_scenario_parse(buffer, size, &scenario, &error);
    }
    free(buffer);
}

static void test_schedule_orders_actions(void) {
    body_sync_error_t error;
    TEST_ASSERT_TRUE_MESSAGE(parse("scenario a\nscene b\np0 walk 1 1\np0 drop\nat 2s p0 punch\np0 wait 10t\nexpect no_stuck\n", &error), error.text);
    stand_schedule_t *s = malloc(sizeof(*s));
    stand_schedule_init(s, &scenario);
    TEST_ASSERT_EQUAL_INT32(0, stand_schedule_next(s, 0, 0));
    TEST_ASSERT_EQUAL_INT32(-1, stand_schedule_next(s, 0, 1)); /* busy: the untimed drop waits */
    stand_schedule_end(s, 0);
    TEST_ASSERT_EQUAL_INT32(1, stand_schedule_next(s, 0, 2));
    stand_schedule_end(s, 0);
    TEST_ASSERT_EQUAL_INT32(-1, stand_schedule_next(s, 0, 3)); /* the timed punch is not due */
    TEST_ASSERT_EQUAL_INT32(2, stand_schedule_next(s, 0, 120));
    TEST_ASSERT_FALSE(stand_schedule_done(s));
    stand_schedule_end(s, 0);
    TEST_ASSERT_EQUAL_INT32(3, stand_schedule_next(s, 0, 121));
    stand_schedule_end(s, 0);
    TEST_ASSERT_TRUE(stand_schedule_done(s));
    /* a due timed action skips the untimed ones before it */
    stand_schedule_init(s, &scenario);
    TEST_ASSERT_EQUAL_INT32(0, stand_schedule_next(s, 0, 0));
    TEST_ASSERT_EQUAL_INT32(2, stand_schedule_next(s, 0, 200));
    free(s);
}

static void test_rules_verdicts(void) {
    body_sync_body_desc_t box = body_sync_body_default(BODY_SYNC_SHAPE_BOX, (const float[3]){0.2f, 0.2f, 0.2f}, 2.0f, NULL);
    box.flags = BODY_SYNC_BODY_GRABBABLE;
    stand_target_t t = {.live = true, .desc = &box, .host_simulates = true, .position = {1.0f, 0.2f, 0.0f}};
    const float near[3] = {0.0f, 0.87f, 0.0f}, far[3] = {9.0f, 0.87f, 0.0f};
    TEST_ASSERT_TRUE(stand_rules_grab(&t, near, 1.65f));
    TEST_ASSERT_FALSE(stand_rules_grab(&t, far, 1.65f));
    t.held = true;
    TEST_ASSERT_FALSE(stand_rules_grab(&t, near, 1.65f));
    t.held = false;
    box.flags = 0;
    TEST_ASSERT_FALSE(stand_rules_grab(&t, near, 1.65f));

    body_sync_body_desc_t dummy = body_sync_body_default(BODY_SYNC_SHAPE_CAPSULE, (const float[3]){0.3f, 0.55f, 0.0f}, 60.0f, NULL);
    dummy.role = BODY_SYNC_ROLE_MANNEQUIN;
    stand_target_t m = {.live = true, .desc = &dummy, .host_simulates = true, .position = {1.0f, 0.87f, 0.0f}};
    stand_effect_t effect;
    TEST_ASSERT_TRUE(stand_rules_hit(&m, near, 1.4f, &effect));
    TEST_ASSERT_EQUAL(STAND_EFFECT_KNOCK, effect);
    TEST_ASSERT_FALSE(stand_rules_hit(&m, far, 1.4f, &effect));
    m.knocked = true;
    TEST_ASSERT_FALSE(stand_rules_hit(&m, near, 1.4f, &effect));

    body_sync_body_desc_t glass = body_sync_body_default(BODY_SYNC_SHAPE_BOX, (const float[3]){0.03f, 0.6f, 0.4f}, 2.0f, NULL);
    glass.role = BODY_SYNC_ROLE_BREAKABLE;
    glass.break_speed = 3.0f;
    glass.debris_speed = 2.0f;
    stand_target_t g = {.live = true, .desc = &glass, .position = {0.0f, 0.6f, 0.0f}};
    TEST_ASSERT_EQUAL(STAND_EFFECT_BREAK, stand_rules_impact(&g, 3.5f));
    TEST_ASSERT_EQUAL(STAND_EFFECT_NONE, stand_rules_impact(&g, 2.5f));
    TEST_ASSERT_EQUAL(STAND_EFFECT_NONE, stand_rules_impact(&g, NAN));
    stand_rng_t rng;
    stand_rng_seed(&rng, 5);
    const float rot[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    for (int i = 0; i < 100; i++) {
        float at[3], v[3];
        stand_rules_debris(&rng, &glass, g.position, rot, at, v);
        TEST_ASSERT_TRUE(fabsf(at[0]) <= glass.size[0] && fabsf(at[1] - 0.6f) <= glass.size[1] && fabsf(at[2]) <= glass.size[2]);
        TEST_ASSERT_TRUE(isfinite(v[0]) && isfinite(v[1]) && isfinite(v[2]) && stand_length3(v) <= glass.debris_speed + 0.6f);
    }
}

enum { STAMP = 1000 }; /* the records' own stamp; an impact names a contact just before it */

static void test_records_round_trip_and_refuse(void) {
    const stand_record_t samples[] = {
        {.type = STAND_REC_HIT, .target = ID(3, 1), .action = 7, .origin = {1.0f, 2.0f, 3.0f}},
        {.type = STAND_REC_IMPACT, .target = ID(4, 2), .body = ID(5, 1), .number = 513, .speed = 6.0f, .tick = STAMP - 1},
        {.type = STAND_REC_VERDICT, .target = ID(3, 1), .action = 7, .accepted = 1},
        {.type = STAND_REC_SPAWN, .target = ID(40, 3), .source = 2, .position = {1, 2, 3}, .rotation = {0, 0, 0, 1}, .velocity = {1, 0, 0}},
        {.type = STAND_REC_REMOVE, .target = ID(9, 1)},
    };
    uint8_t buffer[STAND_RECORD_MAX];
    stand_record_t out;
    char why[96];
    for (size_t i = 0; i < sizeof(samples) / sizeof(samples[0]); i++) {
        uint32_t size = stand_record_encode(&samples[i], buffer, sizeof(buffer));
        TEST_ASSERT_TRUE(size > 1);
        TEST_ASSERT_EQUAL_UINT8(STAND_FAMILY_STAND, buffer[0]);
        bool from_host = samples[i].type >= STAND_REC_VERDICT;
        TEST_ASSERT_TRUE_MESSAGE(stand_record_decode(buffer + 1, size - 1, from_host, STAMP, &limits, &out, why, sizeof(why)), why);
        TEST_ASSERT_EQUAL(samples[i].type, out.type);
        TEST_ASSERT_EQUAL_UINT32(samples[i].target, out.target);
        TEST_ASSERT_FALSE(stand_record_decode(buffer + 1, size - 1, !from_host, STAMP, &limits, &out, why, sizeof(why)));
        for (uint32_t cut = 0; cut < size - 1; cut++) {
            TEST_ASSERT_FALSE(stand_record_decode(buffer + 1, cut, from_host, STAMP, &limits, &out, why, sizeof(why)));
        }
        TEST_ASSERT_EQUAL_UINT32(0, stand_record_encode(&samples[i], buffer, size - 1));
    }
    stand_rng_t rng;
    stand_rng_seed(&rng, 17);
    for (int i = 0; i < 20000; i++) {
        uint32_t size = (uint32_t)(stand_rng_next(&rng) % 70);
        for (uint32_t k = 0; k < size; k++) {
            buffer[k] = (uint8_t)stand_rng_next(&rng);
        }
        if (stand_record_decode(buffer, size, (i & 1) != 0, STAMP, &limits, &out, why, sizeof(why))) {
            TEST_ASSERT_TRUE((out.target & 0xFFFFu) < STAND_BODIES_MAX && (out.target >> 16) != 0);
        }
    }
}

/* The impact rule by role: a breakable breaks and a standing mannequin falls at or past its
   threshold, nothing else; past a threshold every faster contact has the same effect. */
static void test_impact_rules_by_role(void) {
    body_sync_body_desc_t glass = body_sync_body_default(BODY_SYNC_SHAPE_BOX, (const float[3]){0.03f, 0.6f, 0.4f}, 2.0f, NULL);
    glass.role = BODY_SYNC_ROLE_BREAKABLE;
    glass.break_speed = 3.0f;
    body_sync_body_desc_t dummy = body_sync_body_default(BODY_SYNC_SHAPE_CAPSULE, (const float[3]){0.3f, 0.55f, 0.0f}, 60.0f, NULL);
    dummy.role = BODY_SYNC_ROLE_MANNEQUIN;
    body_sync_body_desc_t box = body_sync_body_default(BODY_SYNC_SHAPE_BOX, (const float[3]){0.2f, 0.2f, 0.2f}, 2.0f, NULL);
    TEST_ASSERT_TRUE(dummy.fall_speed > 0.0f); /* the default fells a mannequin at some speed */
    stand_target_t g = {.live = true, .desc = &glass}, m = {.live = true, .desc = &dummy}, b = {.live = true, .desc = &box};
    const struct {
        stand_target_t *target;
        stand_effect_t effect;
        float threshold;
    } rows[] = {{&g, STAND_EFFECT_BREAK, glass.break_speed}, {&m, STAND_EFFECT_KNOCK, dummy.fall_speed}, {&b, STAND_EFFECT_NONE, 0.0f}};
    for (size_t r = 0; r < sizeof(rows) / sizeof(rows[0]); r++) {
        bool seen = false;
        for (float v = 0.0f; v <= 60.0f; v += 0.25f) {
            stand_effect_t e = stand_rules_impact(rows[r].target, v);
            TEST_ASSERT_TRUE(e == STAND_EFFECT_NONE || e == rows[r].effect);
            TEST_ASSERT_TRUE_MESSAGE(!seen || e == rows[r].effect, "an effect stopped at a faster contact");
            TEST_ASSERT_TRUE(rows[r].effect == STAND_EFFECT_NONE || (e != STAND_EFFECT_NONE) == (v >= rows[r].threshold));
            seen = seen || e != STAND_EFFECT_NONE;
        }
        TEST_ASSERT_EQUAL(rows[r].effect, stand_rules_impact(rows[r].target, 60.0f));
        TEST_ASSERT_EQUAL(STAND_EFFECT_NONE, stand_rules_impact(rows[r].target, NAN));
        TEST_ASSERT_EQUAL(STAND_EFFECT_NONE, stand_rules_impact(rows[r].target, INFINITY));
    }
    /* A felled mannequin, a body gone and a character take no effect. */
    m.knocked = true;
    TEST_ASSERT_EQUAL(STAND_EFFECT_NONE, stand_rules_impact(&m, 60.0f));
    g.live = false;
    TEST_ASSERT_EQUAL(STAND_EFFECT_NONE, stand_rules_impact(&g, 60.0f));
    stand_target_t c = {.live = true, .character = true};
    TEST_ASSERT_EQUAL(STAND_EFFECT_NONE, stand_rules_impact(&c, 60.0f));
}

/* The host's limits: the gap within the reach, the claimed speed within a quarter plus slack of
   the approach it saw; a measure that is not a number refuses. */
static void test_impact_limits(void) {
    TEST_ASSERT_TRUE(stand_impact_accepts(0.0f, 1.0f, 7.0f, 7.0f));
    TEST_ASSERT_TRUE(stand_impact_accepts(1.0f, 1.0f, 7.0f, 7.0f * STAND_IMPACT_SPEED_SCALE + STAND_IMPACT_SPEED_SLACK));
    TEST_ASSERT_FALSE(stand_impact_accepts(1.01f, 1.0f, 7.0f, 7.0f));
    TEST_ASSERT_FALSE(stand_impact_accepts(0.0f, 1.0f, 7.0f, 7.0f * STAND_IMPACT_SPEED_SCALE + STAND_IMPACT_SPEED_SLACK + 0.01f));
    TEST_ASSERT_TRUE(stand_impact_accepts(0.0f, 1.0f, 0.0f, STAND_IMPACT_SPEED_SLACK));
    TEST_ASSERT_FALSE(stand_impact_accepts(INFINITY, 1.0f, 7.0f, 1.0f)); /* a STALE verdict's gap */
    TEST_ASSERT_FALSE(stand_impact_accepts(NAN, 1.0f, 7.0f, 1.0f));
    TEST_ASSERT_FALSE(stand_impact_accepts(0.0f, NAN, 7.0f, 1.0f));
    TEST_ASSERT_FALSE(stand_impact_accepts(0.0f, 1.0f, NAN, 1.0f));
    /* Monotone: a wider reach or a faster approach never refuses what a tighter one took. */
    for (float gap = 0.0f; gap < 3.0f; gap += 0.5f) {
        for (float speed = 0.0f; speed < 12.0f; speed += 1.0f) {
            bool a = stand_impact_accepts(gap, 1.5f, 6.0f, speed), b = stand_impact_accepts(gap, 2.5f, 8.0f, speed);
            TEST_ASSERT_TRUE(!a || b);
        }
    }
}

/* A key is sent once; a new number (a new throw), another target, agent or seat is a new key; the
   ring forgets the oldest past its size. */
static void test_claim_keys(void) {
    stand_claim_keys_t *keys = calloc(1, sizeof(*keys));
    const stand_claim_key_t k = {.agent = ID(5, 1), .target = ID(4, 2), .number = 7, .seat = 1};
    TEST_ASSERT_FALSE(stand_claim_keys_has(keys, k));
    TEST_ASSERT_TRUE(stand_claim_keys_add(keys, k));
    TEST_ASSERT_TRUE(stand_claim_keys_has(keys, k));
    TEST_ASSERT_FALSE(stand_claim_keys_add(keys, k));
    stand_claim_key_t other = k;
    other.number = 8;
    TEST_ASSERT_TRUE(stand_claim_keys_add(keys, other));
    other = k;
    other.target = ID(6, 1);
    TEST_ASSERT_TRUE(stand_claim_keys_add(keys, other));
    other = k;
    other.agent = ID(5, 2);
    TEST_ASSERT_TRUE(stand_claim_keys_add(keys, other));
    other = k;
    other.seat = 2;
    TEST_ASSERT_TRUE(stand_claim_keys_add(keys, other));
    TEST_ASSERT_FALSE(stand_claim_keys_add(keys, k));
    for (uint16_t n = 100; n < 100 + STAND_CLAIM_KEYS_MAX; n++) {
        stand_claim_key_t fill = k;
        fill.number = n;
        TEST_ASSERT_TRUE(stand_claim_keys_add(keys, fill));
    }
    TEST_ASSERT_EQUAL_UINT32(STAND_CLAIM_KEYS_MAX, keys->count);
    TEST_ASSERT_TRUE(stand_claim_keys_add(keys, k));
    free(keys);
}

/* An impact claim's every field out of range is refused by name; a valid one mutated byte by
   byte, and random bytes, never crash, and whatever parses holds every field in range. */
static void test_impact_record_refusals(void) {
    const stand_record_t valid = {.type = STAND_REC_IMPACT, .target = ID(4, 2), .body = ID(5, 1), .number = 9, .speed = 6.0f, .tick = STAMP - 2};
    uint8_t buffer[STAND_RECORD_MAX];
    stand_record_t out;
    char why[96];
    const struct {
        stand_record_t record;
        const char *why;
    } cases[] = {
        {{.type = STAND_REC_IMPACT, .target = ID(STAND_BODIES_MAX, 1), .body = ID(5, 1), .speed = 6.0f}, "body id out of range"},
        {{.type = STAND_REC_IMPACT, .target = ID(4, 0), .body = ID(5, 1), .speed = 6.0f}, "body id out of range"},
        {{.type = STAND_REC_IMPACT, .target = ID(4, 2), .body = ID(0xFFFF, 1), .speed = 6.0f}, "impact agent id out of range"},
        {{.type = STAND_REC_IMPACT, .target = ID(4, 2), .body = ID(5, 0), .speed = 6.0f}, "impact agent id out of range"},
        {{.type = STAND_REC_IMPACT, .target = ID(4, 2), .body = ID(4, 2), .speed = 6.0f}, "impact agent is its target"},
        {{.type = STAND_REC_IMPACT, .target = ID(4, 2), .body = ID(5, 1), .speed = NAN}, "impact speed out of range"},
        {{.type = STAND_REC_IMPACT, .target = ID(4, 2), .body = ID(5, 1), .speed = INFINITY}, "impact speed out of range"},
        {{.type = STAND_REC_IMPACT, .target = ID(4, 2), .body = ID(5, 1), .speed = -1.0f}, "impact speed out of range"},
        {{.type = STAND_REC_IMPACT, .target = ID(4, 2), .body = ID(5, 1), .speed = 100.5f}, "impact speed out of range"},
        {{.type = STAND_REC_IMPACT, .target = ID(4, 2), .body = ID(5, 1), .speed = 6.0f, .tick = STAMP + 1}, "impact tick out of range"},
        {{.type = STAND_REC_IMPACT, .target = ID(4, 2), .body = ID(5, 1), .speed = 6.0f, .tick = STAMP - STAND_CONTACT_AGE_MAX_TICKS - 1}, "impact tick out of range"},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        uint32_t size = stand_record_encode(&cases[i].record, buffer, sizeof(buffer));
        TEST_ASSERT_TRUE(size > 1);
        TEST_ASSERT_FALSE(stand_record_decode(buffer + 1, size - 1, false, STAMP, &limits, &out, why, sizeof(why)));
        TEST_ASSERT_NOT_NULL_MESSAGE(strstr(why, cases[i].why), why);
    }
    uint32_t size = stand_record_encode(&valid, buffer, sizeof(buffer));
    TEST_ASSERT_TRUE(stand_record_decode(buffer + 1, size - 1, false, STAMP, &limits, &out, why, sizeof(why)));
    TEST_ASSERT_EQUAL_UINT32(valid.body, out.body);
    TEST_ASSERT_EQUAL_UINT16(valid.number, out.number);
    TEST_ASSERT_TRUE(out.speed == valid.speed);
    TEST_ASSERT_EQUAL_UINT32(valid.tick, out.tick);
    /* wrong length both ways, an unknown type, and a host's type from a guest */
    TEST_ASSERT_FALSE(stand_record_decode(buffer + 1, size, false, STAMP, &limits, &out, why, sizeof(why)));
    TEST_ASSERT_NOT_NULL(strstr(why, "wrong size"));
    buffer[1] = 0;
    TEST_ASSERT_FALSE(stand_record_decode(buffer + 1, size - 1, false, STAMP, &limits, &out, why, sizeof(why)));
    TEST_ASSERT_NOT_NULL(strstr(why, "unknown type"));
    buffer[1] = STAND_REC_REMOVE + 1;
    TEST_ASSERT_FALSE(stand_record_decode(buffer + 1, size - 1, false, STAMP, &limits, &out, why, sizeof(why)));
    TEST_ASSERT_NOT_NULL(strstr(why, "unknown type"));

    stand_rng_t rng;
    stand_rng_seed(&rng, 41);
    uint32_t refused = 0, taken = 0;
    for (int i = 0; i < 50000; i++) {
        size = stand_record_encode(&valid, buffer, sizeof(buffer));
        uint32_t flips = 1u + (uint32_t)(stand_rng_next(&rng) % 4);
        for (uint32_t f = 0; f < flips; f++) {
            buffer[1 + stand_rng_next(&rng) % (size - 1)] = (uint8_t)stand_rng_next(&rng);
        }
        uint32_t cut = (stand_rng_next(&rng) % 8 == 0) ? (uint32_t)(stand_rng_next(&rng) % size) : size - 1;
        why[0] = 0;
        if (stand_record_decode(buffer + 1, cut, false, STAMP, &limits, &out, why, sizeof(why))) {
            taken++;
            TEST_ASSERT_TRUE(out.type == STAND_REC_IMPACT || out.type == STAND_REC_HIT);
            TEST_ASSERT_TRUE((out.target & 0xFFFFu) < STAND_BODIES_MAX && (out.target >> 16) != 0);
            if (out.type == STAND_REC_IMPACT) {
                TEST_ASSERT_TRUE((out.body & 0xFFFFu) < STAND_BODIES_MAX && (out.body >> 16) != 0 && out.body != out.target);
                TEST_ASSERT_TRUE(isfinite(out.speed) && out.speed >= 0.0f && out.speed <= limits.max_speed);
                TEST_ASSERT_TRUE((uint32_t)STAMP - out.tick <= STAND_CONTACT_AGE_MAX_TICKS);
            }
        } else {
            refused++;
            TEST_ASSERT_TRUE_MESSAGE(why[0] != 0, "a refusal says why");
        }
    }
    TEST_ASSERT_EQUAL_UINT32(50000, refused + taken);
    TEST_ASSERT_TRUE(refused > 0 && taken > 0);
}

static void test_new_forms_are_read(void) {
    body_sync_error_t error;
    TEST_ASSERT_TRUE_MESSAGE(parse("scenario a\nscene b\nnet g1 rtt 6t rate off\nexpect agree_at_rest 5cm\nexpect agree_at_rest 5cm 0deg\n"
                                   "expect left g1\n",
                                   &error),
                             error.text);
    TEST_ASSERT_TRUE(fabsf(scenario.net[0].conditions.delay_ms - 50.0f) < 1e-3f);
    TEST_ASSERT_EQUAL_UINT32(0, scenario.net[0].conditions.rate_bytes_per_s);
    TEST_ASSERT_TRUE(scenario.net[0].fields & BODY_SYNC_FIELD_RATE);
    TEST_ASSERT_TRUE(scenario.expects[0].tolerance_deg < 0.0f);  /* no angle written: not checked */
    TEST_ASSERT_TRUE(scenario.expects[1].tolerance_deg == 0.0f); /* 0deg written: a bound */
    TEST_ASSERT_EQUAL(BODY_SYNC_EXPECT_LEFT, scenario.expects[2].kind);
    TEST_ASSERT_EQUAL_UINT8(1, scenario.expects[2].endpoint);
    TEST_ASSERT_TRUE_MESSAGE(parse("scenario a\nscene b\nexpect effect dummy\n", &error), error.text);
    TEST_ASSERT_EQUAL(BODY_SYNC_EXPECT_EFFECT, scenario.expects[0].kind);
    TEST_ASSERT_EQUAL_STRING("dummy", scenario.expects[0].body);
    TEST_ASSERT_TRUE_MESSAGE(parse("scenario a\nscene b\nguests 0\np0 walk 1 1\nexpect host_ticks.max <= 3t\nexpect no_stuck\n", &error), error.text);
    /* a timed action cuts off the long wait before it, so this schedule fits */
    TEST_ASSERT_TRUE_MESSAGE(parse("scenario a\nscene b\np0 wait 600s\nat 1s p0 wait 1s\n", &error), error.text);
    TEST_ASSERT_TRUE_MESSAGE(parse("scenario a\nscene b\nat 599s p0 drop\nend 600s\n", &error), error.text);
    TEST_ASSERT_FALSE(parse("scenario a\nscene b\np0 wait 598s\n", &error)); /* the default settle runs past */
}

/* A delay never seen is unbounded: the bound fails and the label is named. */
static void test_unseen_delays_are_unbounded(void) {
    body_sync_report_t *r = calloc(1, sizeof(*r));
    r->action_count = 4;
    /* a walk that started at its target showed nothing: out of every delay */
    r->actions[3] = (body_sync_action_metrics_t){.line = 7, .verb = BODY_SYNC_VERB_WALK, .start_tick = 1, .host_ticks = -1, .echo_ticks = -1, .seen_ticks = -1};
    r->actions[0] = (body_sync_action_metrics_t){.line = 4, .verb = BODY_SYNC_VERB_WALK, .start_tick = 0, .host_ticks = 3, .echo_ticks = -1, .seen_ticks = 5, .shows = true};
    r->actions[1] = (body_sync_action_metrics_t){.line = 5, .verb = BODY_SYNC_VERB_WAIT, .start_tick = 9, .host_ticks = -1, .echo_ticks = -1, .seen_ticks = -1, .shows = false};
    r->actions[2] = (body_sync_action_metrics_t){.line = 6, .verb = BODY_SYNC_VERB_GRAB, .start_tick = UINT32_MAX, .host_ticks = -1, .echo_ticks = -1, .seen_ticks = -1, .shows = true, .answered = true};
    snprintf(r->actions[0].label, sizeof(r->actions[0].label), "w");
    double v = 0.0;
    char why[128];
    TEST_ASSERT_TRUE(stand_metric_value(r, NULL, "host_ticks.max", &v, why, sizeof(why)));
    TEST_ASSERT_TRUE(v > 2.9 && v < 3.1); /* a wait and an action never started do not count */
    TEST_ASSERT_TRUE(stand_metric_value(r, NULL, "echo_ticks.max", &v, why, sizeof(why)));
    TEST_ASSERT_TRUE(v < 0.5); /* a walk has no echo */
    r->actions[2].start_tick = 30;
    r->actions[2].host_ticks = 4;
    TEST_ASSERT_TRUE(stand_metric_value(r, NULL, "echo_ticks.max", &v, why, sizeof(why)));
    TEST_ASSERT_TRUE(isinf(v));
    TEST_ASSERT_NOT_NULL(strstr(why, "line 6"));
    r->actions[0].seen_ticks = -1;
    TEST_ASSERT_TRUE(stand_metric_value(r, NULL, "seen_ticks.max", &v, why, sizeof(why)));
    TEST_ASSERT_TRUE(isinf(v));
    TEST_ASSERT_NOT_NULL(strstr(why, "w on line 4"));
    free(r);
}

/* A throw is a thrower's own: one never drawn moving on it, none at all, or a thrower with no
   body it simulates measured reads unbounded, never 0. */
static void test_a_throw_unseen_by_its_thrower_is_unbounded(void) {
    body_sync_report_t *r = calloc(1, sizeof(*r));
    r->guest_count = 1;
    r->guests[0].seat = 1;
    double v = 0.0;
    char why[128];
    TEST_ASSERT_TRUE(stand_metric_value(r, NULL, "own_ticks.max", &v, why, sizeof(why)));
    TEST_ASSERT_TRUE(isinf(v));
    TEST_ASSERT_NOT_NULL(strstr(why, "no throw"));
    TEST_ASSERT_TRUE(stand_metric_value(r, NULL, "local_correction.max", &v, why, sizeof(why)));
    TEST_ASSERT_TRUE(v < 0.5); /* nothing thrown: no body to simulate is no defect */
    r->action_count = 1;
    r->actions[0] = (body_sync_action_metrics_t){.line = 3, .player = 1, .verb = BODY_SYNC_VERB_THROW, .start_tick = 5, .outcome = BODY_SYNC_OUTCOME_DONE,
                                                 .host_ticks = -1, .echo_ticks = -1, .seen_ticks = -1, .own_ticks = -1};
    TEST_ASSERT_TRUE(stand_metric_value(r, NULL, "own_ticks.max", &v, why, sizeof(why)));
    TEST_ASSERT_TRUE(isinf(v));
    TEST_ASSERT_NOT_NULL(strstr(why, "line 3"));
    TEST_ASSERT_TRUE(stand_metric_value(r, NULL, "local_turn.max", &v, why, sizeof(why)));
    TEST_ASSERT_TRUE(isinf(v));
    TEST_ASSERT_NOT_NULL(strstr(why, "g1 threw"));
    r->actions[0].own_ticks = 1;
    r->guests[0].local_samples = 3;
    TEST_ASSERT_TRUE(stand_metric_value(r, NULL, "own_ticks.max", &v, why, sizeof(why)));
    TEST_ASSERT_TRUE(v > 0.9 && v < 1.1);
    TEST_ASSERT_TRUE(stand_metric_value(r, NULL, "local_correction.max", &v, why, sizeof(why)));
    TEST_ASSERT_TRUE(v < 0.5);
    free(r);
}

/* A driven body's rest by its window: a copy-grid step a quarter second (closed in one tick at
   0.2 m/s) rests; a drive kept at 6 cm/s, a centimetre's snap in one tick and a turn past a step
   in one tick do not. */
static void test_a_driven_body_rests_by_its_window(void) {
    stand_drive_t d;
    float speed, spin;
    memset(&d, 0, sizeof(d));
    for (uint32_t i = 0; i < 120; i++) {
        stand_drive_note(&d, i % STAND_DRIVE_TICKS == 0 ? 0.0034f / STAND_DT : 0.0f, 0.0f);
        stand_drive_motion(&d, STAND_DT, &speed, &spin);
        TEST_ASSERT_TRUE_MESSAGE(speed < 0.05f && spin < 0.5f, "a step of the copy grid read as motion");
    }
    memset(&d, 0, sizeof(d));
    for (uint32_t i = 0; i < STAND_DRIVE_TICKS; i++) {
        stand_drive_note(&d, 0.06f, 0.0f);
    }
    stand_drive_motion(&d, STAND_DT, &speed, &spin);
    TEST_ASSERT_TRUE_MESSAGE(speed >= 0.05f, "a drive kept above the rest speed read as rest");
    memset(&d, 0, sizeof(d));
    stand_drive_note(&d, 0.01f / STAND_DT, 0.0f);
    stand_drive_motion(&d, STAND_DT, &speed, &spin);
    TEST_ASSERT_TRUE_MESSAGE(speed >= 0.05f, "a centimetre's snap read as rest");
    stand_drive_note(&d, 0.0f, 0.04f / STAND_DT);
    stand_drive_motion(&d, STAND_DT, &speed, &spin);
    TEST_ASSERT_TRUE_MESSAGE(spin >= 0.5f, "a turn past a step in one tick read as rest");
}

static void test_histogram_quantiles(void) {
    stand_hist_t *h = calloc(1, sizeof(*h));
    for (int i = 1; i <= 1000; i++) {
        stand_hist_add(h, (double)i * 0.001);
    }
    double p50 = stand_hist_quantile(h, 0.5), p95 = stand_hist_quantile(h, 0.95);
    TEST_ASSERT_TRUE(p50 >= 0.5 * 0.96 && p50 <= 0.5 * 1.05);
    TEST_ASSERT_TRUE(p95 >= 0.95 * 0.96 && p95 <= 0.95 * 1.05);
    TEST_ASSERT_TRUE(p50 <= p95 && p95 <= h->max);
    TEST_ASSERT_TRUE(fabs(stand_hist_mean(h) - 0.5005) < 1e-9);
    stand_hist_add(h, (double)NAN);
    TEST_ASSERT_EQUAL_UINT64(1000, h->count);
    free(h);
}

static void test_json_is_complete_and_repeatable(void) {
    stand_metrics_t *m = calloc(1, sizeof(*m));
    body_sync_report_t *r = calloc(1, sizeof(*r));
    stand_metrics_reset(m, 1);
    for (uint32_t t = 0; t < 120; t++) {
        stand_metrics_begin_tick(m);
        stand_metrics_divergence(m, 0, 0.01 * (double)(t % 7), 1.0);
        stand_metrics_jerk(m, 0, 0.001 * (double)(t % 5));
        stand_metrics_end_tick(m);
        stand_metrics_bytes(m, 0, STAND_DIR_TO_GUEST, 1, 100, t);
    }
    snprintf(r->scenario, sizeof(r->scenario), "x\"y");
    r->rest_tick = UINT32_MAX;
    r->tick_hz = 60;
    r->ticks = 120;
    stand_metrics_summarize(m, r);
    r->guests[0].rtt_ms = (double)NAN;
    size_t need = 0;
    TEST_ASSERT_FALSE(stand_json_write(r, m, true, NULL, 0, &need));
    TEST_ASSERT_TRUE(need > 100);
    char *a = malloc(need + 1), *b = malloc(need + 1);
    size_t sa, sb;
    TEST_ASSERT_TRUE(stand_json_write(r, m, true, a, need + 1, &sa));
    TEST_ASSERT_TRUE(stand_json_write(r, m, true, b, need + 1, &sb));
    TEST_ASSERT_EQUAL_size_t(sa, sb);
    TEST_ASSERT_EQUAL_MEMORY(a, b, sa);
    TEST_ASSERT_NOT_NULL(strstr(a, "\"schema\":\"body_sync.metrics.v2\""));
    TEST_ASSERT_NOT_NULL(strstr(a, "\"x\\\"y\""));
    TEST_ASSERT_NOT_NULL(strstr(a, "\"rtt_ms\":null"));
    int depth = 0;
    bool in_string = false;
    for (size_t i = 0; i < sa; i++) {
        if (in_string) {
            if (a[i] == '\\') {
                i++;
            } else if (a[i] == '"') {
                in_string = false;
            }
            continue;
        }
        depth += a[i] == '{' || a[i] == '[' ? 1 : a[i] == '}' || a[i] == ']' ? -1 : 0;
        in_string = a[i] == '"';
        TEST_ASSERT_TRUE(depth >= 0);
    }
    TEST_ASSERT_EQUAL_INT(0, depth);
    double v;
    TEST_ASSERT_TRUE(stand_metric_value(r, m, "bytes_per_s.to_guest", &v, NULL, 0));
    TEST_ASSERT_TRUE(v > 5000.0 && v < 7000.0);
    TEST_ASSERT_TRUE(stand_metric_value(r, m, "peak_bytes_per_s.to_guest", &v, NULL, 0));
    TEST_ASSERT_TRUE(v >= 6000.0 - 1.0 && v <= 6000.0 + 1.0);
    TEST_ASSERT_FALSE(stand_metric_value(r, m, "nonsense", &v, NULL, 0));
    free(a);
    free(b);
    free(m);
    free(r);
}

static void test_catalog_validates_and_broken_scenes_are_refused(void) {
    uint32_t count = 0;
    const body_sync_scene_entry_t *catalog = body_sync_scene_catalog(&count);
    body_sync_error_t error;
    /* Every scene a shipped scenario names is in the catalog. */
    for (size_t i = 0; i < sizeof(shipped) / sizeof(shipped[0]); i++) {
        char path[512];
        snprintf(path, sizeof(path), "%s/%s.scn", BODY_SYNC_SCENARIOS_DIR, shipped[i]);
        TEST_ASSERT_TRUE_MESSAGE(body_sync_scenario_load(path, &scenario, &error), shipped[i]);
        TEST_ASSERT_NOT_NULL_MESSAGE(body_sync_scene_lookup(catalog, count, scenario.scene), shipped[i]);
    }
    for (uint32_t i = 0; i < count; i++) {
        catalog[i].build(&scene);
        TEST_ASSERT_TRUE_MESSAGE(body_sync_scene_validate(&scene, &error), error.text);
        TEST_ASSERT_EQUAL_STRING(catalog[i].name, scene.name);
        TEST_ASSERT_TRUE(scene.body_count > 0);
    }
    TEST_ASSERT_NOT_NULL(body_sync_scene_lookup(catalog, count, "load300"));
    TEST_ASSERT_NULL(body_sync_scene_lookup(catalog, count, "nothing"));
    catalog[0].build(&scene);
    scene.bodies[2].size[0] = NAN;
    TEST_ASSERT_FALSE(body_sync_scene_validate(&scene, &error));
    TEST_ASSERT_EQUAL_UINT32(0, error.line); /* a scene is data: the body is named, not a line */
    TEST_ASSERT_NOT_NULL(strstr(error.text, "body 2 'crate'"));
    catalog[0].build(&scene);
    scene.bodies[2].position[0] = 35.0f; /* beyond the bounds */
    TEST_ASSERT_FALSE(body_sync_scene_validate(&scene, &error));
    catalog[0].build(&scene);
    scene.players[1].position[0] = 1.0f; /* inside the crate */
    TEST_ASSERT_FALSE(body_sync_scene_validate(&scene, &error));
    TEST_ASSERT_NOT_NULL(strstr(error.text, "player 1"));
    /* the body the player stands in is named, not the first one it touches */
    catalog[0].build(&scene);
    scene.players[1].position[0] = 1.0f;
    body_sync_body_desc_t big = body_sync_body_default(BODY_SYNC_SHAPE_BOX, (const float[3]){0.4f, 0.4f, 0.4f}, 5.0f, (const float[3]){1.0f, 1.0f, 0.0f});
    body_sync_scene_add(&scene, &big);
    TEST_ASSERT_FALSE(body_sync_scene_validate(&scene, &error));
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(error.text, "body 3 "), error.text);
    /* a body's extent, not only its centre, stays inside the bounds */
    catalog[0].build(&scene);
    scene.bodies[2].position[0] = 29.9f;
    TEST_ASSERT_FALSE(body_sync_scene_validate(&scene, &error));
    TEST_ASSERT_NOT_NULL(strstr(error.text, "reaches outside"));
    /* two players on one spot */
    catalog[0].build(&scene);
    scene.players[1].position[0] = scene.players[0].position[0] + 0.3f;
    scene.players[1].position[2] = scene.players[0].position[2];
    TEST_ASSERT_FALSE(body_sync_scene_validate(&scene, &error));
    TEST_ASSERT_NOT_NULL(strstr(error.text, "one spot"));
    catalog[6].build(&scene);
    scene.bodies[1].debris_count = 32;
    scene.bodies[1].contents_count = 2;
    scene.bodies[2].debris_count = 32;
    TEST_ASSERT_TRUE_MESSAGE(body_sync_scene_validate(&scene, &error), error.text);
    /* 34 spawns are within one break's 40; 41 are not */
    catalog[6].build(&scene);
    while (scene.template_count < 9) {
        scene.templates[scene.template_count] = scene.templates[0];
        scene.template_count++;
    }
    scene.bodies[2].contents_first = 0;
    scene.bodies[2].contents_count = 9;
    scene.bodies[2].debris_count = 32;
    TEST_ASSERT_FALSE(body_sync_scene_validate(&scene, &error));
    TEST_ASSERT_NOT_NULL(strstr(error.text, "more than 40"));
    catalog[0].build(&scene);
    scene.bodies[2].rotation[3] = 0.5f;
    TEST_ASSERT_FALSE(body_sync_scene_validate(&scene, &error));
    catalog[0].build(&scene);
    snprintf(scene.bodies[2].name, sizeof(scene.bodies[2].name), "wall");
    TEST_ASSERT_FALSE(body_sync_scene_validate(&scene, &error));
    catalog[0].build(&scene);
    scene.bodies[1].flags = BODY_SYNC_BODY_GRABBABLE;
    TEST_ASSERT_FALSE(body_sync_scene_validate(&scene, &error));
    catalog[0].build(&scene);
    TEST_ASSERT_TRUE(scene.bodies[2].linear_damping == 0.0f && scene.bodies[2].angular_damping == 0.0f);
    scene.bodies[2].linear_damping = 0.1f;
    scene.bodies[2].angular_damping = 0.3f;
    TEST_ASSERT_TRUE_MESSAGE(body_sync_scene_validate(&scene, &error), error.text);
    scene.bodies[2].angular_damping = -0.1f;
    TEST_ASSERT_FALSE(body_sync_scene_validate(&scene, &error));
    TEST_ASSERT_NOT_NULL(strstr(error.text, "damping"));
    scene.bodies[2].angular_damping = 0.0f;
    scene.bodies[2].linear_damping = INFINITY;
    TEST_ASSERT_FALSE(body_sync_scene_validate(&scene, &error));
    catalog[6].build(&scene);
    scene.template_count = 1;
    TEST_ASSERT_FALSE(body_sync_scene_validate(&scene, &error));
    catalog[0].build(&scene);
    scene.body_count = BODY_SYNC_SCENE_BODIES_MAX + 1;
    TEST_ASSERT_FALSE(body_sync_scene_validate(&scene, &error));
}

/* A rest's end: a copy rests only drawn on its host body, and a copy that moves after its host body
   crept faster than the seam's creep bound (a copy-grid step, 3.4 mm, in under 8 ticks) is the
   host's wake, though that creep (40 mm/s) is under the run's rest speed (50 mm/s). */
static void test_a_copy_following_its_creeping_owner_is_no_break(void) {
    TEST_ASSERT_TRUE(stand_copy_rests(true, true, 0.009f, 0.01f));
    TEST_ASSERT_FALSE_MESSAGE(stand_copy_rests(true, true, 0.02f, 0.01f), "a copy drawn 2 cm off its host body rests");
    TEST_ASSERT_FALSE(stand_copy_rests(false, true, 0.0f, 0.01f));
    TEST_ASSERT_FALSE(stand_copy_rests(true, false, 0.0f, 0.01f));
    enum { N = 38 };
    float path[N][3];
    for (int speed_mm_s = 10; speed_mm_s <= 50; speed_mm_s += 10) {
        for (int k = 0; k < N; k++) {
            path[k][0] = (float)speed_mm_s * 0.001f * (float)k / 60.0f;
            path[k][1] = path[k][2] = 0.0f;
        }
        bool moved = stand_path_moved((const float (*)[3])path, N, 0.0034f, 8u);
        TEST_ASSERT_EQUAL_MESSAGE(speed_mm_s > 27, moved, "the seam's creep bound read wrong");
    }
    TEST_ASSERT_FALSE(stand_path_moved((const float (*)[3])path, 8u, 0.0034f, 8u)); /* too short to tell */
    /* In turn the bound is a rotation step, 0.0195 rad, in 8 ticks: 0.146 rad/s. */
    float turns[N][4];
    for (int mrad_s = 50; mrad_s <= 250; mrad_s += 50) {
        for (int k = 0; k < N; k++) {
            float half = 0.5f * (float)mrad_s * 0.001f * (float)k / 60.0f;
            turns[k][0] = turns[k][2] = 0.0f;
            turns[k][1] = sinf(half);
            turns[k][3] = cosf(half);
        }
        bool moved = stand_turns_moved((const float (*)[4])turns, N, 0.0195f, 8u);
        TEST_ASSERT_EQUAL_MESSAGE(mrad_s > 146, moved, "the seam's creep bound in turn read wrong");
    }
    TEST_ASSERT_TRUE_MESSAGE(stand_copy_breaks(true, false, false), "a copy moving off a still host body is no break");
    TEST_ASSERT_FALSE_MESSAGE(stand_copy_breaks(true, false, true), "a copy following its creeping host body is a break");
    TEST_ASSERT_FALSE(stand_copy_breaks(true, true, false));
    TEST_ASSERT_FALSE(stand_copy_breaks(false, false, false));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_a_copy_following_its_creeping_owner_is_no_break);
    RUN_TEST(test_shipped_scenarios_parse);
    RUN_TEST(test_parse_reads_the_grammar);
    RUN_TEST(test_host_rules_parse);
    RUN_TEST(test_net_items_sorted_by_tick);
    RUN_TEST(test_refusals_name_their_line);
    RUN_TEST(test_parser_fuzz);
    RUN_TEST(test_schedule_orders_actions);
    RUN_TEST(test_rules_verdicts);
    RUN_TEST(test_records_round_trip_and_refuse);
    RUN_TEST(test_impact_rules_by_role);
    RUN_TEST(test_impact_limits);
    RUN_TEST(test_claim_keys);
    RUN_TEST(test_impact_record_refusals);
    RUN_TEST(test_new_forms_are_read);
    RUN_TEST(test_unseen_delays_are_unbounded);
    RUN_TEST(test_a_throw_unseen_by_its_thrower_is_unbounded);
    RUN_TEST(test_a_driven_body_rests_by_its_window);
    RUN_TEST(test_histogram_quantiles);
    RUN_TEST(test_json_is_complete_and_repeatable);
    RUN_TEST(test_catalog_validates_and_broken_scenes_are_refused);
    return UNITY_END();
}
