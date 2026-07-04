#include "systems/sys_scene_interactions.h"

#include "input/nt_input.h"
#include "scene/scene_interactions.h"
#include "scene/scene_layout.h"
#include "window/nt_window.h"

void sys_scene_interactions_update(World *w) {
    scene_interactions_init_first_scene(w);

    const int fb_w = (int)g_nt_window.fb_width;
    const int fb_h = (int)g_nt_window.fb_height;
    const float center_x = w->first_scene.camera_initialized ? w->first_scene.camera_center_x : scene_layout_default_center_x();
    const float center_y = w->first_scene.camera_initialized ? w->first_scene.camera_center_y : scene_layout_default_center_y();
    const scene_view_t view = scene_layout_compute_view(fb_w, fb_h, center_x, center_y);
    if (view.scale <= 0.0F) {
        scene_interactions_update_pointer_state(w, SCENE_OBJECT_ID_NONE, false, false, false);
        return;
    }

    const nt_pointer_t *p = &g_nt_input.pointers[0];
    const scene_point_t master = scene_layout_pointer_to_master(view, p->x, p->y);
    const scene_object_id_t hit = scene_interactions_hit_test(master.x, master.y);
    scene_interactions_update_pointer_state(w, hit,
                                            p->buttons[NT_BUTTON_LEFT].is_pressed,
                                            p->buttons[NT_BUTTON_LEFT].is_down,
                                            p->buttons[NT_BUTTON_LEFT].is_released);
}
