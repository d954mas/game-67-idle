#ifndef FEATURES_SCENES_SCENE_MANAGER_H
#define FEATURES_SCENES_SCENE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    SCENE_MANAGER_MAX_SCENES = 64,
    SCENE_MANAGER_MAX_HISTORY = 128,
    SCENE_ID_MAX_LENGTH = 127,
    SCENE_ROUTE_ARGS_INLINE_CAPACITY = 64,
    SCENE_MANAGER_STORAGE_BYTES = 32768
};

#ifndef SCENE_MANAGER_LOAD_DEADLINE_STEPS
#define SCENE_MANAGER_LOAD_DEADLINE_STEPS 36000u
#endif

#ifndef SCENE_MANAGER_TRANSITION_DEADLINE_STEPS
#define SCENE_MANAGER_TRANSITION_DEADLINE_STEPS 3600u
#endif

typedef uint64_t scene_operation_id_t;

#define SCENE_OPERATION_ID_MAX UINT64_C(9007199254740991)

typedef enum scene_kind {
    SCENE_KIND_SCREEN,
    SCENE_KIND_MODAL
} scene_kind_t;

typedef enum scene_modal_update_policy {
    SCENE_MODAL_PAUSE_BELOW,
    SCENE_MODAL_CONTINUE_BELOW
} scene_modal_update_policy_t;

typedef enum scene_load_result {
    SCENE_LOAD_PENDING,
    SCENE_LOAD_READY
} scene_load_result_t;

typedef enum scene_ui_mode {
    SCENE_UI_INTERACTIVE,
    SCENE_UI_PASSIVE
} scene_ui_mode_t;

typedef enum scene_residency {
    SCENE_RESIDENCY_UNLOADED,
    SCENE_RESIDENCY_LOADING,
    SCENE_RESIDENCY_READY
} scene_residency_t;

typedef enum scene_transition_direction {
    SCENE_TRANSITION_ENTER,
    SCENE_TRANSITION_EXIT
} scene_transition_direction_t;

typedef enum scene_transition_result {
    SCENE_TRANSITION_PENDING,
    SCENE_TRANSITION_DONE
} scene_transition_result_t;

typedef enum scene_result {
    SCENE_RESULT_ACCEPTED,
    SCENE_RESULT_BUSY,
    SCENE_RESULT_ALREADY_TOP,
    /* Game-owned id-specific close/dismiss helper found the scene below top. */
    SCENE_RESULT_NOT_TOP,
    SCENE_RESULT_ROOT_PROTECTED
} scene_result_t;

typedef enum scene_preload_result {
    SCENE_PRELOAD_SCHEDULED,
    SCENE_PRELOAD_ALREADY_SCHEDULED,
    SCENE_PRELOAD_ALREADY_LOADING,
    SCENE_PRELOAD_ALREADY_READY
} scene_preload_result_t;

typedef enum scene_operation_state {
    SCENE_OPERATION_NOT_FOUND,
    SCENE_OPERATION_PENDING,
    SCENE_OPERATION_ACTIVE,
    SCENE_OPERATION_COMPLETED
} scene_operation_state_t;

typedef enum scene_operation_kind {
    SCENE_OPERATION_START,
    SCENE_OPERATION_SHOW,
    SCENE_OPERATION_REPLACE,
    SCENE_OPERATION_RELOAD,
    SCENE_OPERATION_BACK,
    SCENE_OPERATION_BACK_TO,
    SCENE_OPERATION_CLOSE_MODALS
} scene_operation_kind_t;

typedef struct scene_route_args_view {
    const void *data;
    uint8_t size;
} scene_route_args_view_t;

typedef struct scene_api {
    void (*load_begin)(void *scene);
    scene_load_result_t (*load_step)(void *scene);
    void (*unload)(void *scene);
    void (*on_show)(void *scene, scene_route_args_view_t args);
    void (*on_hide)(void *scene);
    void (*on_pause)(void *scene);
    void (*on_resume)(void *scene);
    void (*on_update)(void *scene, float dt);
    void (*on_ui)(void *scene, void *ui_context, scene_ui_mode_t mode);
} scene_api_t;

typedef struct scene_transition_api {
    void (*begin)(void *scene, scene_transition_direction_t direction);
    scene_transition_result_t (*step)(void *scene, float dt);
} scene_transition_api_t;

