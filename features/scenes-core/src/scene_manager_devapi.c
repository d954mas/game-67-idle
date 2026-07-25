#include "features/scenes/scene_manager_devapi.h"
#include "scene_id.h"

#if NT_DEVAPI_ENABLED

#include "cJSON.h"
#include "devapi/nt_devapi.h"

#include <math.h>
#include <string.h>

static bool fail(nt_devapi_error *error,
                 const char *code,
                 const char *message) {
    error->code = code;
    error->message = message;
    return false;
}

static bool strict_keys(const cJSON *params,
                        const char *const *allowed,
                        size_t allowed_count,
                        nt_devapi_error *error) {
    const cJSON *item;
    if (params == NULL) {
        return allowed_count == 0 ||
               fail(error, "bad_params", "params object is required");
    }
    if (!cJSON_IsObject(params)) {
        return fail(error, "bad_params", "params must be an object");
    }
    for (item = params->child; item != NULL; item = item->next) {
        const cJSON *previous;
        size_t i;
        bool known = false;
        for (i = 0; i < allowed_count; ++i) {
            if (item->string != NULL &&
                strcmp(item->string, allowed[i]) == 0) {
                known = true;
                break;
            }
        }
        if (!known) {
            return fail(error, "bad_params", "unknown parameter");
        }
        for (previous = params->child; previous != item;
             previous = previous->next) {
            if (previous->string != NULL && item->string != NULL &&
                strcmp(previous->string, item->string) == 0) {
                return fail(error, "bad_params",
                            "duplicate parameter");
            }
        }
    }
    return true;
}

static const char *required_scene_id(const cJSON *params,
                                     nt_devapi_error *error) {
    const cJSON *scene = cJSON_GetObjectItemCaseSensitive(params, "scene");
    if (!cJSON_IsString(scene) ||
        !scene_id_is_valid(scene->valuestring)) {
        (void)fail(
            error, "bad_params",
            "scene must match [a-z_][a-z0-9._-]{0,126}");
        return NULL;
    }
    return scene->valuestring;
}

static const char *kind_name(scene_kind_t kind) {
    return kind == SCENE_KIND_SCREEN ? "screen" : "modal";
}

static const char *residency_name(scene_residency_t residency) {
    switch (residency) {
    case SCENE_RESIDENCY_UNLOADED:
        return "unloaded";
    case SCENE_RESIDENCY_LOADING:
        return "loading";
    case SCENE_RESIDENCY_READY:
        return "ready";
    }
    return "unknown";
}

static const char *operation_state_name(scene_operation_state_t state) {
    switch (state) {
    case SCENE_OPERATION_PENDING:
        return "pending";
    case SCENE_OPERATION_ACTIVE:
        return "active";
    case SCENE_OPERATION_COMPLETED:
        return "completed";
    case SCENE_OPERATION_NOT_FOUND:
        return "not_found";
    }
    return "not_found";
}

static const char *operation_kind_name(scene_operation_kind_t kind) {
    switch (kind) {
    case SCENE_OPERATION_START:
        return "start";
    case SCENE_OPERATION_SHOW:
        return "show";
    case SCENE_OPERATION_REPLACE:
        return "replace";
    case SCENE_OPERATION_RELOAD:
        return "reload";
    case SCENE_OPERATION_BACK:
        return "back";
    case SCENE_OPERATION_BACK_TO:
        return "back_to";
    case SCENE_OPERATION_CLOSE_MODALS:
        return "close_modals";
    }
    return "unknown";
}

static void add_operation(cJSON *result,
                          const scene_operation_status_t *status) {
    cJSON_AddNumberToObject(result, "operationId", (double)status->id);
    cJSON_AddStringToObject(result, "kind",
                            operation_kind_name(status->kind));
    cJSON_AddStringToObject(result, "state",
                            operation_state_name(status->state));
    if (status->target_scene_id != NULL) {
        cJSON_AddStringToObject(result, "scene",
                                status->target_scene_id);
    } else {
        cJSON_AddNullToObject(result, "scene");
    }
}

