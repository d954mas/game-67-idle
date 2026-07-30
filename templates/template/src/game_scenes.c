#include "game_scenes.h"

#include "features/game_features.h"
#include "features/settings/settings.h"
#include "clay.h"
#include "ui/nt_ui.h"
#include "ui/demo_hud.h"
/* scene-scaffold:includes */

#include <stdlib.h>

typedef struct game_scene_instance {
    World *world;
} game_scene_instance_t;

static scene_manager_t s_manager;
static game_scene_instance_t s_root;
static game_scene_instance_t s_settings;
static bool s_initialized;
static const game_input_frame_t *s_frame_input;

static scene_load_result_t scene_load_step(void *instance) {
    (void)instance;
    return SCENE_LOAD_READY;
}

static void root_update(void *instance, float dt) {
    game_scene_instance_t *scene = instance;
    game_features_update_root(scene->world, dt, s_frame_input);
}

static void root_ui(void *instance, void *ui_context, scene_ui_mode_t mode) {
    (void)instance;
    demo_hud_draw_ui(ui_context);
    settings_draw_launcher(ui_context, mode == SCENE_UI_INTERACTIVE);
}

static void settings_ui(void *instance,
                        void *ui_context,
                        scene_ui_mode_t mode) {
    game_scene_instance_t *scene = instance;
    settings_draw_panel(ui_context, scene->world,
                        mode == SCENE_UI_INTERACTIVE);
}

static const scene_api_t k_root_api = {
    .load_step = scene_load_step,
    .on_update = root_update,
    .on_ui = root_ui,
};

static const scene_api_t k_settings_api = {
    .load_step = scene_load_step,
    .on_ui = settings_ui,
};

static const scene_descriptor_t k_catalog[] = {
    {
        .id = GAME_SCENE_ROOT,
        .kind = SCENE_KIND_SCREEN,
        .keep_loaded = true,
        .instance = &s_root,
        .api = &k_root_api,
    },
    {
        .id = GAME_SCENE_SETTINGS,
        .kind = SCENE_KIND_MODAL,
        .modal_update_policy = SCENE_MODAL_PAUSE_BELOW,
        .instance = &s_settings,
        .api = &k_settings_api,
    },
    /* scene-scaffold:catalog */
};

void game_scenes_init(World *world) {
    scene_operation_id_t operation_id;
    s_root.world = world;
    s_settings.world = world;
    scene_manager_init(&s_manager, k_catalog,
                       sizeof k_catalog / sizeof k_catalog[0]);
    const scene_result_t start_result = scene_manager_start(
        &s_manager, GAME_SCENE_ROOT, (scene_route_args_view_t){0},
        &operation_id, NULL);
    if (start_result != SCENE_RESULT_ACCEPTED) {
        abort();
    }
    scene_manager_step(&s_manager, 0, 0.0F);
    s_initialized = true;
}

void game_scenes_step(uint64_t frame_index, float dt) {
    if (s_initialized) {
        scene_manager_step(&s_manager, frame_index, dt);
    }
}

void game_scenes_update(float dt, const game_input_frame_t *input) {
    if (s_initialized) {
        s_frame_input = input;
        scene_manager_update(&s_manager, dt);
        s_frame_input = NULL;
    }
}

void game_scenes_build_ui(nt_ui_context_t *ui_context) {
    if (s_initialized) {
        scene_manager_build_ui(&s_manager, ui_context);
    }
}

void game_scenes_build_input_gate(nt_ui_context_t *ui_context) {
    if (!s_initialized || !scene_manager_input_gated(&s_manager)) {
        return;
    }
    CLAY({
        .id = CLAY_ID("scene/navigation/input_gate"),
        .floating = {
            .attachTo = CLAY_ATTACH_TO_ROOT,
            .zIndex = 32760,
        },
        .layout = {
            .sizing = {
                CLAY_SIZING_GROW(0),
                CLAY_SIZING_GROW(0),
            },
        },
    }) {
        nt_ui_block_pointer(
            ui_context, nt_ui_id("scene/navigation/input_gate"), NULL);
    }
}

void game_scenes_shutdown(void) {
    if (s_initialized) {
        scene_manager_shutdown(&s_manager);
        s_initialized = false;
    }
}

scene_manager_t *game_scenes_manager(void) {
    return s_initialized ? &s_manager : NULL;
}

bool game_scenes_is_in_history(const char *scene_id) {
    return s_initialized && scene_manager_contains(&s_manager, scene_id);
}

bool game_scenes_is_presented(const char *scene_id) {
    const scene_descriptor_t *scene;
    if (!s_initialized) {
        return false;
    }
    scene = scene_manager_find_scene(&s_manager, scene_id);
    return scene != NULL &&
           scene_manager_is_presented(&s_manager, scene);
}

bool game_scenes_can_process_game_input(void) {
    return s_initialized &&
           scene_manager_can_process_input(&s_manager, &k_catalog[0]);
}

bool game_scenes_should_render_world(void) {
    return s_initialized &&
           scene_manager_is_presented(&s_manager, &k_catalog[0]);
}

bool game_scenes_input_gated(void) {
    return s_initialized && scene_manager_input_gated(&s_manager);
}

bool game_scenes_handle_escape(void) {
    scene_operation_id_t operation_id;
    scene_history_entry_view_t top;
    if (!s_initialized) {
        return false;
    }
    if (scene_manager_input_gated(&s_manager)) {
        return true;
    }
    top = scene_manager_top(&s_manager);
    if (!top.scene || top.scene->kind != SCENE_KIND_MODAL) {
        return false;
    }
    (void)scene_manager_back(&s_manager, 1, &operation_id, NULL);
    return true;
}

scene_result_t game_scenes_show_settings(void) {
    scene_operation_id_t operation_id;
    if (!s_initialized) {
        return SCENE_RESULT_BUSY;
    }
    return scene_manager_show(
        &s_manager, GAME_SCENE_SETTINGS, (scene_route_args_view_t){0},
        &operation_id, NULL);
}

scene_result_t game_scenes_close_settings(void) {
    scene_operation_id_t operation_id;
    const scene_descriptor_t *settings_scene;
    if (!s_initialized ||
        !scene_manager_contains(&s_manager, GAME_SCENE_SETTINGS)) {
        return SCENE_RESULT_ALREADY_TOP;
    }
    settings_scene =
        scene_manager_find_scene(&s_manager, GAME_SCENE_SETTINGS);
    if (scene_manager_top(&s_manager).scene != settings_scene) {
        return SCENE_RESULT_NOT_TOP;
    }
    return scene_manager_back(&s_manager, 1, &operation_id, NULL);
}
