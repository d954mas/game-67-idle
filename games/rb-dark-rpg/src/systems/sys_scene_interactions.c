#include "systems/sys_scene_interactions.h"

#include "game_state.h"
#include "input/nt_input.h"
#include "scene/scene_interactions.h"
#include "scene/scene_layout.h"
#include "ui/bottom_nav.h"
#include "ui/combat_flow.h"
#include "ui/ui_runtime.h"
#include "window/nt_window.h"

#include <string.h>

static bool first_scene_interactions_active(const World *w) {
    return !w || !w->player_state || w->player_state->world_current_location_id[0] == '\0' ||
           strcmp(w->player_state->world_current_location_id, "hub_last_post") == 0;
}

void sys_scene_interactions_update(World *w) {
    if (!w) {
        return;
    }
    scene_interactions_init_first_scene(w);

    if (ui_runtime_blocks_world_input() || !first_scene_interactions_active(w) || w->dialogue.open ||
        bottom_nav_sheet_open() || combat_flow_is_open(w)) {
        scene_interactions_update_pointer_state(w, NULL, false, false, false);
        return;
    }

    const int fb_w = (int)g_nt_window.fb_width;
    const int fb_h = (int)g_nt_window.fb_height;
    const float center_x = w->first_scene.camera_initialized ? w->first_scene.camera_center_x : scene_layout_default_center_x();
    const float center_y = w->first_scene.camera_initialized ? w->first_scene.camera_center_y : scene_layout_default_center_y();
    const scene_view_t view = scene_layout_compute_view(fb_w, fb_h, center_x, center_y);
    if (view.scale <= 0.0F) {
        scene_interactions_update_pointer_state(w, NULL, false, false, false);
        return;
    }

    const nt_pointer_t *p = &g_nt_input.pointers[0];
    const scene_point_t master = scene_layout_pointer_to_master(view, p->x, p->y);
    const char *hit = scene_interactions_hit_test(w, master.x, master.y);
    scene_interactions_update_pointer_state(w, hit,
                                            p->buttons[NT_BUTTON_LEFT].is_pressed,
                                            p->buttons[NT_BUTTON_LEFT].is_down,
                                            p->buttons[NT_BUTTON_LEFT].is_released);
}
