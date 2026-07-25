#include "features/scenes/scene_manager.h"
#include "features/scenes/scene_manager_devapi.h"

#include "cJSON.h"
#include "core/nt_core.h"
#include "devapi/nt_devapi.h"
#include "unity.h"

#include <stdio.h>
#include <string.h>

typedef struct devapi_scene {
    bool ready;
} devapi_scene_t;

static scene_manager_t s_manager;
static devapi_scene_t s_root;
static devapi_scene_t s_next;
static devapi_scene_t s_modal;
static devapi_scene_t s_typed;

static scene_load_result_t load_step(void *instance) {
    const devapi_scene_t *scene = instance;
    return scene->ready ? SCENE_LOAD_READY : SCENE_LOAD_PENDING;
}

static const scene_api_t k_api = {
    .load_step = load_step,
};

static bool dummy_endpoint(
    const cJSON *params, cJSON *result,
    nt_devapi_error *error, void *user) {
    (void)params;
    (void)result;
    (void)error;
    (void)user;
    return true;
}

static const scene_descriptor_t k_catalog[] = {
    {.id = "root", .kind = SCENE_KIND_SCREEN,
     .instance = &s_root, .api = &k_api},
    {.id = "next", .kind = SCENE_KIND_SCREEN,
     .instance = &s_next, .api = &k_api},
    {.id = "settings", .kind = SCENE_KIND_MODAL,
     .modal_update_policy = SCENE_MODAL_PAUSE_BELOW,
     .instance = &s_modal, .api = &k_api},
    {.id = "item", .kind = SCENE_KIND_MODAL,
     .modal_update_policy = SCENE_MODAL_PAUSE_BELOW,
     .route_args_size = sizeof(uint32_t),
     .instance = &s_typed, .api = &k_api},
};

static cJSON *submit(const char *request) {
    const char *response = nt_devapi_submit(request);
    TEST_ASSERT_NOT_NULL(response);
    cJSON *root = cJSON_Parse(response);
    TEST_ASSERT_NOT_NULL(root);
    return root;
}

static const char *error_code(const cJSON *root) {
    const cJSON *error = cJSON_GetObjectItemCaseSensitive(root, "error");
    const cJSON *code = cJSON_GetObjectItemCaseSensitive(error, "code");
    return cJSON_IsString(code) ? code->valuestring : NULL;
}

static const cJSON *response_result(const cJSON *root) {
    return cJSON_GetObjectItemCaseSensitive(root, "result");
}

static double response_operation_id(const cJSON *root) {
    const cJSON *operation_id = cJSON_GetObjectItemCaseSensitive(
        response_result(root), "operationId");
    TEST_ASSERT_TRUE(cJSON_IsNumber(operation_id));
    return operation_id->valuedouble;
}

static void assert_navigation_result(
    const cJSON *root, const char *expected_result,
    bool expects_operation_id, bool expects_blocking_id) {
    const cJSON *result = response_result(root);
    const cJSON *operation_id =
        cJSON_GetObjectItemCaseSensitive(result, "operationId");
    const cJSON *blocking_id =
        cJSON_GetObjectItemCaseSensitive(result, "blockingOperationId");
    TEST_ASSERT_TRUE(cJSON_IsTrue(
        cJSON_GetObjectItemCaseSensitive(root, "ok")));
    TEST_ASSERT_EQUAL_STRING(
        expected_result,
        cJSON_GetObjectItemCaseSensitive(result, "result")->valuestring);
    TEST_ASSERT_EQUAL(expects_operation_id, cJSON_IsNumber(operation_id));
    TEST_ASSERT_EQUAL(expects_blocking_id, cJSON_IsNumber(blocking_id));
}

void setUp(void) {
    scene_operation_id_t operation_id;
    s_root.ready = true;
    s_next.ready = true;
    s_modal.ready = true;
    s_typed.ready = true;
    scene_manager_init(
        &s_manager, k_catalog, sizeof k_catalog / sizeof k_catalog[0]);
    TEST_ASSERT_EQUAL(
        SCENE_RESULT_ACCEPTED,
        scene_manager_start(
            &s_manager, "root", (scene_route_args_view_t){0},
            &operation_id, NULL));
    scene_manager_step(&s_manager, 1, 0.016F);
    TEST_ASSERT_EQUAL(NT_OK, nt_devapi_init());
    TEST_ASSERT_TRUE(scene_manager_register_devapi(&s_manager));
}

