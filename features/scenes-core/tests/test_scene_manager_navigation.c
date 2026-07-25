#include "features/scenes/scene_manager.h"

#include "unity.h"

#include <string.h>

typedef struct nav_scene {
    int loads;
    int unloads;
    int shows;
    int hides;
    unsigned shown_value;
    unsigned shown_values[8];
    bool ready;
} nav_scene_t;

typedef struct item_args {
    unsigned value;
} item_args_t;

typedef struct max_args_scene {
    uint8_t shown[SCENE_ROUTE_ARGS_INLINE_CAPACITY];
} max_args_scene_t;

static void nav_load_begin(void *instance) {
    nav_scene_t *scene = instance;
    ++scene->loads;
}

static scene_load_result_t nav_load_step(void *instance) {
    nav_scene_t *scene = instance;
    return scene->ready ? SCENE_LOAD_READY : SCENE_LOAD_PENDING;
}

static void nav_unload(void *instance) {
    nav_scene_t *scene = instance;
    ++scene->unloads;
}

static void nav_show(void *instance, scene_route_args_view_t args) {
    nav_scene_t *scene = instance;
    ++scene->shows;
    if (args.size > 0) {
        item_args_t copied;
        memcpy(&copied, args.data, sizeof copied);
        scene->shown_value = copied.value;
        scene->shown_values[scene->shows - 1] = copied.value;
    }
}

static void nav_hide(void *instance) {
    nav_scene_t *scene = instance;
    ++scene->hides;
}

static const scene_api_t k_nav_api = {
    .load_begin = nav_load_begin,
    .load_step = nav_load_step,
    .unload = nav_unload,
    .on_show = nav_show,
    .on_hide = nav_hide,
};

static void max_args_show(void *instance, scene_route_args_view_t args) {
    max_args_scene_t *scene = instance;
    memcpy(scene->shown, args.data, args.size);
}

static scene_load_result_t max_args_ready(void *instance) {
    (void)instance;
    return SCENE_LOAD_READY;
}

static const scene_api_t k_max_args_api = {
    .load_step = max_args_ready,
    .on_show = max_args_show,
};

void setUp(void) {}
void tearDown(void) {}

static void settle(scene_manager_t *manager, uint64_t *frame) {
    scene_manager_step(manager, ++*frame, 0.016F);
    TEST_ASSERT_FALSE(scene_manager_input_gated(manager));
}

static void test_start_show_back_and_route_argument_copy(void) {
    nav_scene_t root = {.ready = true};
    nav_scene_t item = {.ready = true};
    const scene_descriptor_t catalog[] = {
        {.id = "root", .kind = SCENE_KIND_SCREEN, .keep_loaded = true,
         .instance = &root, .api = &k_nav_api},
        {.id = "item", .kind = SCENE_KIND_MODAL,
         .route_args_size = sizeof(item_args_t), .instance = &item,
         .api = &k_nav_api},
    };
    scene_manager_t manager;
    scene_operation_id_t operation;
    scene_operation_status_t operation_status;
    uint64_t frame = 0;
    item_args_t args = {.value = 42};
    char item_id[] = "item";

    scene_manager_init(&manager, catalog, 2);
    TEST_ASSERT_EQUAL(
        SCENE_RESULT_ACCEPTED,
        scene_manager_start(&manager, "root",
                            (scene_route_args_view_t){0}, &operation, NULL));
    TEST_ASSERT_TRUE(scene_manager_input_gated(&manager));
    TEST_ASSERT_EQUAL_UINT32(0, scene_manager_history_count(&manager));
    settle(&manager, &frame);
    TEST_ASSERT_EQUAL_UINT32(1, scene_manager_history_count(&manager));
    TEST_ASSERT_EQUAL_STRING("root",
                             scene_manager_top(&manager).scene->id);

    TEST_ASSERT_EQUAL(
        SCENE_RESULT_ACCEPTED,
        scene_manager_show(
            &manager, item_id,
            (scene_route_args_view_t){.data = &args, .size = sizeof args},
            &operation, NULL));
    item_id[0] = 'x';
    TEST_ASSERT_EQUAL(
        SCENE_OPERATION_PENDING,
        scene_manager_operation_status(
            &manager, operation, &operation_status));
    TEST_ASSERT_EQUAL_STRING("item", operation_status.target_scene_id);
    args.value = 99;
    settle(&manager, &frame);
    TEST_ASSERT_EQUAL_UINT32(2, scene_manager_history_count(&manager));
    TEST_ASSERT_EQUAL_UINT32(42, item.shown_value);
    TEST_ASSERT_FALSE(scene_manager_can_process_input(&manager, &catalog[0]));
    TEST_ASSERT_TRUE(scene_manager_can_process_input(&manager, &catalog[1]));

    TEST_ASSERT_EQUAL(
        SCENE_RESULT_ACCEPTED,
        scene_manager_back(&manager, 1, &operation, NULL));
    settle(&manager, &frame);
    TEST_ASSERT_EQUAL_STRING("root",
                             scene_manager_top(&manager).scene->id);
    TEST_ASSERT_EQUAL_INT(1, item.hides);
    TEST_ASSERT_EQUAL_INT(1, item.unloads);
    TEST_ASSERT_EQUAL(SCENE_RESULT_ROOT_PROTECTED,
                      scene_manager_back(&manager, 1, NULL, NULL));

    scene_manager_shutdown(&manager);
}

