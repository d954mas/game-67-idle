#include "features/scenes/scene_manager.h"

#include "unity.h"

#include <string.h>

enum {
    PROBE_ROOT = 1,
    PROBE_SCREEN = 2,
    PROBE_MODAL = 3,
};

enum {
    EVENT_EXIT = 10,
    EVENT_HIDE = 20,
    EVENT_PAUSE = 30,
    EVENT_RESUME = 40,
    EVENT_SHOW = 50,
    EVENT_ENTER = 60,
};

typedef struct event_log {
    int values[32];
    size_t count;
} event_log_t;

typedef struct ordering_scene {
    scene_manager_t *manager;
    event_log_t *log;
    int id;
    bool observe_queries;
    const char *top_seen_on_hide;
    const char *top_seen_on_show;
    const char *top_seen_on_pause;
    const char *top_seen_on_resume;
    int exits;
    int enters;
} ordering_scene_t;

static void record(ordering_scene_t *scene, int event) {
    TEST_ASSERT_LESS_THAN(32, scene->log->count);
    scene->log->values[scene->log->count++] = event + scene->id;
}

static scene_load_result_t ready(void *instance) {
    (void)instance;
    return SCENE_LOAD_READY;
}

static void shown(void *instance, scene_route_args_view_t args) {
    ordering_scene_t *scene = instance;
    (void)args;
    if (scene->observe_queries) {
        scene->top_seen_on_show =
            scene_manager_top(scene->manager).scene->id;
    }
    record(scene, EVENT_SHOW);
}

static void hidden(void *instance) {
    ordering_scene_t *scene = instance;
    if (scene->observe_queries) {
        scene->top_seen_on_hide =
            scene_manager_top(scene->manager).scene->id;
    }
    record(scene, EVENT_HIDE);
}

static void paused(void *instance) {
    ordering_scene_t *scene = instance;
    if (scene->observe_queries) {
        scene->top_seen_on_pause =
            scene_manager_top(scene->manager).scene->id;
    }
    record(scene, EVENT_PAUSE);
}

static void resumed(void *instance) {
    ordering_scene_t *scene = instance;
    if (scene->observe_queries) {
        scene->top_seen_on_resume =
            scene_manager_top(scene->manager).scene->id;
    }
    record(scene, EVENT_RESUME);
}

static void transition_begin(
    void *instance, scene_transition_direction_t direction) {
    ordering_scene_t *scene = instance;
    if (direction == SCENE_TRANSITION_EXIT) {
        ++scene->exits;
        record(scene, EVENT_EXIT);
    } else {
        ++scene->enters;
        record(scene, EVENT_ENTER);
    }
}

static scene_transition_result_t transition_step(
    void *instance, float dt) {
    (void)instance;
    (void)dt;
    return SCENE_TRANSITION_DONE;
}

static const scene_api_t k_api = {
    .load_step = ready,
    .on_show = shown,
    .on_hide = hidden,
    .on_pause = paused,
    .on_resume = resumed,
};

static const scene_transition_api_t k_transitions = {
    .begin = transition_begin,
    .step = transition_step,
};

static void assert_events(
    const event_log_t *log, const int *expected, size_t count) {
    TEST_ASSERT_EQUAL_UINT32(count, log->count);
    for (size_t index = 0; index < count; ++index) {
        TEST_ASSERT_EQUAL_INT(expected[index], log->values[index]);
    }
}

void setUp(void) {}
void tearDown(void) {}