void tearDown(void) {
    nt_devapi_shutdown();
    scene_manager_shutdown(&s_manager);
}

static void test_duplicate_keys_reject_before_navigation(void) {
    cJSON *root = submit(
        "{\"method\":\"game.scene.show\",\"params\":{"
        "\"scene\":\"next\",\"scene\":\"settings\"}}");
    TEST_ASSERT_TRUE(cJSON_IsFalse(
        cJSON_GetObjectItemCaseSensitive(root, "ok")));
    TEST_ASSERT_EQUAL_STRING("bad_params", error_code(root));
    TEST_ASSERT_EQUAL_UINT32(1, scene_manager_history_count(&s_manager));
    cJSON_Delete(root);
}

static void test_replace_cross_kind_returns_error_without_mutation(void) {
    scene_operation_id_t operation_id;
    TEST_ASSERT_EQUAL(
        SCENE_RESULT_ACCEPTED,
        scene_manager_show(
            &s_manager, "settings", (scene_route_args_view_t){0},
            &operation_id, NULL));
    scene_manager_step(&s_manager, 2, 0.016F);

    cJSON *root = submit(
        "{\"method\":\"game.scene.replace\","
        "\"params\":{\"scene\":\"next\"}}");
    TEST_ASSERT_TRUE(cJSON_IsFalse(
        cJSON_GetObjectItemCaseSensitive(root, "ok")));
    TEST_ASSERT_EQUAL_STRING("bad_operation", error_code(root));
    TEST_ASSERT_EQUAL_STRING(
        "settings", scene_manager_top(&s_manager).scene->id);
    cJSON_Delete(root);
}

static void test_parameterized_scene_can_preload_generically(void) {
    cJSON *root = submit(
        "{\"method\":\"game.scene.preload\","
        "\"params\":{\"scene\":\"item\"}}");
    const cJSON *result =
        cJSON_GetObjectItemCaseSensitive(root, "result");
    TEST_ASSERT_TRUE(cJSON_IsTrue(
        cJSON_GetObjectItemCaseSensitive(root, "ok")));
    TEST_ASSERT_EQUAL_STRING(
        "scheduled",
        cJSON_GetObjectItemCaseSensitive(result, "result")->valuestring);
    cJSON_Delete(root);
}

static void test_scene_id_format_matches_the_published_schema(void) {
    cJSON *root = submit(
        "{\"method\":\"game.scene.show\","
        "\"params\":{\"scene\":\"Upper\"}}");
    TEST_ASSERT_TRUE(cJSON_IsFalse(
        cJSON_GetObjectItemCaseSensitive(root, "ok")));
    TEST_ASSERT_EQUAL_STRING("bad_params", error_code(root));
    cJSON_Delete(root);
}

static void assert_request_error(const char *request,
                                 const char *expected_code) {
    const size_t history_count =
        scene_manager_history_count(&s_manager);
    cJSON *root = submit(request);
    TEST_ASSERT_TRUE(cJSON_IsFalse(
        cJSON_GetObjectItemCaseSensitive(root, "ok")));
    TEST_ASSERT_EQUAL_STRING(expected_code, error_code(root));
    TEST_ASSERT_EQUAL_UINT32(
        history_count, scene_manager_history_count(&s_manager));
    cJSON_Delete(root);
}

static void reset_manager_with_empty_history(void) {
    scene_manager_shutdown(&s_manager);
    scene_manager_init(
        &s_manager, k_catalog, sizeof k_catalog / sizeof k_catalog[0]);
    TEST_ASSERT_EQUAL_UINT32(0, scene_manager_history_count(&s_manager));
}

static void test_show_before_start_returns_bad_operation(void) {
    cJSON *root;

    reset_manager_with_empty_history();
    root = submit(
        "{\"method\":\"game.scene.show\","
        "\"params\":{\"scene\":\"next\"}}");

    TEST_ASSERT_TRUE(cJSON_IsFalse(
        cJSON_GetObjectItemCaseSensitive(root, "ok")));
    TEST_ASSERT_EQUAL_STRING("bad_operation", error_code(root));
    TEST_ASSERT_EQUAL_UINT32(0, scene_manager_history_count(&s_manager));
    cJSON_Delete(root);
}