static void test_busy_preload_attachment_and_reload_full_lifecycle(void) {
    nav_scene_t root = {.ready = false};
    const scene_descriptor_t catalog[] = {
        {.id = "root", .kind = SCENE_KIND_SCREEN, .instance = &root,
         .api = &k_nav_api},
    };
    scene_manager_t manager;
    scene_operation_id_t start_id = 0;
    scene_operation_id_t blocker = 0;
    scene_operation_id_t reload_id = 0;
    scene_operation_status_t status;

    scene_manager_init(&manager, catalog, 1);
    TEST_ASSERT_EQUAL(SCENE_PRELOAD_SCHEDULED,
                      scene_manager_preload(&manager, "root"));
    TEST_ASSERT_EQUAL(
        SCENE_RESULT_ACCEPTED,
        scene_manager_start(&manager, "root",
                            (scene_route_args_view_t){0}, &start_id, NULL));
    TEST_ASSERT_EQUAL(
        SCENE_RESULT_BUSY,
        scene_manager_reload(&manager, &reload_id, &blocker));
    TEST_ASSERT_EQUAL_UINT64(start_id, blocker);

    scene_manager_step(&manager, 1, 0.016F);
    TEST_ASSERT_EQUAL_INT(1, root.loads);
    TEST_ASSERT_EQUAL(SCENE_OPERATION_ACTIVE,
                      scene_manager_operation_status(
                          &manager, start_id, &status));
    root.ready = true;
    scene_manager_step(&manager, 2, 0.016F);
    TEST_ASSERT_EQUAL(SCENE_OPERATION_COMPLETED,
                      scene_manager_operation_status(
                          &manager, start_id, &status));

    TEST_ASSERT_EQUAL(SCENE_RESULT_ACCEPTED,
                      scene_manager_reload(&manager, &reload_id, NULL));
    scene_manager_step(&manager, 3, 0.016F);
    TEST_ASSERT_TRUE(scene_manager_input_gated(&manager));
    TEST_ASSERT_EQUAL_INT(1, root.hides);
    TEST_ASSERT_EQUAL_INT(1, root.unloads);
    TEST_ASSERT_FALSE(scene_manager_is_presented(&manager, &catalog[0]));

    scene_manager_step(&manager, 4, 0.016F);
    TEST_ASSERT_FALSE(scene_manager_input_gated(&manager));
    TEST_ASSERT_EQUAL_INT(2, root.loads);
    TEST_ASSERT_EQUAL_INT(2, root.shows);

    scene_manager_shutdown(&manager);
}

