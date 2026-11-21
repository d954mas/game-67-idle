#define _CRT_SECURE_NO_WARNINGS
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "features/body_sync/body_sync_stand.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static body_sync_scene_t scene;
static body_sync_scenario_t scenario;

/* The count is filled before it is read: in one call the order of arguments is unspecified. */
static const body_sync_scene_entry_t *scene_named(const char *name) {
    uint32_t count = 0;
    const body_sync_scene_entry_t *catalog = body_sync_scene_catalog(&count);
    return body_sync_scene_lookup(catalog, count, name);
}

static body_sync_stand_t *stand_for(const char *scene_name, uint32_t guests) {
    char text[256];
    snprintf(text, sizeof(text), "scenario build\nscene %s\nguests %u\nend 10t\nexpect no_stuck\n", scene_name, guests);
    body_sync_error_t error;
    TEST_ASSERT_TRUE_MESSAGE(body_sync_scenario_parse(text, strlen(text), &scenario, &error), error.text);
    const body_sync_scene_entry_t *entry = scene_named(scene_name);
    TEST_ASSERT_NOT_NULL(entry);
    entry->build(&scene);
    body_sync_stand_config_t config = {.scene = &scene, .scenario = &scenario};
    body_sync_stand_t *stand = body_sync_stand_create(&config, &error);
    TEST_ASSERT_NOT_NULL_MESSAGE(stand, error.text);
    return stand;
}

static void test_every_scene_builds_identically_on_every_endpoint(void) {
    uint32_t count = 0;
    const body_sync_scene_entry_t *catalog = body_sync_scene_catalog(&count);
    for (uint32_t i = 0; i < count; i++) {
        body_sync_stand_t *stand = stand_for(catalog[i].name, 3);
        TEST_ASSERT_EQUAL_UINT32(4, body_sync_stand_endpoint_count(stand));
        uint64_t first = body_sync_stand_world_hash(stand, 0);
        TEST_ASSERT_TRUE(first != 0);
        for (uint32_t e = 1; e < 4; e++) {
            TEST_ASSERT_EQUAL_UINT64_MESSAGE(first, body_sync_stand_world_hash(stand, e), catalog[i].name);
        }
        TEST_ASSERT_EQUAL_UINT32(body_sync_stand_body_count(stand, 0), body_sync_stand_body_count(stand, 3));
        body_sync_stand_destroy(stand);
    }
}

/* A prop's damping is built into every endpoint's world and seen by the world hash. */
static void test_damping_is_built_and_hashed(void) {
    body_sync_stand_t *plain = stand_for("two_chars_item", 1);
    uint64_t plain_hash = body_sync_stand_world_hash(plain, 0);
    body_sync_stand_destroy(plain);
    body_sync_error_t error;
    scene_named("two_chars_item")->build(&scene);
    scene.bodies[1].linear_damping = 0.1f;
    scene.bodies[1].angular_damping = 0.3f;
    body_sync_stand_config_t config = {.scene = &scene, .scenario = &scenario};
    body_sync_stand_t *damped = body_sync_stand_create(&config, &error);
    TEST_ASSERT_NOT_NULL_MESSAGE(damped, error.text);
    TEST_ASSERT_TRUE(body_sync_stand_world_hash(damped, 0) != plain_hash);
    TEST_ASSERT_EQUAL_UINT64(body_sync_stand_world_hash(damped, 0), body_sync_stand_world_hash(damped, 1));
    body_sync_stand_destroy(damped);
}

static void test_view_walks_one_endpoint(void) {
    body_sync_stand_t *stand = stand_for("two_chars_item", 1);
    body_sync_stand_tick(stand);
    uint32_t characters = 0, copies = 0, statics = 0;
    for (uint32_t e = 0; e < 2; e++) {
        for (uint32_t i = 0; i < body_sync_stand_body_count(stand, e); i++) {
            body_sync_body_view_t v;
            TEST_ASSERT_TRUE(body_sync_stand_body(stand, e, i, &v));
            TEST_ASSERT_TRUE(isfinite(v.position[0]) && isfinite(v.position[1]) && isfinite(v.position[2]));
            float norm = v.rotation[0] * v.rotation[0] + v.rotation[1] * v.rotation[1] + v.rotation[2] * v.rotation[2] + v.rotation[3] * v.rotation[3];
            TEST_ASSERT_TRUE(fabsf(norm - 1.0f) < 1e-3f);
            TEST_ASSERT_NOT_NULL(v.name);
            characters += (v.flags & BODY_SYNC_VIEW_CHARACTER) ? 1u : 0u;
            statics += (v.flags & BODY_SYNC_VIEW_STATIC) ? 1u : 0u;
            if ((v.flags & BODY_SYNC_VIEW_CHARACTER) && (v.flags & BODY_SYNC_VIEW_COPY)) {
                copies++;
                TEST_ASSERT_TRUE(v.player != e);
            }
        }
    }
    TEST_ASSERT_EQUAL_UINT32(4, characters); /* two players drawn on two endpoints */
    TEST_ASSERT_EQUAL_UINT32(2, copies);     /* each endpoint draws the other's player as a copy */
    TEST_ASSERT_EQUAL_UINT32(2, statics);
    body_sync_body_view_t v;
    TEST_ASSERT_FALSE(body_sync_stand_body(stand, 0, 9999, &v));
    TEST_ASSERT_FALSE(body_sync_stand_body(stand, 7, 0, &v));
    body_sync_stand_destroy(stand);
}