static void test_replace_before_start_returns_bad_operation(void) {
    cJSON *root;

    reset_manager_with_empty_history();
    root = submit(
        "{\"method\":\"game.scene.replace\","
        "\"params\":{\"scene\":\"next\"}}");

    TEST_ASSERT_TRUE(cJSON_IsFalse(
        cJSON_GetObjectItemCaseSensitive(root, "ok")));
    TEST_ASSERT_EQUAL_STRING("bad_operation", error_code(root));
    TEST_ASSERT_EQUAL_UINT32(0, scene_manager_history_count(&s_manager));
    cJSON_Delete(root);
}

static void test_reload_before_start_returns_bad_operation(void) {
    cJSON *root;

    reset_manager_with_empty_history();
    root = submit(
        "{\"method\":\"game.scene.reload\",\"params\":{}}");

    TEST_ASSERT_TRUE(cJSON_IsFalse(
        cJSON_GetObjectItemCaseSensitive(root, "ok")));
    TEST_ASSERT_EQUAL_STRING("bad_operation", error_code(root));
    TEST_ASSERT_EQUAL_UINT32(0, scene_manager_history_count(&s_manager));
    cJSON_Delete(root);
}

static void test_strict_param_type_range_and_unknown_key_matrix(void) {
    assert_request_error(
        "{\"method\":\"game.scene.show\","
        "\"params\":{\"scene\":\"next\",\"extra\":true}}",
        "bad_params");
    assert_request_error(
        "{\"method\":\"game.scene.show\",\"params\":{}}",
        "bad_params");
    assert_request_error(
        "{\"method\":\"game.scene.show\","
        "\"params\":{\"scene\":7}}",
        "bad_params");
    assert_request_error(
        "{\"method\":\"game.scene.show\","
        "\"params\":{\"scene\":\"missing\"}}",
        "scene_not_found");
    assert_request_error(
        "{\"method\":\"game.scene.show\","
        "\"params\":{\"scene\":\"item\"}}",
        "typed_endpoint_required");
    assert_request_error(
        "{\"method\":\"game.scene.reload\","
        "\"params\":{\"extra\":true}}",
        "bad_params");
    assert_request_error(
        "{\"method\":\"game.scene.back\","
        "\"params\":{\"count\":0}}",
        "bad_params");
    assert_request_error(
        "{\"method\":\"game.scene.back\","
        "\"params\":{\"count\":1.5}}",
        "bad_params");
    assert_request_error(
        "{\"method\":\"game.scene.back\","
        "\"params\":{\"count\":129}}",
        "bad_params");
    assert_request_error(
        "{\"method\":\"game.scene.operation_status\","
        "\"params\":{\"operationId\":0}}",
        "bad_params");
    assert_request_error(
        "{\"method\":\"game.scene.operation_status\","
        "\"params\":{\"operationId\":1.5}}",
        "bad_params");
    assert_request_error(
        "{\"method\":\"game.scene.operation_status\","
        "\"params\":{\"operationId\":9007199254740992}}",
        "bad_params");
}

