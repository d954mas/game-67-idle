#include "features/scenes/scene_manager.h"

#include "unity.h"

typedef struct fake_scene {
    int begin_count;
    int step_count;
    int unload_count;
    int show_count;
    int hide_count;
    int transition_begin_count;
    int ready_after;
    int order_value;
} fake_scene_t;

static int s_step_order;

static void fake_load_begin(void *instance) {
    fake_scene_t *scene = instance;
    ++scene->begin_count;
}

static scene_load_result_t fake_load_step(void *instance) {
    fake_scene_t *scene = instance;
    ++scene->step_count;
    scene->order_value = ++s_step_order;
    return scene->step_count >= scene->ready_after ? SCENE_LOAD_READY
                                                   : SCENE_LOAD_PENDING;
}

static void fake_unload(void *instance) {
    fake_scene_t *scene = instance;
    ++scene->unload_count;
}

static void fake_show(void *instance, scene_route_args_view_t args) {
    fake_scene_t *scene = instance;
    (void)args;
    ++scene->show_count;
}

static void fake_hide(void *instance) {
    fake_scene_t *scene = instance;
    ++scene->hide_count;
}

static void fake_transition_begin(
    void *instance, scene_transition_direction_t direction) {
    fake_scene_t *scene = instance;
    (void)direction;
    ++scene->transition_begin_count;
}

static scene_transition_result_t fake_transition_pending(
    void *instance, float dt) {
    (void)instance;
    (void)dt;
    return SCENE_TRANSITION_PENDING;
}

static const scene_api_t k_api = {
    .load_begin = fake_load_begin,
    .load_step = fake_load_step,
    .unload = fake_unload,
    .on_show = fake_show,
    .on_hide = fake_hide,
};

static const scene_transition_api_t k_pending_transition = {
    .begin = fake_transition_begin,
    .step = fake_transition_pending,
};

void setUp(void) { s_step_order = 0; }
void tearDown(void) {}

static void test_preload_is_a_deterministic_hint_and_shutdown_unloads(void) {
    fake_scene_t first = {.ready_after = 2};
    fake_scene_t second = {.ready_after = 1};
    const scene_descriptor_t catalog[] = {
        {.id = "first", .kind = SCENE_KIND_SCREEN, .instance = &first,
         .api = &k_api},
        {.id = "second", .kind = SCENE_KIND_SCREEN, .instance = &second,
         .api = &k_api},
    };
    scene_manager_t manager;

    scene_manager_init(&manager, catalog, 2);
    TEST_ASSERT_EQUAL(SCENE_PRELOAD_SCHEDULED,
                      scene_manager_preload(&manager, "second"));
    TEST_ASSERT_EQUAL(SCENE_PRELOAD_SCHEDULED,
                      scene_manager_preload(&manager, "first"));
    TEST_ASSERT_EQUAL(SCENE_PRELOAD_ALREADY_SCHEDULED,
                      scene_manager_preload(&manager, "first"));

    scene_manager_step(&manager, 1, 0.016F);
    TEST_ASSERT_EQUAL_INT(1, first.begin_count);
    TEST_ASSERT_EQUAL_INT(1, second.begin_count);
    TEST_ASSERT_TRUE(first.order_value < second.order_value);
    TEST_ASSERT_EQUAL(SCENE_RESIDENCY_LOADING,
                      scene_manager_scene_residency(&manager, &catalog[0]));
    TEST_ASSERT_EQUAL(SCENE_RESIDENCY_READY,
                      scene_manager_scene_residency(&manager, &catalog[1]));
    TEST_ASSERT_EQUAL(SCENE_PRELOAD_ALREADY_LOADING,
                      scene_manager_preload(&manager, "first"));
    TEST_ASSERT_EQUAL(SCENE_PRELOAD_ALREADY_READY,
                      scene_manager_preload(&manager, "second"));

    scene_manager_step(&manager, 2, 0.016F);
    TEST_ASSERT_EQUAL(SCENE_RESIDENCY_READY,
                      scene_manager_scene_residency(&manager, &catalog[0]));

    scene_manager_shutdown(&manager);
    TEST_ASSERT_EQUAL_INT(1, first.unload_count);
    TEST_ASSERT_EQUAL_INT(1, second.unload_count);
}

static void test_shutdown_cleans_partially_loading_scene_once(void) {
    fake_scene_t loading = {.ready_after = 100};
    const scene_descriptor_t catalog[] = {
        {.id = "loading", .kind = SCENE_KIND_SCREEN, .instance = &loading,
         .api = &k_api},
    };
    scene_manager_t manager;

    scene_manager_init(&manager, catalog, 1);
    (void)scene_manager_preload(&manager, "loading");
    scene_manager_step(&manager, 1, 0.016F);
    scene_manager_shutdown(&manager);

    TEST_ASSERT_EQUAL_INT(1, loading.unload_count);
}