static char *run_two_walk(size_t *size, const body_sync_report_t **report_out, body_sync_stand_t **keep) {
    body_sync_error_t error;
    TEST_ASSERT_TRUE_MESSAGE(body_sync_scenario_load(BODY_SYNC_SCENARIOS_DIR "/two_walk.scn", &scenario, &error), error.text);
    scene_named(scenario.scene)->build(&scene);
    body_sync_stand_config_t config = {.scene = &scene, .scenario = &scenario, .series = true};
    body_sync_stand_t *stand = body_sync_stand_create(&config, &error);
    TEST_ASSERT_NOT_NULL_MESSAGE(stand, error.text);
    const body_sync_report_t *report = body_sync_stand_run(stand);
    body_sync_stand_write_json(stand, NULL, 0, size);
    char *json = malloc(*size + 1);
    TEST_ASSERT_TRUE(body_sync_stand_write_json(stand, json, *size + 1, size));
    if (report_out) {
        *report_out = report;
    }
    if (keep) {
        *keep = stand;
    } else {
        body_sync_stand_destroy(stand);
    }
    return json;
}

static void test_two_walk_runs_on_the_stub_and_writes_metrics(void) {
    size_t size = 0;
    const body_sync_report_t *report;
    body_sync_stand_t *stand;
    char *json = run_two_walk(&size, &report, &stand);
    TEST_ASSERT_TRUE(report->ticks > 60);
    TEST_ASSERT_EQUAL_UINT32(1, report->guest_count);
    const body_sync_guest_metrics_t *g = &report->guests[0];
    TEST_ASSERT_TRUE(isfinite(g->divergence_p95_m) && g->divergence_p95_m >= 0.0 && g->divergence_p95_m <= g->divergence_max_m + 1e-9);
    double down = 0.0;
    for (uint32_t k = 0; k < BODY_SYNC_KINDS_ALL; k++) {
        down += g->to_guest.mean_bytes_per_s[k];
    }
    TEST_ASSERT_TRUE(down > 0.0);
    for (uint32_t i = 0; i < report->check_count; i++) {
        TEST_ASSERT_TRUE_MESSAGE(report->checks[i].passed, report->checks[i].detail);
    }
    TEST_ASSERT_TRUE(report->passed);
    TEST_ASSERT_TRUE(body_sync_stand_save_json(stand, "two_walk_in_process.json", NULL));
    FILE *file = fopen("two_walk_in_process.json", "rb");
    TEST_ASSERT_NOT_NULL(file);
    fseek(file, 0, SEEK_END);
    TEST_ASSERT_EQUAL_INT64((long)size, ftell(file));
    fclose(file);
    TEST_ASSERT_NOT_NULL(strstr(json, "\"schema\":\"body_sync.metrics.v1\""));
    body_sync_stand_destroy(stand);
    free(json);
}

static void test_one_seed_twice_in_one_process_gives_equal_metrics(void) {
    size_t a_size = 0, b_size = 0;
    char *a = run_two_walk(&a_size, NULL, NULL);
    char *b = run_two_walk(&b_size, NULL, NULL);
    TEST_ASSERT_EQUAL_size_t(a_size, b_size);
    TEST_ASSERT_EQUAL_MEMORY(a, b, a_size);
    free(a);
    free(b);
}

/* A character's facing and activity reach the other endpoint through the seam: the guest draws
   the host's player turned where it walked, and the host sees the guest's carry, throw and punch. */
