#include "features/scenes/scene_manager.h"

#include "unity.h"

#include <string.h>

typedef struct presentation_scene {
    int load_begins;
    int load_steps;
    int unloads;
    int updates;
    int pauses;
    int resumes;
    int ui_calls;
    int shows;
    int hides;
    int enters;
    int exits;
    unsigned shown_value;
    scene_ui_mode_t last_ui_mode;
} presentation_scene_t;

typedef struct presentation_args {
    unsigned value;
} presentation_args_t;

static void load_begin(void *instance) {
    presentation_scene_t *scene = instance;
    ++scene->load_begins;
}

static scene_load_result_t ready(void *instance) {
    presentation_scene_t *scene = instance;
    ++scene->load_steps;
    return SCENE_LOAD_READY;
}

static void unloaded(void *instance) {
    presentation_scene_t *scene = instance;
    ++scene->unloads;
}

static void updated(void *instance, float dt) {
    presentation_scene_t *scene = instance;
    (void)dt;
    ++scene->updates;
}

static void paused(void *instance) {
    presentation_scene_t *scene = instance;
    ++scene->pauses;
}

static void resumed(void *instance) {
    presentation_scene_t *scene = instance;
    ++scene->resumes;
}

static void shown(void *instance, scene_route_args_view_t args) {
    presentation_scene_t *scene = instance;
    ++scene->shows;
    if (args.size > 0) {
        presentation_args_t copied;
        memcpy(&copied, args.data, sizeof copied);
        scene->shown_value = copied.value;
    }
}

static void hidden(void *instance) {
    presentation_scene_t *scene = instance;
    ++scene->hides;
}

static void ui(void *instance, void *context, scene_ui_mode_t mode) {
    presentation_scene_t *scene = instance;
    (void)context;
    ++scene->ui_calls;
    scene->last_ui_mode = mode;
}

static void transition_begin(void *instance,
                             scene_transition_direction_t direction) {
    presentation_scene_t *scene = instance;
    if (direction == SCENE_TRANSITION_ENTER) {
        ++scene->enters;
    } else {
        ++scene->exits;
    }
}

static scene_transition_result_t transition_step(void *instance, float dt) {
    (void)instance;
    (void)dt;
    return SCENE_TRANSITION_DONE;
}

static const scene_api_t k_api = {
    .load_begin = load_begin,
    .load_step = ready,
    .unload = unloaded,
    .on_show = shown,
    .on_hide = hidden,
    .on_pause = paused,
    .on_resume = resumed,
    .on_update = updated,
    .on_ui = ui,
};

static const scene_transition_api_t k_transitions = {
    .begin = transition_begin,
    .step = transition_step,
};

void setUp(void) {}
void tearDown(void) {}

static void test_modal_policy_input_and_passive_ui(void) {
    presentation_scene_t root = {0};
    presentation_scene_t modal = {0};
    const scene_descriptor_t catalog[] = {
        {.id = "root", .kind = SCENE_KIND_SCREEN, .keep_loaded = true,
         .instance = &root, .api = &k_api, .transitions = &k_transitions},
        {.id = "modal", .kind = SCENE_KIND_MODAL,
         .modal_update_policy = SCENE_MODAL_PAUSE_BELOW,
         .instance = &modal, .api = &k_api, .transitions = &k_transitions},
    };
    scene_manager_t manager;
    scene_operation_id_t operation;

    scene_manager_init(&manager, catalog, 2);
    (void)scene_manager_start(&manager, "root",
                              (scene_route_args_view_t){0}, &operation, NULL);
    scene_manager_step(&manager, 1, 0.016F);
    TEST_ASSERT_EQUAL_INT(1, root.enters);
    TEST_ASSERT_EQUAL_INT(0, root.exits);
    scene_manager_update(&manager, 0.016F);
    scene_manager_build_ui(&manager, NULL);
    TEST_ASSERT_EQUAL_INT(1, root.updates);
    TEST_ASSERT_EQUAL(SCENE_UI_INTERACTIVE, root.last_ui_mode);

    (void)scene_manager_show(&manager, "modal",
                             (scene_route_args_view_t){0}, &operation, NULL);
    scene_manager_build_ui(&manager, NULL);
    TEST_ASSERT_EQUAL(SCENE_UI_PASSIVE, root.last_ui_mode);
    scene_manager_step(&manager, 2, 0.016F);
    TEST_ASSERT_EQUAL_INT(0, root.exits);
    TEST_ASSERT_EQUAL_INT(1, modal.enters);
    TEST_ASSERT_EQUAL_INT(1, root.pauses);

    scene_manager_update(&manager, 0.016F);
    scene_manager_build_ui(&manager, NULL);
    TEST_ASSERT_EQUAL_INT(1, root.updates);
    TEST_ASSERT_EQUAL_INT(1, modal.updates);
    TEST_ASSERT_EQUAL(SCENE_UI_PASSIVE, root.last_ui_mode);
    TEST_ASSERT_EQUAL(SCENE_UI_INTERACTIVE, modal.last_ui_mode);
    TEST_ASSERT_FALSE(scene_manager_can_process_input(&manager, &catalog[0]));
    TEST_ASSERT_TRUE(scene_manager_can_process_input(&manager, &catalog[1]));

    (void)scene_manager_back(&manager, 1, &operation, NULL);
    scene_manager_step(&manager, 3, 0.016F);
    TEST_ASSERT_EQUAL_INT(1, modal.exits);
    TEST_ASSERT_EQUAL_INT(1, root.enters);
    TEST_ASSERT_EQUAL_INT(1, root.resumes);
    scene_manager_shutdown(&manager);
}

