#include <setjmp.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static jmp_buf s_assert_jump;
static const char *s_expected_assert_fragment;
static const char *s_caught_assert_expression;

static void reentrancy_test_assert(bool condition, const char *expression) {
    if (condition) {
        return;
    }
    if (s_expected_assert_fragment != NULL) {
        s_caught_assert_expression = expression;
        longjmp(s_assert_jump, 1);
    }
    abort();
}

#define SCENE_MANAGER_ASSERT(condition) \
    reentrancy_test_assert((condition), #condition)
#include "../src/scene_manager.c"

#include "unity.h"

typedef enum callback_action {
    CALLBACK_STEP,
    CALLBACK_SHUTDOWN,
    CALLBACK_SHOW_MODAL,
    CALLBACK_LIFECYCLE_UPDATE,
    CALLBACK_LIFECYCLE_UI,
    CALLBACK_LIFECYCLE_NAVIGATION,
    CALLBACK_LIFECYCLE_PRELOAD,
    CALLBACK_TRANSITION_NAVIGATION
} callback_action_t;

typedef struct callback_scene {
    scene_manager_t *manager;
    callback_action_t action;
    scene_result_t navigation_result;
} callback_scene_t;

static scene_load_result_t ready(void *instance) {
    (void)instance;
    return SCENE_LOAD_READY;
}

static void callback_update(void *instance, float dt) {
    callback_scene_t *scene = instance;
    if (scene->action == CALLBACK_STEP) {
        scene_manager_step(scene->manager, 100, dt);
    } else if (scene->action == CALLBACK_SHOW_MODAL) {
        scene->navigation_result = scene_manager_show(
            scene->manager, "modal", (scene_route_args_view_t){0},
            NULL, NULL);
    }
}

static void callback_ui(void *instance, void *context,
                        scene_ui_mode_t mode) {
    callback_scene_t *scene = instance;
    (void)context;
    (void)mode;
    if (scene->action == CALLBACK_SHUTDOWN) {
        scene_manager_shutdown(scene->manager);
    }
}

static void callback_show(void *instance, scene_route_args_view_t args) {
    callback_scene_t *scene = instance;
    (void)args;
    if (scene->action == CALLBACK_LIFECYCLE_UPDATE) {
        scene_manager_update(scene->manager, 0.016F);
    } else if (scene->action == CALLBACK_LIFECYCLE_UI) {
        scene_manager_build_ui(scene->manager, NULL);
    } else if (scene->action == CALLBACK_LIFECYCLE_NAVIGATION) {
        (void)scene_manager_show(
            scene->manager, "modal", (scene_route_args_view_t){0},
            NULL, NULL);
    } else if (scene->action == CALLBACK_LIFECYCLE_PRELOAD) {
        (void)scene_manager_preload(scene->manager, "root");
    }
}

static void callback_transition_begin(
    void *instance, scene_transition_direction_t direction) {
    callback_scene_t *scene = instance;
    (void)direction;
    if (scene->action == CALLBACK_TRANSITION_NAVIGATION) {
        (void)scene_manager_show(
            scene->manager, "modal", (scene_route_args_view_t){0},
            NULL, NULL);
    }
}

static scene_transition_result_t callback_transition_step(
    void *instance, float dt) {
    (void)instance;
    (void)dt;
    return SCENE_TRANSITION_DONE;
}

static const scene_api_t k_root_api = {
    .load_step = ready,
    .on_update = callback_update,
    .on_ui = callback_ui,
};

static const scene_api_t k_modal_api = {
    .load_step = ready,
    .on_show = callback_show,
};

static const scene_transition_api_t k_modal_transitions = {
    .begin = callback_transition_begin,
    .step = callback_transition_step,
};

static void start_manager(scene_manager_t *manager,
                          callback_scene_t *root) {
    const scene_descriptor_t catalog[] = {
        {.id = "root", .kind = SCENE_KIND_SCREEN, .keep_loaded = true,
         .instance = root, .api = &k_root_api},
        {.id = "modal", .kind = SCENE_KIND_MODAL,
         .modal_update_policy = SCENE_MODAL_CONTINUE_BELOW,
         .instance = root,
         .api = &k_modal_api,
         .transitions = &k_modal_transitions},
    };
    scene_operation_id_t operation_id;

    /*
     * The manager keeps the catalog pointer, so copy it into static-duration
     * storage owned by this test helper's caller below.
     */
    static scene_descriptor_t stable_catalog[2];
    stable_catalog[0] = catalog[0];
    stable_catalog[1] = catalog[1];
    scene_manager_init(manager, stable_catalog, 2);
    root->manager = manager;
    (void)scene_manager_start(
        manager, "root", (scene_route_args_view_t){0},
        &operation_id, NULL);
    scene_manager_step(manager, 1, 0.016F);
}

static bool catches_expected_assert(void (*scenario)(void *),
                                    void *context,
                                    const char *expression_fragment) {
    s_expected_assert_fragment = expression_fragment;
    s_caught_assert_expression = NULL;
    if (setjmp(s_assert_jump) == 0) {
        scenario(context);
        s_expected_assert_fragment = NULL;
        return false;
    }
    s_expected_assert_fragment = NULL;
    return s_caught_assert_expression != NULL &&
           strstr(s_caught_assert_expression,
                  expression_fragment) != NULL;
}

static void run_update(void *context) {
    scene_manager_update(context, 0.016F);
}

static void run_ui(void *context) {
    scene_manager_build_ui(context, NULL);
}

typedef struct invalid_args_context {
    scene_manager_t *manager;
    scene_route_args_view_t args;
} invalid_args_context_t;

static void run_invalid_start(void *context) {
    const invalid_args_context_t *invalid = context;
    (void)scene_manager_start(
        invalid->manager, "typed", invalid->args, NULL, NULL);
}

static void run_step(void *context) {
    scene_manager_step(context, 1000, 0.016F);
}

static void run_show_modal(void *context) {
    (void)scene_manager_show(
        context, "modal", (scene_route_args_view_t){0}, NULL, NULL);
}

static void run_can_back_to_after_shutdown(void *context) {
    (void)scene_manager_can_back_to(context, "root");
}

void setUp(void) {}
void tearDown(void) {}

static void test_update_callback_cannot_drive_manager_step(void) {
    scene_manager_t manager;
    callback_scene_t root = {.action = CALLBACK_STEP};
    start_manager(&manager, &root);

    TEST_ASSERT_TRUE(catches_expected_assert(
        run_update, &manager, "consumer_dispatch_depth == 0"));
}

static void test_ui_callback_cannot_shutdown_manager(void) {
    scene_manager_t manager;
    callback_scene_t root = {.action = CALLBACK_SHUTDOWN};
    start_manager(&manager, &root);

    TEST_ASSERT_TRUE(catches_expected_assert(
        run_ui, &manager, "consumer_dispatch_depth == 0"));
}

static void test_update_callback_can_enqueue_one_navigation_command(void) {
    scene_manager_t manager;
    callback_scene_t root = {.action = CALLBACK_SHOW_MODAL};
    start_manager(&manager, &root);

    scene_manager_update(&manager, 0.016F);
    TEST_ASSERT_EQUAL(SCENE_RESULT_ACCEPTED, root.navigation_result);
    TEST_ASSERT_TRUE(scene_manager_input_gated(&manager));
    scene_manager_step(&manager, 2, 0.016F);
    TEST_ASSERT_EQUAL_STRING(
        "modal", scene_manager_top(&manager).scene->id);
    scene_manager_shutdown(&manager);
}

static void test_lifecycle_callback_cannot_dispatch_update(void) {
    scene_manager_t manager;
    callback_scene_t root = {.action = CALLBACK_LIFECYCLE_UPDATE};
    start_manager(&manager, &root);
    TEST_ASSERT_EQUAL(
        SCENE_RESULT_ACCEPTED,
        scene_manager_show(
            &manager, "modal", (scene_route_args_view_t){0}, NULL, NULL));

    TEST_ASSERT_TRUE(catches_expected_assert(
        run_step, &manager, "m->dispatch_depth == 0"));
}

static void test_lifecycle_callback_cannot_dispatch_ui(void) {
    scene_manager_t manager;
    callback_scene_t root = {.action = CALLBACK_LIFECYCLE_UI};
    start_manager(&manager, &root);
    TEST_ASSERT_EQUAL(
        SCENE_RESULT_ACCEPTED,
        scene_manager_show(
            &manager, "modal", (scene_route_args_view_t){0}, NULL, NULL));

    TEST_ASSERT_TRUE(catches_expected_assert(
        run_step, &manager, "m->dispatch_depth == 0"));
}

static void test_lifecycle_callback_cannot_navigate_or_preload(void) {
    const callback_action_t actions[] = {
        CALLBACK_LIFECYCLE_NAVIGATION,
        CALLBACK_LIFECYCLE_PRELOAD,
    };
    for (size_t index = 0; index < sizeof actions / sizeof actions[0];
         ++index) {
        scene_manager_t manager;
        callback_scene_t root = {.action = actions[index]};
        start_manager(&manager, &root);
        TEST_ASSERT_EQUAL(
            SCENE_RESULT_ACCEPTED,
            scene_manager_show(
                &manager, "modal", (scene_route_args_view_t){0},
                NULL, NULL));
        TEST_ASSERT_TRUE(catches_expected_assert(
            run_step, &manager, "m->dispatch_depth == 0"));
    }
}

static void test_transition_callback_cannot_navigate(void) {
    scene_manager_t manager;
    callback_scene_t root = {.action = CALLBACK_TRANSITION_NAVIGATION};
    start_manager(&manager, &root);
    TEST_ASSERT_EQUAL(
        SCENE_RESULT_ACCEPTED,
        scene_manager_show(
            &manager, "modal", (scene_route_args_view_t){0}, NULL, NULL));

    TEST_ASSERT_TRUE(catches_expected_assert(
        run_step, &manager, "m->dispatch_depth == 0"));
}

static void test_route_argument_null_and_size_rules_assert_exactly(void) {
    uint8_t typed_instance = 0;
    uint8_t byte = 7;
    const scene_api_t api = {.load_step = ready};
    const scene_descriptor_t catalog[] = {
        {.id = "typed", .kind = SCENE_KIND_SCREEN,
         .route_args_size = 1, .instance = &typed_instance, .api = &api},
    };
    scene_manager_t manager;
    invalid_args_context_t invalid;

    scene_manager_init(&manager, catalog, 1);
    invalid = (invalid_args_context_t){
        .manager = &manager,
        .args = {.data = NULL, .size = 1},
    };
    TEST_ASSERT_TRUE(catches_expected_assert(
        run_invalid_start, &invalid, "args.data == NULL"));

    invalid.args = (scene_route_args_view_t){
        .data = &byte, .size = 0,
    };
    TEST_ASSERT_TRUE(catches_expected_assert(
        run_invalid_start, &invalid, "args.size == scene->route_args_size"));
}

static void test_history_capacity_accepts_128_entries_then_asserts(void) {
    uint8_t typed_instance = 0;
    const scene_api_t api = {.load_step = ready};
    const scene_descriptor_t catalog[] = {
        {.id = "typed", .kind = SCENE_KIND_SCREEN,
         .route_args_size = 1, .instance = &typed_instance, .api = &api},
    };
    scene_manager_t manager;
    uint8_t value = 0;

    scene_manager_init(&manager, catalog, 1);
    TEST_ASSERT_EQUAL(
        SCENE_RESULT_ACCEPTED,
        scene_manager_start(
            &manager, "typed",
            (scene_route_args_view_t){.data = &value, .size = 1},
            NULL, NULL));
    scene_manager_step(&manager, 1, 0.016F);
    for (size_t index = 1;
         index < SCENE_MANAGER_MAX_HISTORY;
         ++index) {
        value = (uint8_t)index;
        TEST_ASSERT_EQUAL(
            SCENE_RESULT_ACCEPTED,
            scene_manager_show(
                &manager, "typed",
                (scene_route_args_view_t){.data = &value, .size = 1},
                NULL, NULL));
        scene_manager_step(&manager, (uint64_t)index + 1, 0.016F);
    }
    TEST_ASSERT_EQUAL_UINT32(
        SCENE_MANAGER_MAX_HISTORY,
        scene_manager_history_count(&manager));

    TEST_ASSERT_EQUAL(
        SCENE_RESULT_ACCEPTED,
        scene_manager_show(
            &manager, "typed",
            (scene_route_args_view_t){.data = &value, .size = 1},
            NULL, NULL));
    TEST_ASSERT_TRUE(catches_expected_assert(
        run_step, &manager,
        "command->candidate_count < SCENE_MANAGER_MAX_HISTORY"));
}

static void test_operation_id_exhaustion_asserts_before_wrap(void) {
    scene_manager_t manager;
    callback_scene_t root = {0};
    start_manager(&manager, &root);
    impl(&manager)->next_operation_id = SCENE_OPERATION_ID_MAX;

    TEST_ASSERT_TRUE(catches_expected_assert(
        run_show_modal, &manager,
        "m->next_operation_id < SCENE_OPERATION_ID_MAX"));
}

static void test_last_safe_operation_id_is_accepted(void) {
    scene_manager_t manager;
    callback_scene_t root = {0};
    scene_operation_id_t operation_id = 0;
    start_manager(&manager, &root);
    impl(&manager)->next_operation_id = SCENE_OPERATION_ID_MAX - 1;

    TEST_ASSERT_EQUAL(
        SCENE_RESULT_ACCEPTED,
        scene_manager_show(
            &manager, "modal", (scene_route_args_view_t){0},
            &operation_id, NULL));
    TEST_ASSERT_EQUAL_UINT64(SCENE_OPERATION_ID_MAX, operation_id);
}

static void test_can_back_to_checks_lifecycle_before_catalog_access(void) {
    scene_manager_t manager;
    callback_scene_t root = {0};
    start_manager(&manager, &root);
    scene_manager_shutdown(&manager);
    impl(&manager)->catalog =
        (const scene_descriptor_t *)(uintptr_t)1;
    impl(&manager)->scene_count = 1;

    TEST_ASSERT_TRUE(catches_expected_assert(
        run_can_back_to_after_shutdown, &manager,
        "m != NULL && m->initialized && !m->shutting_down"));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_update_callback_cannot_drive_manager_step);
    RUN_TEST(test_ui_callback_cannot_shutdown_manager);
    RUN_TEST(test_update_callback_can_enqueue_one_navigation_command);
    RUN_TEST(test_lifecycle_callback_cannot_dispatch_update);
    RUN_TEST(test_lifecycle_callback_cannot_dispatch_ui);
    RUN_TEST(test_lifecycle_callback_cannot_navigate_or_preload);
    RUN_TEST(test_transition_callback_cannot_navigate);
    RUN_TEST(test_route_argument_null_and_size_rules_assert_exactly);
    RUN_TEST(test_history_capacity_accepts_128_entries_then_asserts);
    RUN_TEST(test_last_safe_operation_id_is_accepted);
    RUN_TEST(test_operation_id_exhaustion_asserts_before_wrap);
    RUN_TEST(test_can_back_to_checks_lifecycle_before_catalog_access);
    return UNITY_END();
}