static void test_facing_and_activity_travel_to_the_other_endpoint(void) {
    const char *text = "scenario facing\nscene two_chars_item\nguests 1\nsettle 1s\n"
                       "p0 walk -5 0\n"
                       "p1 walk box\np1 grab box\np1 wait 300ms\np1 throw 4 0.5 -3\np1 punch\n"
                       "expect no_stuck\n";
    body_sync_error_t error;
    TEST_ASSERT_TRUE_MESSAGE(body_sync_scenario_parse(text, strlen(text), &scenario, &error), error.text);
    scene_named("two_chars_item")->build(&scene);
    body_sync_stand_config_t config = {.scene = &scene, .scenario = &scenario};
    body_sync_stand_t *stand = body_sync_stand_create(&config, &error);
    TEST_ASSERT_NOT_NULL_MESSAGE(stand, error.text);
    uint32_t seen[2][2] = {{0, 0}, {0, 0}}; /* [endpoint][player]: a bit per activity */
    float yaw_y[2] = {0.0f, 0.0f};          /* p0's rotation.y on each endpoint at the end */
    bool more = true;
    for (uint32_t tick = 0; more && tick < 1200; tick++) {
        more = body_sync_stand_tick(stand);
        for (uint32_t e = 0; e < 2; e++) {
            for (uint32_t i = 0; i < body_sync_stand_body_count(stand, e); i++) {
                body_sync_body_view_t v;
                TEST_ASSERT_TRUE(body_sync_stand_body(stand, e, i, &v));
                if (!(v.flags & BODY_SYNC_VIEW_CHARACTER)) {
                    TEST_ASSERT_EQUAL_INT(BODY_SYNC_ACTIVITY_NONE, v.activity);
                    continue;
                }
                TEST_ASSERT_TRUE(v.activity >= BODY_SYNC_ACTIVITY_IDLE && v.activity <= BODY_SYNC_ACTIVITY_PUNCH);
                seen[e][v.player] |= 1u << v.activity;
                if (v.player == 0) {
                    yaw_y[e] = v.rotation[1];
                }
            }
        }
    }
    TEST_ASSERT_FALSE(more);
    TEST_ASSERT_TRUE(seen[1][0] & (1u << BODY_SYNC_ACTIVITY_WALK));
    TEST_ASSERT_TRUE(seen[1][0] & (1u << BODY_SYNC_ACTIVITY_IDLE));
    TEST_ASSERT_TRUE(seen[0][1] & (1u << BODY_SYNC_ACTIVITY_CARRY));
    TEST_ASSERT_TRUE(seen[0][1] & (1u << BODY_SYNC_ACTIVITY_THROW));
    TEST_ASSERT_TRUE(seen[0][1] & (1u << BODY_SYNC_ACTIVITY_PUNCH));
    /* p0 started facing +X and walked to -X: a yaw of -90 degrees, the same on both. */
    TEST_ASSERT_TRUE(fabsf(yaw_y[0] + 0.7071f) < 0.02f);
    TEST_ASSERT_TRUE(fabsf(yaw_y[0] - yaw_y[1]) < 0.02f);
    body_sync_stand_destroy(stand);
}

static void test_create_refuses_what_the_scene_lacks(void) {
    const char *texts[] = {
        "scenario x\nscene two_chars_item\np1 grab nothing_here\nexpect no_stuck\n",
        "scenario x\nscene stack10\nguests 1\np1 drop\nexpect no_stuck\n",
    };
    const char *scenes[] = {"two_chars_item", "two_chars_item"};
    for (int i = 0; i < 2; i++) {
        body_sync_error_t error;
        TEST_ASSERT_TRUE_MESSAGE(body_sync_scenario_parse(texts[i], strlen(texts[i]), &scenario, &error), error.text);
        scene_named(scenes[i])->build(&scene);
        body_sync_stand_config_t config = {.scene = &scene, .scenario = &scenario};
        TEST_ASSERT_NULL(body_sync_stand_create(&config, &error));
        TEST_ASSERT_TRUE(error.text[0] != '\0');
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_every_scene_builds_identically_on_every_endpoint);
    RUN_TEST(test_damping_is_built_and_hashed);
    RUN_TEST(test_view_walks_one_endpoint);
    RUN_TEST(test_two_walk_runs_on_the_stub_and_writes_metrics);
    RUN_TEST(test_one_seed_twice_in_one_process_gives_equal_metrics);
    RUN_TEST(test_facing_and_activity_travel_to_the_other_endpoint);
    RUN_TEST(test_create_refuses_what_the_scene_lacks);
    return UNITY_END();
}