static void test_operation_polling_and_busy_blocker_are_deterministic(void) {
    cJSON *root;
    const cJSON *result;
    double operation_id;
    char request[160];

    s_next.ready = false;
    root = submit(
        "{\"method\":\"game.scene.show\","
        "\"params\":{\"scene\":\"next\"}}");
    TEST_ASSERT_TRUE(cJSON_IsTrue(
        cJSON_GetObjectItemCaseSensitive(root, "ok")));
    operation_id = response_operation_id(root);
    cJSON_Delete(root);

    (void)snprintf(
        request, sizeof request,
        "{\"method\":\"game.scene.operation_status\","
        "\"params\":{\"operationId\":%.0f}}",
        operation_id);
    root = submit(request);
    result = response_result(root);
    TEST_ASSERT_EQUAL_STRING(
        "pending",
        cJSON_GetObjectItemCaseSensitive(result, "state")->valuestring);
    cJSON_Delete(root);

    root = submit(
        "{\"method\":\"game.scene.replace\","
        "\"params\":{\"scene\":\"settings\"}}");
    result = response_result(root);
    TEST_ASSERT_TRUE(cJSON_IsTrue(
        cJSON_GetObjectItemCaseSensitive(root, "ok")));
    TEST_ASSERT_EQUAL_STRING(
        "busy",
        cJSON_GetObjectItemCaseSensitive(result, "result")->valuestring);
    TEST_ASSERT_EQUAL_UINT64(
        (uint64_t)operation_id,
        (uint64_t)cJSON_GetObjectItemCaseSensitive(
            result, "blockingOperationId")->valuedouble);
    cJSON_Delete(root);

    scene_manager_step(&s_manager, 2, 0.016F);
    root = submit(request);
    result = response_result(root);
    TEST_ASSERT_EQUAL_STRING(
        "active",
        cJSON_GetObjectItemCaseSensitive(result, "state")->valuestring);
    cJSON_Delete(root);

    root = submit(
        "{\"method\":\"game.scene.back_to\","
        "\"params\":{\"scene\":\"settings\"}}");
    result = response_result(root);
    TEST_ASSERT_TRUE(cJSON_IsTrue(
        cJSON_GetObjectItemCaseSensitive(root, "ok")));
    TEST_ASSERT_EQUAL_STRING(
        "busy",
        cJSON_GetObjectItemCaseSensitive(result, "result")->valuestring);
    TEST_ASSERT_EQUAL_UINT64(
        (uint64_t)operation_id,
        (uint64_t)cJSON_GetObjectItemCaseSensitive(
            result, "blockingOperationId")->valuedouble);
    cJSON_Delete(root);

    s_next.ready = true;
    scene_manager_step(&s_manager, 3, 0.016F);
    root = submit(request);
    result = response_result(root);
    TEST_ASSERT_EQUAL_STRING(
        "completed",
        cJSON_GetObjectItemCaseSensitive(result, "state")->valuestring);
    cJSON_Delete(root);

    root = submit(
        "{\"method\":\"game.scene.operation_status\","
        "\"params\":{\"operationId\":1}}");
    TEST_ASSERT_TRUE(cJSON_IsFalse(
        cJSON_GetObjectItemCaseSensitive(root, "ok")));
    TEST_ASSERT_EQUAL_STRING("operation_not_found", error_code(root));
    cJSON_Delete(root);
}

static void test_duplicate_registration_reports_failure(void) {
    TEST_ASSERT_FALSE(scene_manager_register_devapi(&s_manager));
}

static void test_mid_registration_failure_requires_full_registry_reset(void) {
    const nt_devapi_command_desc conflict = {
        "game.scene.status", "test", "conflict", "{}", "{}",
        "immediate", "none",
    };
    cJSON *root;

    nt_devapi_shutdown();
    TEST_ASSERT_EQUAL(NT_OK, nt_devapi_init());
    TEST_ASSERT_EQUAL(
        NT_OK, nt_devapi_register(&conflict, dummy_endpoint, NULL));
    TEST_ASSERT_FALSE(scene_manager_register_devapi(&s_manager));

    root = submit(
        "{\"method\":\"game.scene.list\",\"params\":{}}");
    TEST_ASSERT_TRUE(cJSON_IsTrue(
        cJSON_GetObjectItemCaseSensitive(root, "ok")));
    cJSON_Delete(root);

    nt_devapi_shutdown();
    TEST_ASSERT_EQUAL(NT_OK, nt_devapi_init());
    TEST_ASSERT_TRUE(scene_manager_register_devapi(&s_manager));
    root = submit(
        "{\"method\":\"game.scene.status\",\"params\":{}}");
    TEST_ASSERT_TRUE(cJSON_IsTrue(
        cJSON_GetObjectItemCaseSensitive(root, "ok")));
    cJSON_Delete(root);
}

