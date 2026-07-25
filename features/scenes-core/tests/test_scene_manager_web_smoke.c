#include "features/scenes/scene_manager.h"

#include <stdbool.h>

typedef struct smoke_scene {
    int shows;
    int hides;
    int unloads;
    int updates;
} smoke_scene_t;

static scene_load_result_t ready(void *instance) {
    (void)instance;
    return SCENE_LOAD_READY;
}

static void shown(void *instance, scene_route_args_view_t args) {
    smoke_scene_t *scene = instance;
    (void)args;
    ++scene->shows;
}

static void hidden(void *instance) {
    smoke_scene_t *scene = instance;
    ++scene->hides;
}

static void unloaded(void *instance) {
    smoke_scene_t *scene = instance;
    ++scene->unloads;
}

static void updated(void *instance, float dt) {
    smoke_scene_t *scene = instance;
    (void)dt;
    ++scene->updates;
}

static const scene_api_t k_api = {
    .load_step = ready,
    .unload = unloaded,
    .on_show = shown,
    .on_hide = hidden,
    .on_update = updated,
};

int main(void) {
    smoke_scene_t screen = {0};
    smoke_scene_t modal = {0};
    const scene_descriptor_t catalog[] = {
        {
            .id = "root",
            .kind = SCENE_KIND_SCREEN,
            .keep_loaded = true,
            .instance = &screen,
            .api = &k_api,
        },
        {
            .id = "modal",
            .kind = SCENE_KIND_MODAL,
            .modal_update_policy = SCENE_MODAL_CONTINUE_BELOW,
            .instance = &modal,
            .api = &k_api,
        },
    };
    scene_manager_t manager;

    scene_manager_init(&manager, catalog, 2);
    if (scene_manager_start(
            &manager, "root", (scene_route_args_view_t){0},
            NULL, NULL) != SCENE_RESULT_ACCEPTED) {
        return 1;
    }
    scene_manager_step(&manager, 1, 0.016F);
    scene_manager_update(&manager, 0.016F);
    if (!scene_manager_is_presented(&manager, &catalog[0]) ||
        screen.shows != 1 || screen.updates != 1) {
        return 2;
    }

    if (scene_manager_show(
            &manager, "modal", (scene_route_args_view_t){0},
            NULL, NULL) != SCENE_RESULT_ACCEPTED) {
        return 3;
    }
    scene_manager_step(&manager, 2, 0.016F);
    scene_manager_update(&manager, 0.016F);
    if (!scene_manager_is_presented(&manager, &catalog[0]) ||
        !scene_manager_is_presented(&manager, &catalog[1]) ||
        screen.updates != 2 || modal.updates != 1) {
        return 4;
    }

    if (scene_manager_reload(&manager, NULL, NULL) !=
        SCENE_RESULT_ACCEPTED) {
        return 5;
    }
    scene_manager_step(&manager, 3, 0.016F);
    scene_manager_update(&manager, 0.016F);
    if (scene_manager_is_presented(&manager, &catalog[1]) ||
        screen.updates != 3 || modal.updates != 1) {
        return 6;
    }
    scene_manager_step(&manager, 4, 0.016F);
    if (!scene_manager_is_presented(&manager, &catalog[1]) ||
        modal.shows != 2) {
        return 7;
    }

    scene_manager_shutdown(&manager);
    if (screen.hides != 1 || screen.unloads != 1 ||
        modal.hides != 2 || modal.unloads != 2) {
        return 8;
    }
    return 0;
}
