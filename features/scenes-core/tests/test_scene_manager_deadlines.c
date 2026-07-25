#include <setjmp.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static jmp_buf s_assert_jump;
static bool s_capture_assert;
static const char *s_caught_expression;

static void deadline_test_assert(bool condition, const char *expression) {
    if (condition) {
        return;
    }
    if (s_capture_assert) {
        s_caught_expression = expression;
        longjmp(s_assert_jump, 1);
    }
    abort();
}

#define SCENE_MANAGER_ASSERT(condition) \
    deadline_test_assert((condition), #condition)
#include "../src/scene_manager.c"

#include "unity.h"

typedef struct pending_scene {
    bool load_ready;
} pending_scene_t;

static scene_manager_t s_manager;
static pending_scene_t s_scene;
static scene_descriptor_t s_catalog[1];

static scene_load_result_t pending_load_step(void *instance) {
    const pending_scene_t *scene = instance;
    return scene->load_ready ? SCENE_LOAD_READY : SCENE_LOAD_PENDING;
}

static scene_transition_result_t pending_transition_step(
    void *instance, float dt) {
    (void)instance;
    (void)dt;
    return SCENE_TRANSITION_PENDING;
}

static const scene_api_t k_loading_api = {
    .load_step = pending_load_step,
};

static const scene_transition_api_t k_pending_transition = {
    .step = pending_transition_step,
};

static bool catches_assert(void (*scenario)(void),
                           const char **expression) {
    s_capture_assert = true;
    s_caught_expression = NULL;
    if (setjmp(s_assert_jump) == 0) {
        scenario();
        s_capture_assert = false;
        if (expression != NULL) {
            *expression = NULL;
        }
        return false;
    }
    s_capture_assert = false;
    if (expression != NULL) {
        *expression = s_caught_expression;
    }
    return true;
}

static void step_same_frame(void) {
    scene_manager_step(&s_manager, 7, 0.016F);
}

static void setup_load_deadline(void) {
    memset(&s_scene, 0, sizeof s_scene);
    s_catalog[0] = (scene_descriptor_t){
        .id = "loading",
        .kind = SCENE_KIND_SCREEN,
        .instance = &s_scene,
        .api = &k_loading_api,
    };
    scene_manager_init(&s_manager, s_catalog, 1);
    (void)scene_manager_preload(&s_manager, "loading");
}

static void setup_transition_deadline(void) {
    scene_operation_id_t operation_id;
    memset(&s_scene, 0, sizeof s_scene);
    s_scene.load_ready = true;
    s_catalog[0] = (scene_descriptor_t){
        .id = "root",
        .kind = SCENE_KIND_SCREEN,
        .instance = &s_scene,
        .api = &k_loading_api,
        .transitions = &k_pending_transition,
    };
    scene_manager_init(&s_manager, s_catalog, 1);
    (void)scene_manager_start(
        &s_manager, "root", (scene_route_args_view_t){0},
        &operation_id, NULL);
}

static void assert_inclusive_deadline_then_failure(
    void (*setup)(void), const char *deadline_fragment) {
    const char *expression;
    setup();
    TEST_ASSERT_FALSE(catches_assert(step_same_frame, &expression));
    TEST_ASSERT_FALSE(catches_assert(step_same_frame, &expression));
    TEST_ASSERT_TRUE(catches_assert(step_same_frame, &expression));
    TEST_ASSERT_NOT_NULL(expression);
    TEST_ASSERT_NOT_NULL(strstr(expression, deadline_fragment));
}

static void test_load_deadline_counts_manager_steps_not_frame_values(void) {
    assert_inclusive_deadline_then_failure(
        setup_load_deadline,
        "SCENE_MANAGER_LOAD_DEADLINE_STEPS");
}

static void test_transition_deadline_counts_manager_steps_not_frame_values(void) {
    assert_inclusive_deadline_then_failure(
        setup_transition_deadline,
        "SCENE_MANAGER_TRANSITION_DEADLINE_STEPS");
}

void setUp(void) {}
void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_load_deadline_counts_manager_steps_not_frame_values);
    RUN_TEST(test_transition_deadline_counts_manager_steps_not_frame_values);
    return UNITY_END();
}
