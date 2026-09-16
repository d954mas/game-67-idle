#include "scenes/login_confirm_scene.h"

#include "ui/login_prompt.h"

static scene_load_result_t load_step(void *instance) {
    (void)instance;
    return SCENE_LOAD_READY;
}

static void on_show(void *instance, scene_route_args_view_t args) {
    game_login_confirm_scene_t *scene = instance;
    (void)args;
    scene->visible = true;
}

static void on_hide(void *instance) {
    game_login_confirm_scene_t *scene = instance;
    scene->visible = false;
}

static void on_ui(void *instance, void *ui_context, scene_ui_mode_t mode) {
    (void)instance;
    login_prompt_draw_confirm(ui_context, mode == SCENE_UI_INTERACTIVE);
}

game_login_confirm_scene_t g_game_login_confirm_scene;

const scene_api_t g_game_login_confirm_scene_api = {
    .load_step = load_step,
    .on_show = on_show,
    .on_hide = on_hide,
    .on_ui = on_ui,
};