static void test_list_and_empty_status_response_shapes(void) {
    cJSON *root = submit(
        "{\"method\":\"game.scene.list\",\"params\":{}}");
    const cJSON *result = response_result(root);
    const cJSON *scenes =
        cJSON_GetObjectItemCaseSensitive(result, "scenes");
    const cJSON *first = cJSON_GetArrayItem(scenes, 0);
    TEST_ASSERT_TRUE(cJSON_IsTrue(
        cJSON_GetObjectItemCaseSensitive(root, "ok")));
    TEST_ASSERT_TRUE(cJSON_IsArray(scenes));
    TEST_ASSERT_EQUAL_INT(4, cJSON_GetArraySize(scenes));
    TEST_ASSERT_TRUE(cJSON_IsString(
        cJSON_GetObjectItemCaseSensitive(first, "id")));
    TEST_ASSERT_TRUE(cJSON_IsString(
        cJSON_GetObjectItemCaseSensitive(first, "kind")));
    TEST_ASSERT_TRUE(cJSON_IsBool(
        cJSON_GetObjectItemCaseSensitive(first, "debugOnly")));
    TEST_ASSERT_TRUE(cJSON_IsNumber(
        cJSON_GetObjectItemCaseSensitive(first, "argsSize")));
    TEST_ASSERT_TRUE(cJSON_IsString(
        cJSON_GetObjectItemCaseSensitive(first, "residency")));
    cJSON_Delete(root);

    reset_manager_with_empty_history();
    root = submit(
        "{\"method\":\"game.scene.status\",\"params\":{}}");
    result = response_result(root);
    TEST_ASSERT_TRUE(cJSON_IsTrue(
        cJSON_GetObjectItemCaseSensitive(root, "ok")));
    TEST_ASSERT_EQUAL_INT(
        0, cJSON_GetArraySize(
               cJSON_GetObjectItemCaseSensitive(result, "history")));
    TEST_ASSERT_TRUE(cJSON_IsFalse(
        cJSON_GetObjectItemCaseSensitive(result, "inputGated")));
    TEST_ASSERT_TRUE(cJSON_IsNull(
        cJSON_GetObjectItemCaseSensitive(result, "top")));
    cJSON_Delete(root);
}

static void test_navigation_and_preload_result_matrix(void) {
    cJSON *root = submit(
        "{\"method\":\"game.scene.back\",\"params\":{\"count\":1}}");
    TEST_ASSERT_EQUAL_STRING(
        "root_protected",
        cJSON_GetObjectItemCaseSensitive(
            response_result(root), "result")->valuestring);
    cJSON_Delete(root);

    root = submit(
        "{\"method\":\"game.scene.close_modals\",\"params\":{}}");
    TEST_ASSERT_EQUAL_STRING(
        "already_top",
        cJSON_GetObjectItemCaseSensitive(
            response_result(root), "result")->valuestring);
    cJSON_Delete(root);

    root = submit(
        "{\"method\":\"game.scene.show\","
        "\"params\":{\"scene\":\"settings\"}}");
    TEST_ASSERT_EQUAL_STRING(
        "accepted",
        cJSON_GetObjectItemCaseSensitive(
            response_result(root), "result")->valuestring);
    cJSON_Delete(root);
    scene_manager_step(&s_manager, 2, 0.016F);

    root = submit(
        "{\"method\":\"game.scene.close_modals\",\"params\":{}}");
    TEST_ASSERT_EQUAL_STRING(
        "accepted",
        cJSON_GetObjectItemCaseSensitive(
            response_result(root), "result")->valuestring);
    cJSON_Delete(root);
    scene_manager_step(&s_manager, 3, 0.016F);

    root = submit(
        "{\"method\":\"game.scene.show\","
        "\"params\":{\"scene\":\"next\"}}");
    cJSON_Delete(root);
    scene_manager_step(&s_manager, 4, 0.016F);
    root = submit(
        "{\"method\":\"game.scene.back_to\","
        "\"params\":{\"scene\":\"root\"}}");
    TEST_ASSERT_EQUAL_STRING(
        "accepted",
        cJSON_GetObjectItemCaseSensitive(
            response_result(root), "result")->valuestring);
    cJSON_Delete(root);
    scene_manager_step(&s_manager, 5, 0.016F);

    root = submit(
        "{\"method\":\"game.scene.replace\","
        "\"params\":{\"scene\":\"next\"}}");
    TEST_ASSERT_EQUAL_STRING(
        "accepted",
        cJSON_GetObjectItemCaseSensitive(
            response_result(root), "result")->valuestring);
    cJSON_Delete(root);
    scene_manager_step(&s_manager, 6, 0.016F);

    root = submit(
        "{\"method\":\"game.scene.reload\",\"params\":{}}");
    TEST_ASSERT_EQUAL_STRING(
        "accepted",
        cJSON_GetObjectItemCaseSensitive(
            response_result(root), "result")->valuestring);
    cJSON_Delete(root);
    scene_manager_step(&s_manager, 7, 0.016F);
    scene_manager_step(&s_manager, 8, 0.016F);

    root = submit(
        "{\"method\":\"game.scene.preload\","
        "\"params\":{\"scene\":\"settings\"}}");
    TEST_ASSERT_EQUAL_STRING(
        "scheduled",
        cJSON_GetObjectItemCaseSensitive(
            response_result(root), "result")->valuestring);
    cJSON_Delete(root);
    root = submit(
        "{\"method\":\"game.scene.preload\","
        "\"params\":{\"scene\":\"settings\"}}");
    TEST_ASSERT_EQUAL_STRING(
        "already_scheduled",
        cJSON_GetObjectItemCaseSensitive(
            response_result(root), "result")->valuestring);
    cJSON_Delete(root);
    scene_manager_step(&s_manager, 9, 0.016F);
    root = submit(
        "{\"method\":\"game.scene.preload\","
        "\"params\":{\"scene\":\"settings\"}}");
    TEST_ASSERT_EQUAL_STRING(
        "already_ready",
        cJSON_GetObjectItemCaseSensitive(
            response_result(root), "result")->valuestring);
    cJSON_Delete(root);
}