static void test_replace_screen_with_screen_updates_history_and_lifecycle(void) {
    nav_scene_t root = {.ready = true};
    nav_scene_t next = {.ready = true};
    const scene_descriptor_t catalog[] = {
        {.id = "root", .kind = SCENE_KIND_SCREEN, .instance = &root,
         .api = &k_nav_api},
        {.id = "next", .kind = SCENE_KIND_SCREEN, .instance = &next,
         .api = &k_nav_api},
    };
    scene_manager_t manager;
    scene_operation_id_t operation;
    scene_operation_status_t status;
    uint64_t frame = 0;

    scene_manager_init(&manager, catalog, 2);
    TEST_ASSERT_EQUAL(
        SCENE_RESULT_ACCEPTED,
        scene_manager_start(&manager, "root",
                            (scene_route_args_view_t){0}, NULL, NULL));
    settle(&manager, &frame);

    TEST_ASSERT_EQUAL(
        SCENE_RESULT_ACCEPTED,
        scene_manager_replace(&manager, "next",
                              (scene_route_args_view_t){0},
                              &operation, NULL));
    settle(&manager, &frame);

    TEST_ASSERT_EQUAL_UINT32(1, scene_manager_history_count(&manager));
    TEST_ASSERT_EQUAL_STRING("next", scene_manager_top(&manager).scene->id);
    TEST_ASSERT_EQUAL_INT(1, root.hides);
    TEST_ASSERT_EQUAL_INT(1, root.unloads);
    TEST_ASSERT_EQUAL_INT(1, next.loads);
    TEST_ASSERT_EQUAL_INT(1, next.shows);
    TEST_ASSERT_EQUAL(
        SCENE_OPERATION_COMPLETED,
        scene_manager_operation_status(&manager, operation, &status));
    TEST_ASSERT_EQUAL(SCENE_OPERATION_REPLACE, status.kind);
    TEST_ASSERT_EQUAL_STRING("next", status.target_scene_id);

    scene_manager_shutdown(&manager);
}

static void test_replace_same_scene_recreates_with_copied_args(void) {
    nav_scene_t item = {.ready = true};
    const scene_descriptor_t catalog[] = {
        {.id = "item", .kind = SCENE_KIND_SCREEN,
         .route_args_size = sizeof(item_args_t), .instance = &item,
         .api = &k_nav_api},
    };
    scene_manager_t manager;
    uint64_t frame = 0;
    item_args_t first = {.value = 7};
    item_args_t replacement = {.value = 42};

    scene_manager_init(&manager, catalog, 1);
    TEST_ASSERT_EQUAL(
        SCENE_RESULT_ACCEPTED,
        scene_manager_start(
            &manager, "item",
            (scene_route_args_view_t){.data = &first, .size = sizeof first},
            NULL, NULL));
    settle(&manager, &frame);

    TEST_ASSERT_EQUAL(
        SCENE_RESULT_ACCEPTED,
        scene_manager_replace(
            &manager, "item",
            (scene_route_args_view_t){
                .data = &replacement,
                .size = sizeof replacement,
            },
            NULL, NULL));
    replacement.value = 99;
    scene_manager_step(&manager, ++frame, 0.016F);
    TEST_ASSERT_TRUE(scene_manager_input_gated(&manager));
    TEST_ASSERT_FALSE(scene_manager_is_presented(&manager, &catalog[0]));
    TEST_ASSERT_EQUAL_INT(1, item.hides);
    TEST_ASSERT_EQUAL_INT(1, item.unloads);

    settle(&manager, &frame);
    TEST_ASSERT_EQUAL_UINT32(1, scene_manager_history_count(&manager));
    TEST_ASSERT_EQUAL_STRING("item", scene_manager_top(&manager).scene->id);
    TEST_ASSERT_EQUAL_INT(2, item.loads);
    TEST_ASSERT_EQUAL_INT(2, item.shows);
    TEST_ASSERT_EQUAL_UINT32(7, item.shown_values[0]);
    TEST_ASSERT_EQUAL_UINT32(42, item.shown_values[1]);

    scene_manager_shutdown(&manager);
}

