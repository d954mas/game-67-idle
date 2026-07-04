#include "scene/scene_layout.h"

static float scene_layout_clampf(float v, float lo, float hi) {
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

scene_rect_t scene_layout_master_rect(void) {
    return (scene_rect_t){0, 0, SCENE_LAYOUT_MASTER_W, SCENE_LAYOUT_MASTER_H};
}

scene_rect_t scene_layout_screen_rect(void) {
    return (scene_rect_t){SCENE_LAYOUT_SCREEN_X, SCENE_LAYOUT_SCREEN_Y, SCENE_LAYOUT_SCREEN_W, SCENE_LAYOUT_SCREEN_H};
}

scene_rect_t scene_layout_top_ui_rect(void) {
    return (scene_rect_t){SCENE_LAYOUT_SCREEN_X, SCENE_LAYOUT_SCREEN_Y + SCENE_LAYOUT_SCREEN_H - SCENE_LAYOUT_TOP_UI_H,
                          SCENE_LAYOUT_SCREEN_W, SCENE_LAYOUT_TOP_UI_H};
}

scene_rect_t scene_layout_bottom_ui_rect(void) {
    return (scene_rect_t){SCENE_LAYOUT_SCREEN_X, SCENE_LAYOUT_SCREEN_Y,
                          SCENE_LAYOUT_SCREEN_W, SCENE_LAYOUT_BOTTOM_UI_H};
}

scene_rect_t scene_layout_play_rect(void) {
    return (scene_rect_t){SCENE_LAYOUT_SCREEN_X, SCENE_LAYOUT_SCREEN_Y + SCENE_LAYOUT_BOTTOM_UI_H,
                          SCENE_LAYOUT_SCREEN_W, SCENE_LAYOUT_SCREEN_H - SCENE_LAYOUT_TOP_UI_H - SCENE_LAYOUT_BOTTOM_UI_H};
}

scene_rect_t scene_layout_poki_reserve_rect(void) {
    return (scene_rect_t){SCENE_LAYOUT_SCREEN_X, SCENE_LAYOUT_SCREEN_Y + SCENE_LAYOUT_SCREEN_H - SCENE_LAYOUT_POKI_H,
                          SCENE_LAYOUT_POKI_W, SCENE_LAYOUT_POKI_H};
}

float scene_layout_default_center_x(void) {
    return (float)SCENE_LAYOUT_SCREEN_X + ((float)SCENE_LAYOUT_SCREEN_W * 0.5f);
}

float scene_layout_default_center_y(void) {
    return (float)SCENE_LAYOUT_SCREEN_Y + ((float)SCENE_LAYOUT_SCREEN_H * 0.5f);
}

static void scene_layout_view_size_for_aspect(float aspect, float *out_w, float *out_h) {
    const float screen_w = (float)SCENE_LAYOUT_SCREEN_W;
    const float screen_h = (float)SCENE_LAYOUT_SCREEN_H;
    const float master_h = (float)SCENE_LAYOUT_MASTER_H;
    const float min_aspect_for_screen_width = screen_w / master_h;

    if (aspect <= min_aspect_for_screen_width) {
        *out_h = master_h;
        *out_w = master_h * aspect;
        return;
    }

    *out_w = screen_w;
    *out_h = screen_w / aspect;
    if (*out_h < screen_h) {
        *out_h = screen_h;
        *out_w = screen_h * aspect;
    }
    if (*out_h > master_h) {
        *out_h = master_h;
        *out_w = master_h * aspect;
    }
}

float scene_layout_compute_scale(int framebuffer_w, int framebuffer_h) {
    if (framebuffer_w <= 0 || framebuffer_h <= 0) {
        return 0.0f;
    }
    const float aspect = (float)framebuffer_w / (float)framebuffer_h;
    float view_w = 0.0f;
    float view_h = 0.0f;
    scene_layout_view_size_for_aspect(aspect, &view_w, &view_h);
    (void)view_w;
    return (float)framebuffer_h / view_h;
}

scene_view_t scene_layout_compute_view(int framebuffer_w, int framebuffer_h,
                                       float requested_center_x, float requested_center_y) {
    scene_view_t view = {
        .framebuffer_w = framebuffer_w,
        .framebuffer_h = framebuffer_h,
        .scale = scene_layout_compute_scale(framebuffer_w, framebuffer_h),
        .view_x = 0.0f,
        .view_y = 0.0f,
        .view_w = 0.0f,
        .view_h = 0.0f,
    };

    if (view.scale <= 0.0f) {
        return view;
    }

    const float aspect = (float)framebuffer_w / (float)framebuffer_h;
    scene_layout_view_size_for_aspect(aspect, &view.view_w, &view.view_h);

    if (view.view_w >= (float)SCENE_LAYOUT_MASTER_W) {
        view.view_x = ((float)SCENE_LAYOUT_MASTER_W - view.view_w) * 0.5f;
    } else {
        const float half_w = view.view_w * 0.5f;
        const float min_center_x = half_w;
        const float max_center_x = (float)SCENE_LAYOUT_MASTER_W - half_w;
        const float center_x = scene_layout_clampf(requested_center_x, min_center_x, max_center_x);
        view.view_x = center_x - half_w;
    }

    if (view.view_h >= (float)SCENE_LAYOUT_MASTER_H) {
        view.view_y = ((float)SCENE_LAYOUT_MASTER_H - view.view_h) * 0.5f;
    } else {
        const float half_h = view.view_h * 0.5f;
        const float min_center_y = half_h;
        const float max_center_y = (float)SCENE_LAYOUT_MASTER_H - half_h;
        const float center_y = scene_layout_clampf(requested_center_y, min_center_y, max_center_y);
        view.view_y = center_y - half_h;
    }

    return view;
}

scene_point_t scene_layout_pointer_to_master(scene_view_t view, float pointer_x, float pointer_y) {
    if (view.scale <= 0.0f) {
        return (scene_point_t){0.0f, 0.0f};
    }
    return (scene_point_t){
        .x = view.view_x + (pointer_x / view.scale),
        .y = view.view_y + (((float)view.framebuffer_h - pointer_y) / view.scale),
    };
}

scene_point_t scene_layout_master_to_screen(scene_view_t view, float master_x, float master_y) {
    if (view.scale <= 0.0f) {
        return (scene_point_t){0.0f, 0.0f};
    }
    return (scene_point_t){
        .x = (master_x - view.view_x) * view.scale,
        .y = (float)view.framebuffer_h - ((master_y - view.view_y) * view.scale),
    };
}