static bool ep_list(const cJSON *params,
                    cJSON *result,
                    nt_devapi_error *error,
                    void *user) {
    scene_manager_t *manager = user;
    cJSON *scenes;
    size_t i;
    if (!strict_keys(params, NULL, 0, error)) {
        return false;
    }
    scenes = cJSON_AddArrayToObject(result, "scenes");
    for (i = 0; i < scene_manager_scene_count(manager); ++i) {
        const scene_descriptor_t *scene;
        cJSON *entry;
        /*
         * Catalog order is exposed through lookup of history-independent
         * descriptor ids by the manager's immutable descriptor array. The
         * public snapshot deliberately carries descriptors, not mutable
         * runtime objects.
         */
        scene = scene_manager_catalog_scene(manager, i);
        entry = cJSON_CreateObject();
        cJSON_AddStringToObject(entry, "id", scene->id);
        cJSON_AddStringToObject(entry, "kind", kind_name(scene->kind));
        cJSON_AddBoolToObject(entry, "debugOnly", scene->debug_only);
        cJSON_AddNumberToObject(entry, "argsSize",
                                scene->route_args_size);
        cJSON_AddStringToObject(
            entry, "residency",
            residency_name(
                scene_manager_scene_residency(manager, scene)));
        cJSON_AddItemToArray(scenes, entry);
    }
    return true;
}

static bool ep_status(const cJSON *params,
                      cJSON *result,
                      nt_devapi_error *error,
                      void *user) {
    scene_manager_t *manager = user;
    cJSON *history;
    size_t i;
    if (!strict_keys(params, NULL, 0, error)) {
        return false;
    }
    history = cJSON_AddArrayToObject(result, "history");
    for (i = 0; i < scene_manager_history_count(manager); ++i) {
        const scene_history_entry_view_t view =
            scene_manager_history_entry(manager, i);
        cJSON *entry = cJSON_CreateObject();
        cJSON_AddStringToObject(entry, "id", view.scene->id);
        cJSON_AddNumberToObject(entry, "argsSize", view.args.size);
        cJSON_AddItemToArray(history, entry);
    }
    cJSON_AddBoolToObject(result, "inputGated",
                          scene_manager_input_gated(manager));
    if (scene_manager_history_count(manager) > 0) {
        cJSON_AddStringToObject(
            result, "top", scene_manager_top(manager).scene->id);
    } else {
        cJSON_AddNullToObject(result, "top");
    }
    return true;
}

static bool ep_operation_status(const cJSON *params,
                                cJSON *result,
                                nt_devapi_error *error,
                                void *user) {
    static const char *const keys[] = {"operationId"};
    const cJSON *id;
    scene_operation_status_t status;
    scene_operation_state_t state;
    if (!strict_keys(params, keys, 1, error)) {
        return false;
    }
    id = cJSON_GetObjectItemCaseSensitive(params, "operationId");
    if (!cJSON_IsNumber(id) || !isfinite(id->valuedouble) ||
        id->valuedouble < 1.0 ||
        id->valuedouble > (double)SCENE_OPERATION_ID_MAX ||
        floor(id->valuedouble) != id->valuedouble) {
        return fail(error, "bad_params",
                    "operationId must be a positive integer");
    }
    state = scene_manager_operation_status(
        user, (scene_operation_id_t)id->valuedouble, &status);
    if (state == SCENE_OPERATION_NOT_FOUND) {
        return fail(error, "operation_not_found",
                    "operation is no longer retained");
    }
    add_operation(result, &status);
    return true;
}

static bool ensure_parameterless(scene_manager_t *manager,
                                 const char *id,
                                 const scene_descriptor_t **scene,
                                 nt_devapi_error *error) {
    *scene = scene_manager_find_scene(manager, id);
    if (*scene == NULL) {
        return fail(error, "scene_not_found", "unknown scene");
    }
    if ((*scene)->route_args_size != 0) {
        return fail(error, "typed_endpoint_required",
                    "scene requires its game-owned typed endpoint");
    }
    return true;
}

static void add_navigation_result(cJSON *result,
                                  scene_result_t nav_result,
                                  scene_operation_id_t operation_id,
                                  scene_operation_id_t blocker_id) {
    switch (nav_result) {
    case SCENE_RESULT_ACCEPTED:
        cJSON_AddStringToObject(result, "result", "accepted");
        cJSON_AddNumberToObject(result, "operationId",
                                (double)operation_id);
        break;
    case SCENE_RESULT_BUSY:
        cJSON_AddStringToObject(result, "result", "busy");
        cJSON_AddNumberToObject(result, "blockingOperationId",
                                (double)blocker_id);
        break;
    case SCENE_RESULT_ALREADY_TOP:
        cJSON_AddStringToObject(result, "result", "already_top");
        break;
    case SCENE_RESULT_NOT_TOP:
        cJSON_AddStringToObject(result, "result", "not_top");
        break;
    case SCENE_RESULT_ROOT_PROTECTED:
        cJSON_AddStringToObject(result, "result", "root_protected");
        break;
    }
}