static void test_every_navigation_endpoint_reports_busy_blocker(void) {
    static const char *const busy_requests[] = {
        "{\"method\":\"game.scene.show\","
        "\"params\":{\"scene\":\"settings\"}}",
        "{\"method\":\"game.scene.replace\","
        "\"params\":{\"scene\":\"settings\"}}",
        "{\"method\":\"game.scene.reload\",\"params\":{}}",
        "{\"method\":\"game.scene.back\",\"params\":{\"count\":1}}",
        "{\"method\":\"game.scene.back_to\","
        "\"params\":{\"scene\":\"root\"}}",
        "{\"method\":\"game.scene.close_modals\",\"params\":{}}",
    };
    cJSON *root;
    double blocker;
    size_t i;

    s_next.ready = false;
    root = submit(
        "{\"method\":\"game.scene.show\","
        "\"params\":{\"scene\":\"next\"}}");
    assert_navigation_result(root, "accepted", true, false);
    blocker = response_operation_id(root);
    cJSON_Delete(root);

    for (i = 0; i < sizeof busy_requests / sizeof busy_requests[0]; ++i) {
        const cJSON *result;
        root = submit(busy_requests[i]);
        assert_navigation_result(root, "busy", false, true);
        result = response_result(root);
        TEST_ASSERT_EQUAL_UINT64(
            (uint64_t)blocker,
            (uint64_t)cJSON_GetObjectItemCaseSensitive(
                result, "blockingOperationId")->valuedouble);
        cJSON_Delete(root);
    }
}