static void test_back_reactivates_the_requested_occurrence_and_its_args(void) {
    nav_scene_t item = {.ready = true};
    nav_scene_t middle = {.ready = true};
    const scene_descriptor_t catalog[] = {
        {.id = "item", .kind = SCENE_KIND_SCREEN, .keep_loaded = true,
         .route_args_size = sizeof(item_args_t), .instance = &item,
         .api = &k_nav_api},
        {.id = "middle", .kind = SCENE_KIND_SCREEN, .keep_loaded = true,
         .instance = &middle, .api = &k_nav_api},
    };
    scene_manager_t manager;
    scene_operation_id_t operation;
    uint64_t frame = 0;
    item_args_t first = {.value = 1};
    item_args_t second = {.value = 2};

    scene_manager_init(&manager, catalog, 2);
    TEST_ASSERT_EQUAL(
        SCENE_RESULT_ACCEPTED,
        scene_manager_start(
            &manager, "item",
            (scene_route_args_view_t){.data = &first, .size = sizeof first},
            &operation, NULL));
    settle(&manager, &frame);
    TEST_ASSERT_EQUAL(
        SCENE_RESULT_ACCEPTED,
        scene_manager_show(&manager, "middle",
                           (scene_route_args_view_t){0}, &operation, NULL));
    settle(&manager, &frame);
    TEST_ASSERT_EQUAL(
        SCENE_RESULT_ACCEPTED,
        scene_manager_show(
            &manager, "item",
            (scene_route_args_view_t){.data = &second, .size = sizeof second},
            &operation, NULL));
    settle(&manager, &frame);

    TEST_ASSERT_EQUAL_UINT32(3, scene_manager_history_count(&manager));
    TEST_ASSERT_EQUAL(
        SCENE_RESULT_ACCEPTED,
        scene_manager_back(&manager, 2, &operation, NULL));
    settle(&manager, &frame);

    TEST_ASSERT_EQUAL_UINT32(1, scene_manager_history_count(&manager));
    TEST_ASSERT_EQUAL_STRING("item", scene_manager_top(&manager).scene->id);
    TEST_ASSERT_EQUAL_INT(3, item.shows);
    TEST_ASSERT_EQUAL_INT(2, item.hides);
    TEST_ASSERT_EQUAL_UINT32(1, item.shown_values[0]);
    TEST_ASSERT_EQUAL_UINT32(2, item.shown_values[1]);
    TEST_ASSERT_EQUAL_UINT32(1, item.shown_values[2]);

    scene_manager_shutdown(&manager);
}