static bool add_busy_result_if_needed(scene_manager_t *manager,
                                      cJSON *result) {
    scene_operation_status_t status;
    if (!scene_manager_current_operation(manager, &status)) {
        return false;
    }
    add_navigation_result(
        result, SCENE_RESULT_BUSY, 0, status.id);
    return true;
}

static bool ep_preload(const cJSON *params,
                       cJSON *result,
                       nt_devapi_error *error,
                       void *user) {
    static const char *const keys[] = {"scene"};
    const char *id;
    const scene_descriptor_t *scene;
    scene_preload_result_t preload;
    static const char *const names[] = {
        "scheduled", "already_scheduled", "already_loading", "already_ready"};
    if (!strict_keys(params, keys, 1, error)) {
        return false;
    }
    id = required_scene_id(params, error);
    if (id == NULL) {
        return false;
    }
    scene = scene_manager_find_scene(user, id);
    if (scene == NULL) {
        return fail(error, "scene_not_found", "unknown scene");
    }
    preload = scene_manager_preload(user, id);
    cJSON_AddStringToObject(result, "result", names[preload]);
    return true;
}

static bool route_scene(const cJSON *params,
                        cJSON *result,
                        nt_devapi_error *error,
                        scene_manager_t *manager,
                        bool replace) {
    static const char *const keys[] = {"scene"};
    const char *id;
    const scene_descriptor_t *scene;
    scene_operation_id_t operation_id = 0;
    scene_operation_id_t blocker_id = 0;
    scene_result_t nav_result;
    if (!strict_keys(params, keys, 1, error)) {
        return false;
    }
    id = required_scene_id(params, error);
    if (id == NULL ||
        !ensure_parameterless(manager, id, &scene, error)) {
        return false;
    }
    if (add_busy_result_if_needed(manager, result)) {
        return true;
    }
    if (scene_manager_history_count(manager) == 0) {
        return fail(error, "bad_operation",
                    "scene manager has no root scene");
    }
    if (replace &&
        scene_manager_top(manager).scene->kind != scene->kind) {
        return fail(error, "bad_operation",
                    "replace requires the current scene kind");
    }
    nav_result =
        replace
            ? scene_manager_replace(
                  manager, id, (scene_route_args_view_t){0},
                  &operation_id, &blocker_id)
            : scene_manager_show(
                  manager, id, (scene_route_args_view_t){0},
                  &operation_id, &blocker_id);
    add_navigation_result(result, nav_result, operation_id, blocker_id);
    return true;
}

static bool ep_show(const cJSON *params,
                    cJSON *result,
                    nt_devapi_error *error,
                    void *user) {
    return route_scene(params, result, error, user, false);
}

static bool ep_replace(const cJSON *params,
                       cJSON *result,
                       nt_devapi_error *error,
                       void *user) {
    return route_scene(params, result, error, user, true);
}

static bool no_target_navigation(const cJSON *params,
                                 cJSON *result,
                                 nt_devapi_error *error,
                                 scene_manager_t *manager,
                                 scene_operation_kind_t kind) {
    scene_operation_id_t operation_id = 0;
    scene_operation_id_t blocker_id = 0;
    scene_result_t nav_result;
    if (!strict_keys(params, NULL, 0, error)) {
        return false;
    }
    if (kind == SCENE_OPERATION_RELOAD) {
        if (scene_manager_history_count(manager) == 0) {
            return fail(error, "bad_operation",
                        "scene manager has no root scene");
        }
        nav_result = scene_manager_reload(
            manager, &operation_id, &blocker_id);
    } else {
        nav_result = scene_manager_close_modals(
            manager, &operation_id, &blocker_id);
    }
    add_navigation_result(result, nav_result, operation_id, blocker_id);
    return true;
}

static bool ep_reload(const cJSON *params,
                      cJSON *result,
                      nt_devapi_error *error,
                      void *user) {
    return no_target_navigation(params, result, error, user,
                                SCENE_OPERATION_RELOAD);
}

static bool ep_close_modals(const cJSON *params,
                            cJSON *result,
                            nt_devapi_error *error,
                            void *user) {
    return no_target_navigation(params, result, error, user,
                                SCENE_OPERATION_CLOSE_MODALS);
}