static void test_atomic_callback_order_and_history_observations(void) {
    scene_manager_t manager;
    event_log_t log = {0};
    ordering_scene_t root = {
        .manager = &manager, .log = &log, .id = PROBE_ROOT,
        .observe_queries = true};
    ordering_scene_t screen = {
        .manager = &manager, .log = &log, .id = PROBE_SCREEN,
        .observe_queries = true};
    ordering_scene_t modal = {
        .manager = &manager, .log = &log, .id = PROBE_MODAL,
        .observe_queries = true};
    const scene_descriptor_t catalog[] = {
        {.id = "root", .kind = SCENE_KIND_SCREEN, .keep_loaded = true,
         .instance = &root, .api = &k_api, .transitions = &k_transitions},
        {.id = "screen", .kind = SCENE_KIND_SCREEN,
         .instance = &screen, .api = &k_api,
         .transitions = &k_transitions},
        {.id = "modal", .kind = SCENE_KIND_MODAL,
         .modal_update_policy = SCENE_MODAL_PAUSE_BELOW,
         .instance = &modal, .api = &k_api,
         .transitions = &k_transitions},
    };
    scene_operation_id_t operation;

    scene_manager_init(&manager, catalog, 3);
    (void)scene_manager_start(
        &manager, "root", (scene_route_args_view_t){0}, &operation, NULL);
    scene_manager_step(&manager, 1, 0.016F);
    log.count = 0;

    (void)scene_manager_show(
        &manager, "modal", (scene_route_args_view_t){0}, &operation, NULL);
    scene_manager_step(&manager, 2, 0.016F);
    const int show_modal[] = {
        EVENT_PAUSE + PROBE_ROOT,
        EVENT_SHOW + PROBE_MODAL,
        EVENT_ENTER + PROBE_MODAL,
    };
    assert_events(&log, show_modal, 3);
    TEST_ASSERT_EQUAL_STRING("modal", root.top_seen_on_pause);
    TEST_ASSERT_EQUAL_STRING("modal", modal.top_seen_on_show);
    log.count = 0;

    (void)scene_manager_back(&manager, 1, &operation, NULL);
    scene_manager_step(&manager, 3, 0.016F);
    const int close_modal[] = {
        EVENT_EXIT + PROBE_MODAL,
        EVENT_HIDE + PROBE_MODAL,
        EVENT_RESUME + PROBE_ROOT,
    };
    assert_events(&log, close_modal, 3);
    TEST_ASSERT_EQUAL_STRING("modal", modal.top_seen_on_hide);
    TEST_ASSERT_EQUAL_STRING("root", root.top_seen_on_resume);
    log.count = 0;

    (void)scene_manager_show(
        &manager, "screen", (scene_route_args_view_t){0}, &operation, NULL);
    scene_manager_step(&manager, 4, 0.016F);
    const int show_screen[] = {
        EVENT_EXIT + PROBE_ROOT,
        EVENT_HIDE + PROBE_ROOT,
        EVENT_SHOW + PROBE_SCREEN,
        EVENT_ENTER + PROBE_SCREEN,
    };
    assert_events(&log, show_screen, 4);
    TEST_ASSERT_EQUAL_STRING("root", root.top_seen_on_hide);
    TEST_ASSERT_EQUAL_STRING("screen", screen.top_seen_on_show);
    root.observe_queries = false;
    screen.observe_queries = false;
    modal.observe_queries = false;
    scene_manager_shutdown(&manager);
}

static void test_atomic_back_skips_hidden_scene_transitions(void) {
    scene_manager_t manager;
    event_log_t log = {0};
    ordering_scene_t root = {
        .manager = &manager, .log = &log, .id = PROBE_ROOT,
        .observe_queries = true};
    ordering_scene_t middle = {
        .manager = &manager, .log = &log, .id = PROBE_SCREEN,
        .observe_queries = true};
    ordering_scene_t top = {
        .manager = &manager, .log = &log, .id = PROBE_MODAL,
        .observe_queries = true};
    const scene_descriptor_t catalog[] = {
        {.id = "root", .kind = SCENE_KIND_SCREEN, .keep_loaded = true,
         .instance = &root, .api = &k_api, .transitions = &k_transitions},
        {.id = "middle", .kind = SCENE_KIND_SCREEN, .keep_loaded = true,
         .instance = &middle, .api = &k_api,
         .transitions = &k_transitions},
        {.id = "top", .kind = SCENE_KIND_SCREEN,
         .instance = &top, .api = &k_api, .transitions = &k_transitions},
    };
    scene_operation_id_t operation;

    scene_manager_init(&manager, catalog, 3);
    (void)scene_manager_start(
        &manager, "root", (scene_route_args_view_t){0}, &operation, NULL);
    scene_manager_step(&manager, 1, 0.016F);
    (void)scene_manager_show(
        &manager, "middle", (scene_route_args_view_t){0}, &operation, NULL);
    scene_manager_step(&manager, 2, 0.016F);
    (void)scene_manager_show(
        &manager, "top", (scene_route_args_view_t){0}, &operation, NULL);
    scene_manager_step(&manager, 3, 0.016F);
    const int middle_exits_before = middle.exits;
    const int middle_enters_before = middle.enters;

    (void)scene_manager_back(&manager, 2, &operation, NULL);
    scene_manager_step(&manager, 4, 0.016F);
    TEST_ASSERT_EQUAL_INT(middle_exits_before, middle.exits);
    TEST_ASSERT_EQUAL_INT(middle_enters_before, middle.enters);
    TEST_ASSERT_EQUAL_STRING("root", scene_manager_top(&manager).scene->id);
    root.observe_queries = false;
    middle.observe_queries = false;
    top.observe_queries = false;
    scene_manager_shutdown(&manager);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_atomic_callback_order_and_history_observations);
    RUN_TEST(test_atomic_back_skips_hidden_scene_transitions);
    return UNITY_END();
}