static void test_shutdown_unloads_hidden_retained_and_visible_scenes_once(void) {
    fake_scene_t root = {.ready_after = 1};
    fake_scene_t next = {.ready_after = 1};
    const scene_descriptor_t catalog[] = {
        {.id = "root", .kind = SCENE_KIND_SCREEN, .keep_loaded = true,
         .instance = &root, .api = &k_api},
        {.id = "next", .kind = SCENE_KIND_SCREEN,
         .instance = &next, .api = &k_api},
    };
    scene_manager_t manager;

    scene_manager_init(&manager, catalog, 2);
    (void)scene_manager_start(
        &manager, "root", (scene_route_args_view_t){0},
        NULL, NULL);
    scene_manager_step(&manager, 1, 0.016F);
    (void)scene_manager_show(
        &manager, "next", (scene_route_args_view_t){0},
        NULL, NULL);
    scene_manager_step(&manager, 2, 0.016F);

    TEST_ASSERT_EQUAL_INT(1, root.hide_count);
    TEST_ASSERT_EQUAL_INT(0, root.unload_count);
    TEST_ASSERT_FALSE(scene_manager_is_presented(&manager, &catalog[0]));
    TEST_ASSERT_TRUE(scene_manager_is_presented(&manager, &catalog[1]));
    scene_manager_shutdown(&manager);

    TEST_ASSERT_EQUAL_INT(1, root.unload_count);
    TEST_ASSERT_EQUAL_INT(1, next.hide_count);
    TEST_ASSERT_EQUAL_INT(1, next.unload_count);
}

static void test_shutdown_during_target_loading_preserves_unload_once(void) {
    fake_scene_t root = {.ready_after = 1};
    fake_scene_t loading = {.ready_after = 100};
    const scene_descriptor_t catalog[] = {
        {.id = "root", .kind = SCENE_KIND_SCREEN,
         .instance = &root, .api = &k_api},
        {.id = "loading", .kind = SCENE_KIND_SCREEN,
         .instance = &loading, .api = &k_api},
    };
    scene_manager_t manager;

    scene_manager_init(&manager, catalog, 2);
    (void)scene_manager_start(
        &manager, "root", (scene_route_args_view_t){0},
        NULL, NULL);
    scene_manager_step(&manager, 1, 0.016F);
    (void)scene_manager_show(
        &manager, "loading", (scene_route_args_view_t){0},
        NULL, NULL);
    scene_manager_step(&manager, 2, 0.016F);

    TEST_ASSERT_EQUAL(SCENE_RESIDENCY_LOADING,
                      scene_manager_scene_residency(
                          &manager, &catalog[1]));
    TEST_ASSERT_TRUE(scene_manager_is_presented(&manager, &catalog[0]));
    scene_manager_shutdown(&manager);

    TEST_ASSERT_EQUAL_INT(1, root.hide_count);
    TEST_ASSERT_EQUAL_INT(1, root.unload_count);
    TEST_ASSERT_EQUAL_INT(1, loading.unload_count);
}

static void test_shutdown_in_reload_gap_does_not_double_unload(void) {
    fake_scene_t root = {.ready_after = 1};
    const scene_descriptor_t catalog[] = {
        {.id = "root", .kind = SCENE_KIND_SCREEN,
         .instance = &root, .api = &k_api},
    };
    scene_manager_t manager;

    scene_manager_init(&manager, catalog, 1);
    (void)scene_manager_start(
        &manager, "root", (scene_route_args_view_t){0},
        NULL, NULL);
    scene_manager_step(&manager, 1, 0.016F);
    (void)scene_manager_reload(&manager, NULL, NULL);
    scene_manager_step(&manager, 2, 0.016F);
    TEST_ASSERT_EQUAL_INT(1, root.hide_count);
    TEST_ASSERT_EQUAL_INT(1, root.unload_count);

    scene_manager_shutdown(&manager);
    TEST_ASSERT_EQUAL_INT(1, root.hide_count);
    TEST_ASSERT_EQUAL_INT(1, root.unload_count);
}

static void test_shutdown_during_enter_transition_unloads_without_show(void) {
    fake_scene_t root = {.ready_after = 1};
    const scene_descriptor_t catalog[] = {
        {.id = "root", .kind = SCENE_KIND_SCREEN,
         .instance = &root, .api = &k_api,
         .transitions = &k_pending_transition},
    };
    scene_manager_t manager;

    scene_manager_init(&manager, catalog, 1);
    (void)scene_manager_start(
        &manager, "root", (scene_route_args_view_t){0},
        NULL, NULL);
    scene_manager_step(&manager, 1, 0.016F);
    TEST_ASSERT_EQUAL_INT(1, root.transition_begin_count);
    TEST_ASSERT_EQUAL_INT(1, root.show_count);

    scene_manager_shutdown(&manager);
    TEST_ASSERT_EQUAL_INT(1, root.hide_count);
    TEST_ASSERT_EQUAL_INT(1, root.unload_count);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_preload_is_a_deterministic_hint_and_shutdown_unloads);
    RUN_TEST(test_shutdown_cleans_partially_loading_scene_once);
    RUN_TEST(test_shutdown_unloads_hidden_retained_and_visible_scenes_once);
    RUN_TEST(test_shutdown_during_target_loading_preserves_unload_once);
    RUN_TEST(test_shutdown_in_reload_gap_does_not_double_unload);
    RUN_TEST(test_shutdown_during_enter_transition_unloads_without_show);
    return UNITY_END();
}