static void test_reactivated_screen_pairs_hide_show_without_resume(void) {
    presentation_scene_t root = {0};
    presentation_scene_t modal = {0};
    const scene_descriptor_t catalog[] = {
        {.id = "root", .kind = SCENE_KIND_SCREEN, .keep_loaded = true,
         .route_args_size = sizeof(presentation_args_t),
         .instance = &root, .api = &k_api},
        {.id = "modal", .kind = SCENE_KIND_MODAL,
         .modal_update_policy = SCENE_MODAL_PAUSE_BELOW,
         .instance = &modal, .api = &k_api},
    };
    scene_manager_t manager;
    scene_operation_id_t operation;
    presentation_args_t first = {.value = 1};
    presentation_args_t second = {.value = 2};

    scene_manager_init(&manager, catalog, 2);
    (void)scene_manager_start(
        &manager, "root",
        (scene_route_args_view_t){.data = &first, .size = sizeof first},
        &operation, NULL);
    scene_manager_step(&manager, 1, 0.016F);
    (void)scene_manager_show(
        &manager, "modal", (scene_route_args_view_t){0},
        &operation, NULL);
    scene_manager_step(&manager, 2, 0.016F);
    TEST_ASSERT_EQUAL_INT(1, root.pauses);

    (void)scene_manager_show(
        &manager, "root",
        (scene_route_args_view_t){.data = &second, .size = sizeof second},
        &operation, NULL);
    scene_manager_step(&manager, 3, 0.016F);

    TEST_ASSERT_EQUAL_INT(1, root.hides);
    TEST_ASSERT_EQUAL_INT(2, root.shows);
    TEST_ASSERT_EQUAL_INT(0, root.resumes);
    TEST_ASSERT_EQUAL_UINT32(2, root.shown_value);
    TEST_ASSERT_EQUAL_INT(1, modal.hides);
    TEST_ASSERT_EQUAL_UINT32(2, scene_manager_history_count(&manager));
    TEST_ASSERT_EQUAL_STRING("root",
                             scene_manager_top(&manager).scene->id);
    scene_manager_shutdown(&manager);
}

static void test_reactivated_modal_runs_exit_and_enter_transitions(void) {
    presentation_scene_t root = {0};
    presentation_scene_t modal = {0};
    const scene_descriptor_t catalog[] = {
        {.id = "root", .kind = SCENE_KIND_SCREEN, .keep_loaded = true,
         .instance = &root, .api = &k_api},
        {.id = "modal", .kind = SCENE_KIND_MODAL,
         .modal_update_policy = SCENE_MODAL_CONTINUE_BELOW,
         .route_args_size = sizeof(presentation_args_t),
         .instance = &modal, .api = &k_api,
         .transitions = &k_transitions},
    };
    scene_manager_t manager;
    scene_operation_id_t operation;
    presentation_args_t first = {.value = 1};
    presentation_args_t second = {.value = 2};

    scene_manager_init(&manager, catalog, 2);
    (void)scene_manager_start(
        &manager, "root", (scene_route_args_view_t){0},
        &operation, NULL);
    scene_manager_step(&manager, 1, 0.016F);
    (void)scene_manager_show(
        &manager, "modal",
        (scene_route_args_view_t){.data = &first, .size = sizeof first},
        &operation, NULL);
    scene_manager_step(&manager, 2, 0.016F);
    TEST_ASSERT_EQUAL_INT(1, modal.enters);

    (void)scene_manager_show(
        &manager, "modal",
        (scene_route_args_view_t){.data = &second, .size = sizeof second},
        &operation, NULL);
    scene_manager_step(&manager, 3, 0.016F);
    TEST_ASSERT_EQUAL_INT(1, modal.exits);
    TEST_ASSERT_EQUAL_INT(2, modal.enters);

    (void)scene_manager_back(&manager, 1, &operation, NULL);
    scene_manager_step(&manager, 4, 0.016F);
    TEST_ASSERT_EQUAL_INT(2, modal.exits);
    TEST_ASSERT_EQUAL_INT(3, modal.enters);
    TEST_ASSERT_EQUAL_UINT32(1, modal.shown_value);
    scene_manager_shutdown(&manager);
}