static void test_accepted_back_and_preload_loading_mutate_state(void) {
    cJSON *root;

    root = submit(
        "{\"method\":\"game.scene.show\","
        "\"params\":{\"scene\":\"next\"}}");
    assert_navigation_result(root, "accepted", true, false);
    cJSON_Delete(root);
    scene_manager_step(&s_manager, 2, 0.016F);
    TEST_ASSERT_EQUAL_STRING("next", scene_manager_top(&s_manager).scene->id);

    root = submit(
        "{\"method\":\"game.scene.back\",\"params\":{\"count\":1}}");
    assert_navigation_result(root, "accepted", true, false);
    cJSON_Delete(root);
    scene_manager_step(&s_manager, 3, 0.016F);
    TEST_ASSERT_EQUAL_STRING("root", scene_manager_top(&s_manager).scene->id);
    TEST_ASSERT_EQUAL_UINT32(1, scene_manager_history_count(&s_manager));

    s_modal.ready = false;
    root = submit(
        "{\"method\":\"game.scene.preload\","
        "\"params\":{\"scene\":\"settings\"}}");
    TEST_ASSERT_EQUAL_STRING(
        "scheduled",
        cJSON_GetObjectItemCaseSensitive(
            response_result(root), "result")->valuestring);
    cJSON_Delete(root);
    scene_manager_step(&s_manager, 4, 0.016F);

    root = submit(
        "{\"method\":\"game.scene.preload\","
        "\"params\":{\"scene\":\"settings\"}}");
    TEST_ASSERT_EQUAL_STRING(
        "already_loading",
        cJSON_GetObjectItemCaseSensitive(
            response_result(root), "result")->valuestring);
    cJSON_Delete(root);

    s_modal.ready = true;
    scene_manager_step(&s_manager, 5, 0.016F);
    TEST_ASSERT_EQUAL(
        SCENE_RESIDENCY_READY,
        scene_manager_scene_residency(
            &s_manager, &k_catalog[2]));
}

static int dump_schema_fixtures(void) {
    static const char *const requests[] = {
        "{\"method\":\"game.scene.list\",\"params\":{}}",
        "{\"method\":\"game.scene.status\",\"params\":{}}",
        "{\"method\":\"game.scene.back\",\"params\":{\"count\":1}}",
        "{\"method\":\"game.scene.show\",\"params\":{\"scene\":\"next\"}}",
        "{\"method\":\"game.scene.reload\",\"params\":{}}",
        "{\"method\":\"game.scene.operation_status\","
        "\"params\":{\"operationId\":2}}",
        "{\"method\":\"game.scene.preload\","
        "\"params\":{\"scene\":\"settings\"}}",
        "{\"method\":\"game.scene.show\",\"params\":{}}",
    };
    scene_operation_id_t operation_id;
    size_t i;

    s_root.ready = true;
    s_next.ready = true;
    s_modal.ready = true;
    s_typed.ready = true;
    scene_manager_init(
        &s_manager, k_catalog, sizeof k_catalog / sizeof k_catalog[0]);
    if (scene_manager_start(
            &s_manager, "root", (scene_route_args_view_t){0},
            &operation_id, NULL) != SCENE_RESULT_ACCEPTED) {
        return 2;
    }
    scene_manager_step(&s_manager, 1, 0.016F);
    if (nt_devapi_init() != NT_OK ||
        !scene_manager_register_devapi(&s_manager)) {
        return 3;
    }
    for (i = 0; i < sizeof requests / sizeof requests[0]; ++i) {
        const char *response = nt_devapi_submit(requests[i]);
        if (response == NULL) {
            nt_devapi_shutdown();
            scene_manager_shutdown(&s_manager);
            return 4;
        }
        puts(response);
    }
    nt_devapi_shutdown();
    scene_manager_shutdown(&s_manager);
    return 0;
}

int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "--dump-schema-fixtures") == 0) {
        return dump_schema_fixtures();
    }
    UNITY_BEGIN();
    RUN_TEST(test_duplicate_keys_reject_before_navigation);
    RUN_TEST(test_replace_cross_kind_returns_error_without_mutation);
    RUN_TEST(test_parameterized_scene_can_preload_generically);
    RUN_TEST(test_scene_id_format_matches_the_published_schema);
    RUN_TEST(test_show_before_start_returns_bad_operation);
    RUN_TEST(test_replace_before_start_returns_bad_operation);
    RUN_TEST(test_reload_before_start_returns_bad_operation);
    RUN_TEST(test_strict_param_type_range_and_unknown_key_matrix);
    RUN_TEST(test_operation_polling_and_busy_blocker_are_deterministic);
    RUN_TEST(test_duplicate_registration_reports_failure);
    RUN_TEST(test_mid_registration_failure_requires_full_registry_reset);
    RUN_TEST(test_list_and_empty_status_response_shapes);
    RUN_TEST(test_navigation_and_preload_result_matrix);
    RUN_TEST(test_every_navigation_endpoint_reports_busy_blocker);
    RUN_TEST(test_accepted_back_and_preload_loading_mutate_state);
    return UNITY_END();
}
