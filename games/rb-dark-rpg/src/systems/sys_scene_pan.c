#include "systems/sys_scene_pan.h"

#include "input/nt_input.h"
#include "scene/scene_interactions.h"
#include "scene/scene_layout.h"
#include "ui/ui_runtime.h"
#include "window/nt_window.h"

static bool s_dragging;

static void ensure_camera(World *w) {
    if (w->first_scene.camera_initialized) {
        return;
    }
    w->first_scene.camera_center_x = scene_layout_default_center_x();
    w->first_scene.camera_center_y = scene_layout_default_center_y();
    w->first_scene.camera_initialized = true;
}

static bool contains(scene_rect_t r, float x, float y) {
    return x >= (float)r.x && x <= (float)(r.x + r.w) && y >= (float)r.y && y <= (float)(r.y + r.h);
}

static float clamped_center_x(scene_view_t view) {
    return view.view_x + view.view_w * 0.5F;
}

void sys_scene_pan_update(World *w) {
    ensure_camera(w);
    if (ui_runtime_blocks_world_input()) {
        s_dragging = false;
        return;
    }

    const int fb_w = (int)g_nt_window.fb_width;
    const int fb_h = (int)g_nt_window.fb_height;
    scene_view_t view = scene_layout_compute_view(fb_w, fb_h, w->first_scene.camera_center_x, w->first_scene.camera_center_y);
    if (view.scale <= 0.0F) {
        return;
    }

    const nt_pointer_t *p = &g_nt_input.pointers[0];
    if (p->buttons[NT_BUTTON_LEFT].is_released) {
        s_dragging = false;
    }

    if (p->buttons[NT_BUTTON_LEFT].is_pressed) {
        if (scene_interactions_pointer_captures_pan(w)) {
            s_dragging = false;
            return;
        }
        const scene_point_t master = scene_layout_pointer_to_master(view, p->x, p->y);
        const bool started_on_play = contains(scene_layout_play_rect(), master.x, master.y);
        s_dragging = started_on_play && view.view_w < (float)SCENE_LAYOUT_MASTER_W;
    }

    if (scene_interactions_pointer_captures_pan(w)) {
        s_dragging = false;
        return;
    }

    if (!s_dragging || !p->buttons[NT_BUTTON_LEFT].is_down) {
        return;
    }

    const float next_center_x = w->first_scene.camera_center_x - (p->dx / view.scale);
    view = scene_layout_compute_view(fb_w, fb_h, next_center_x, w->first_scene.camera_center_y);
    w->first_scene.camera_center_x = clamped_center_x(view);
}