static void test_any_pause_modal_wins_and_continue_resumes_after_pop(void) {
    presentation_scene_t root = {0};
    presentation_scene_t continuing = {0};
    presentation_scene_t pausing = {0};
    const scene_descriptor_t catalog[] = {
        {.id = "root", .kind = SCENE_KIND_SCREEN, .keep_loaded = true,
         .instance = &root, .api = &k_api},
        {.id = "continuing", .kind = SCENE_KIND_MODAL,
         .modal_update_policy = SCENE_MODAL_CONTINUE_BELOW,
         .instance = &continuing, .api = &k_api},
        {.id = "pausing", .kind = SCENE_KIND_MODAL,
         .modal_update_policy = SCENE_MODAL_PAUSE_BELOW,
         .instance = &pausing, .api = &k_api},
    };
    scene_manager_t manager;
    scene_operation_id_t operation;

    scene_manager_init(&manager, catalog, 3);
    (void)scene_manager_start(
        &manager, "root", (scene_route_args_view_t){0}, &operation, NULL);
    scene_manager_step(&manager, 1, 0.016F);
    (void)scene_manager_show(
        &manager, "continuing", (scene_route_args_view_t){0},
        &operation, NULL);
    scene_manager_step(&manager, 2, 0.016F);
    scene_manager_update(&manager, 0.016F);
    TEST_ASSERT_EQUAL_INT(1, root.updates);
    TEST_ASSERT_EQUAL_INT(0, root.pauses);

    (void)scene_manager_show(
        &manager, "pausing", (scene_route_args_view_t){0},
        &operation, NULL);
    scene_manager_step(&manager, 3, 0.016F);
    scene_manager_update(&manager, 0.016F);
    TEST_ASSERT_EQUAL_INT(1, root.updates);
    TEST_ASSERT_EQUAL_INT(1, root.pauses);

    (void)scene_manager_back(&manager, 1, &operation, NULL);
    scene_manager_step(&manager, 4, 0.016F);
    scene_manager_update(&manager, 0.016F);
    TEST_ASSERT_EQUAL_INT(2, root.updates);
    TEST_ASSERT_EQUAL_INT(1, root.resumes);
    TEST_ASSERT_TRUE(
        scene_manager_can_process_input(&manager, &catalog[1]));
    scene_manager_shutdown(&manager);
}

static void test_modal_reload_preserves_args_and_suppresses_modal_dispatch(void) {
    presentation_scene_t root = {0};
    presentation_scene_t modal = {0};
    const scene_descriptor_t catalog[] = {
        {.id = "root", .kind = SCENE_KIND_SCREEN, .keep_loaded = true,
         .instance = &root, .api = &k_api},
        {.id = "modal", .kind = SCENE_KIND_MODAL,
         .modal_update_policy = SCENE_MODAL_PAUSE_BELOW,
         .route_args_size = sizeof(presentation_args_t),
         .instance = &modal, .api = &k_api},
    };
    scene_manager_t manager;
    scene_operation_id_t operation;
    presentation_args_t args = {.value = 17};

    scene_manager_init(&manager, catalog, 2);
    (void)scene_manager_start(
        &manager, "root", (scene_route_args_view_t){0}, &operation, NULL);
    scene_manager_step(&manager, 1, 0.016F);
    (void)scene_manager_show(
        &manager, "modal",
        (scene_route_args_view_t){.data = &args, .size = sizeof args},
        &operation, NULL);
    scene_manager_step(&manager, 2, 0.016F);
    TEST_ASSERT_EQUAL_UINT32(17, modal.shown_value);

    (void)scene_manager_reload(&manager, &operation, NULL);
    scene_manager_step(&manager, 3, 0.016F);
    TEST_ASSERT_FALSE(scene_manager_is_presented(&manager, &catalog[1]));
    TEST_ASSERT_EQUAL(SCENE_RESIDENCY_UNLOADED,
                      scene_manager_scene_residency(&manager, &catalog[1]));
    const int modal_updates_before = modal.updates;
    const int modal_ui_before = modal.ui_calls;
    scene_manager_update(&manager, 0.016F);
    scene_manager_build_ui(&manager, NULL);
    TEST_ASSERT_EQUAL_INT(modal_updates_before, modal.updates);
    TEST_ASSERT_EQUAL_INT(modal_ui_before, modal.ui_calls);

    scene_manager_step(&manager, 4, 0.016F);
    TEST_ASSERT_TRUE(scene_manager_is_presented(&manager, &catalog[1]));
    TEST_ASSERT_EQUAL_UINT32(17, modal.shown_value);
    TEST_ASSERT_EQUAL_INT(2, modal.shows);
    TEST_ASSERT_EQUAL_INT(1, modal.hides);
    TEST_ASSERT_EQUAL_INT(1, modal.unloads);
    const scene_history_entry_view_t top = scene_manager_top(&manager);
    presentation_args_t copied;
    memcpy(&copied, top.args.data, sizeof copied);
    TEST_ASSERT_EQUAL_UINT32(17, copied.value);
    scene_manager_shutdown(&manager);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_modal_policy_input_and_passive_ui);
    RUN_TEST(test_reactivated_screen_pairs_hide_show_without_resume);
    RUN_TEST(test_reactivated_modal_runs_exit_and_enter_transitions);
    RUN_TEST(test_any_pause_modal_wins_and_continue_resumes_after_pop);
    RUN_TEST(test_modal_reload_preserves_args_and_suppresses_modal_dispatch);
    return UNITY_END();
}