static bool ep_back(const cJSON *params,
                    cJSON *result,
                    nt_devapi_error *error,
                    void *user) {
    static const char *const keys[] = {"count"};
    const cJSON *count;
    scene_operation_id_t operation_id = 0;
    scene_operation_id_t blocker_id = 0;
    scene_result_t nav_result;
    if (!strict_keys(params, keys, 1, error)) {
        return false;
    }
    count = cJSON_GetObjectItemCaseSensitive(params, "count");
    if (!cJSON_IsNumber(count) || !isfinite(count->valuedouble) ||
        count->valuedouble < 1.0 ||
        count->valuedouble > (double)SCENE_MANAGER_MAX_HISTORY ||
        floor(count->valuedouble) != count->valuedouble) {
        return fail(error, "bad_params",
                    "count must be an integer from 1 to 128");
    }
    nav_result = scene_manager_back(
        user, (size_t)count->valuedouble, &operation_id, &blocker_id);
    add_navigation_result(result, nav_result, operation_id, blocker_id);
    return true;
}

static bool ep_back_to(const cJSON *params,
                       cJSON *result,
                       nt_devapi_error *error,
                       void *user) {
    static const char *const keys[] = {"scene"};
    const char *id;
    scene_operation_id_t operation_id = 0;
    scene_operation_id_t blocker_id = 0;
    scene_result_t nav_result;
    if (!strict_keys(params, keys, 1, error)) {
        return false;
    }
    id = required_scene_id(params, error);
    if (id == NULL || !scene_manager_has_scene(user, id)) {
        return fail(error, "scene_not_found", "unknown scene");
    }
    if (add_busy_result_if_needed(user, result)) {
        return true;
    }
    if (!scene_manager_can_back_to(user, id)) {
        return fail(error, "back_target_not_found",
                    "scene is not present below the top");
    }
    nav_result = scene_manager_back_to(
        user, id, &operation_id, &blocker_id);
    add_navigation_result(result, nav_result, operation_id, blocker_id);
    return true;
}

bool scene_manager_register_devapi(scene_manager_t *manager) {
    static const nt_devapi_command_desc descriptions[] = {
        {"game.scene.list", "game.scene", "List scene catalog and residency.",
         "{}", "{scenes:[...]}", "immediate", "none"},
        {"game.scene.status", "game.scene", "Return scene history and input gate.",
         "{}", "{history:[...],top,inputGated}", "immediate", "none"},
        {"game.scene.operation_status", "game.scene", "Poll a navigation operation.",
         "{operationId:uint}", "{operationId,kind,state,scene}", "immediate", "none"},
        {"game.scene.preload", "game.scene", "Hint that a scene will be needed.",
         "{scene:string}", "{result}", "deferred", "starts scene load"},
        {"game.scene.show", "game.scene", "Show a parameterless scene.",
         "{scene:string}", "{result,operationId?}", "deferred", "navigation"},
        {"game.scene.replace", "game.scene", "Replace top with a parameterless same-kind scene.",
         "{scene:string}", "{result,operationId?}", "deferred", "navigation"},
        {"game.scene.reload", "game.scene", "Fully unload and load the top scene.",
         "{}", "{result,operationId?}", "deferred", "navigation"},
        {"game.scene.back", "game.scene", "Atomically remove count history entries.",
         "{count:uint}", "{result,operationId?}", "deferred", "navigation"},
        {"game.scene.back_to", "game.scene", "Return to nearest named scene below top.",
         "{scene:string}", "{result,operationId?}", "deferred", "navigation"},
        {"game.scene.close_modals", "game.scene", "Atomically close the modal suffix.",
         "{}", "{result,operationId?}", "deferred", "navigation"},
    };
    static const nt_devapi_handler_fn handlers[] = {
        ep_list, ep_status, ep_operation_status, ep_preload, ep_show,
        ep_replace, ep_reload, ep_back, ep_back_to, ep_close_modals,
    };
    size_t i;
    for (i = 0; i < sizeof handlers / sizeof handlers[0]; ++i) {
        if (nt_devapi_register(
                &descriptions[i], handlers[i], manager) != NT_OK) {
            return false;
        }
    }
    return true;
}

#else

bool scene_manager_register_devapi(scene_manager_t *manager) {
    (void)manager;
    return true;
}

#endif