typedef struct scene_descriptor {
    const char *id;
    scene_kind_t kind;
    bool keep_loaded;
    bool debug_only;
    scene_modal_update_policy_t modal_update_policy;
    uint8_t route_args_size;
    void *instance;
    const scene_api_t *api;
    const scene_transition_api_t *transitions;
} scene_descriptor_t;

typedef struct scene_history_entry_view {
    const scene_descriptor_t *scene;
    scene_route_args_view_t args;
} scene_history_entry_view_t;

typedef struct scene_operation_status {
    scene_operation_id_t id;
    scene_operation_kind_t kind;
    scene_operation_state_t state;
    const char *target_scene_id;
} scene_operation_status_t;

/*
 * Opaque, caller-owned fixed storage. Descriptors, callback tables, ids, and
 * scene instances must outlive this object.
 */
typedef union scene_manager {
    max_align_t _alignment;
    unsigned char _storage[SCENE_MANAGER_STORAGE_BYTES];
} scene_manager_t;

void scene_manager_init(scene_manager_t *manager,
                        const scene_descriptor_t *catalog,
                        size_t scene_count);
void scene_manager_shutdown(scene_manager_t *manager);

bool scene_manager_has_scene(const scene_manager_t *manager,
                             const char *scene_id);
const scene_descriptor_t *scene_manager_find_scene(
    const scene_manager_t *manager,
    const char *scene_id);
size_t scene_manager_scene_count(const scene_manager_t *manager);
const scene_descriptor_t *scene_manager_catalog_scene(
    const scene_manager_t *manager,
    size_t index);
scene_residency_t scene_manager_scene_residency(
    const scene_manager_t *manager,
    const scene_descriptor_t *scene);

size_t scene_manager_history_count(const scene_manager_t *manager);
scene_history_entry_view_t scene_manager_history_entry(
    const scene_manager_t *manager,
    size_t index);
scene_history_entry_view_t scene_manager_top(const scene_manager_t *manager);
bool scene_manager_contains(const scene_manager_t *manager,
                            const char *scene_id);
bool scene_manager_is_presented(const scene_manager_t *manager,
                                const scene_descriptor_t *scene);

scene_preload_result_t scene_manager_preload(scene_manager_t *manager,
                                             const char *scene_id);

scene_result_t scene_manager_start(scene_manager_t *manager,
                                   const char *scene_id,
                                   scene_route_args_view_t args,
                                   scene_operation_id_t *operation_id,
                                   scene_operation_id_t *blocking_operation_id);
scene_result_t scene_manager_show(scene_manager_t *manager,
                                  const char *scene_id,
                                  scene_route_args_view_t args,
                                  scene_operation_id_t *operation_id,
                                  scene_operation_id_t *blocking_operation_id);
scene_result_t scene_manager_replace(
    scene_manager_t *manager,
    const char *scene_id,
    scene_route_args_view_t args,
    scene_operation_id_t *operation_id,
    scene_operation_id_t *blocking_operation_id);
scene_result_t scene_manager_reload(
    scene_manager_t *manager,
    scene_operation_id_t *operation_id,
    scene_operation_id_t *blocking_operation_id);
scene_result_t scene_manager_back(scene_manager_t *manager,
                                  size_t count,
                                  scene_operation_id_t *operation_id,
                                  scene_operation_id_t *blocking_operation_id);
scene_result_t scene_manager_back_to(
    scene_manager_t *manager,
    const char *scene_id,
    scene_operation_id_t *operation_id,
    scene_operation_id_t *blocking_operation_id);
bool scene_manager_can_back_to(const scene_manager_t *manager,
                               const char *scene_id);
scene_result_t scene_manager_close_modals(
    scene_manager_t *manager,
    scene_operation_id_t *operation_id,
    scene_operation_id_t *blocking_operation_id);

void scene_manager_step(scene_manager_t *manager,
                        uint64_t frame_index,
                        float dt);
void scene_manager_update(scene_manager_t *manager, float dt);
void scene_manager_build_ui(scene_manager_t *manager, void *ui_context);

bool scene_manager_can_process_input(
    const scene_manager_t *manager,
    const scene_descriptor_t *scene);
bool scene_manager_input_gated(const scene_manager_t *manager);

scene_operation_state_t scene_manager_operation_status(
    const scene_manager_t *manager,
    scene_operation_id_t operation_id,
    scene_operation_status_t *status);
bool scene_manager_current_operation(const scene_manager_t *manager,
                                     scene_operation_status_t *status);
bool scene_manager_last_completed_operation(
    const scene_manager_t *manager,
    scene_operation_status_t *status);

#ifdef __cplusplus
}
#endif

#endif