static void test_modal_stack_close_modals_and_back_to_named_screen(void) {
    nav_scene_t root = {.ready = true};
    nav_scene_t screen = {.ready = true};
    nav_scene_t first_modal = {.ready = true};
    nav_scene_t second_modal = {.ready = true};
    const scene_descriptor_t catalog[] = {
        {.id = "root", .kind = SCENE_KIND_SCREEN, .keep_loaded = true,
         .instance = &root, .api = &k_nav_api},
        {.id = "screen", .kind = SCENE_KIND_SCREEN, .keep_loaded = true,
         .instance = &screen, .api = &k_nav_api},
        {.id = "first_modal", .kind = SCENE_KIND_MODAL,
         .instance = &first_modal, .api = &k_nav_api},
        {.id = "second_modal", .kind = SCENE_KIND_MODAL,
         .instance = &second_modal, .api = &k_nav_api},
    };
    scene_manager_t manager;
    scene_operation_id_t operation;
    uint64_t frame = 0;

    scene_manager_init(&manager, catalog, 4);
    TEST_ASSERT_EQUAL(
        SCENE_RESULT_ACCEPTED,
        scene_manager_start(&manager, "root",
                            (scene_route_args_view_t){0}, &operation, NULL));
    settle(&manager, &frame);
    TEST_ASSERT_EQUAL(
        SCENE_RESULT_ACCEPTED,
        scene_manager_show(&manager, "screen",
                           (scene_route_args_view_t){0}, &operation, NULL));
    settle(&manager, &frame);
    TEST_ASSERT_EQUAL(
        SCENE_RESULT_ACCEPTED,
        scene_manager_show(&manager, "first_modal",
                           (scene_route_args_view_t){0}, &operation, NULL));
    settle(&manager, &frame);
    TEST_ASSERT_EQUAL(
        SCENE_RESULT_ACCEPTED,
        scene_manager_show(&manager, "second_modal",
                           (scene_route_args_view_t){0}, &operation, NULL));
    settle(&manager, &frame);
    TEST_ASSERT_EQUAL_UINT32(4, scene_manager_history_count(&manager));

    TEST_ASSERT_EQUAL(
        SCENE_RESULT_ACCEPTED,
        scene_manager_close_modals(&manager, &operation, NULL));
    settle(&manager, &frame);
    TEST_ASSERT_EQUAL_UINT32(2, scene_manager_history_count(&manager));
    TEST_ASSERT_EQUAL_STRING("screen", scene_manager_top(&manager).scene->id);
    TEST_ASSERT_TRUE(scene_manager_can_back_to(&manager, "root"));

    TEST_ASSERT_EQUAL(
        SCENE_RESULT_ACCEPTED,
        scene_manager_back_to(&manager, "root", &operation, NULL));
    settle(&manager, &frame);
    TEST_ASSERT_EQUAL_UINT32(1, scene_manager_history_count(&manager));
    TEST_ASSERT_EQUAL_STRING("root", scene_manager_top(&manager).scene->id);

    scene_manager_shutdown(&manager);
}

static void test_maximum_route_args_are_copied_before_dispatch(void) {
    max_args_scene_t instance = {0};
    uint8_t args[SCENE_ROUTE_ARGS_INLINE_CAPACITY];
    const scene_descriptor_t catalog[] = {
        {.id = "max_args", .kind = SCENE_KIND_SCREEN,
         .route_args_size = SCENE_ROUTE_ARGS_INLINE_CAPACITY,
         .instance = &instance, .api = &k_max_args_api},
    };
    scene_manager_t manager;
    uint64_t frame = 0;

    for (size_t index = 0;
         index < SCENE_ROUTE_ARGS_INLINE_CAPACITY;
         ++index) {
        args[index] = (uint8_t)(index + 1);
    }
    scene_manager_init(&manager, catalog, 1);
    TEST_ASSERT_EQUAL(
        SCENE_RESULT_ACCEPTED,
        scene_manager_start(
            &manager, "max_args",
            (scene_route_args_view_t){
                .data = args,
                .size = SCENE_ROUTE_ARGS_INLINE_CAPACITY,
            },
            NULL, NULL));
    memset(args, 0, sizeof args);
    settle(&manager, &frame);

    TEST_ASSERT_EQUAL_UINT8(
        1, instance.shown[0]);
    TEST_ASSERT_EQUAL_UINT8(
        SCENE_ROUTE_ARGS_INLINE_CAPACITY,
        instance.shown[
            SCENE_ROUTE_ARGS_INLINE_CAPACITY - 1]);
    scene_manager_shutdown(&manager);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_start_show_back_and_route_argument_copy);
    RUN_TEST(test_busy_preload_attachment_and_reload_full_lifecycle);
    RUN_TEST(test_replace_screen_with_screen_updates_history_and_lifecycle);
    RUN_TEST(test_replace_same_scene_recreates_with_copied_args);
    RUN_TEST(test_back_reactivates_the_requested_occurrence_and_its_args);
    RUN_TEST(test_modal_stack_close_modals_and_back_to_named_screen);
    RUN_TEST(test_maximum_route_args_are_copied_before_dispatch);
    return UNITY_END();
}
