#ifndef RB_DARK_RPG_SCENE_LAYOUT_H
#define RB_DARK_RPG_SCENE_LAYOUT_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int x;
    int y;
    int w;
    int h;
} scene_rect_t;

typedef struct {
    int framebuffer_w;
    int framebuffer_h;
    float scale;
    float view_x;
    float view_y;
    float view_w;
    float view_h;
} scene_view_t;

typedef struct {
    float x;
    float y;
} scene_point_t;

enum {
    SCENE_LAYOUT_MASTER_W = 1280,
    SCENE_LAYOUT_MASTER_H = 700,
    SCENE_LAYOUT_SCREEN_W = 960,
    SCENE_LAYOUT_SCREEN_H = 540,
    SCENE_LAYOUT_SCREEN_X = 160,
    SCENE_LAYOUT_SCREEN_Y = 80,
    SCENE_LAYOUT_BLEED_LR = 160,
    SCENE_LAYOUT_BLEED_TB = 80,
    SCENE_LAYOUT_TOP_UI_H = 72,
    SCENE_LAYOUT_BOTTOM_UI_H = 112,
    SCENE_LAYOUT_POKI_W = 112,
    SCENE_LAYOUT_POKI_H = 72
};

scene_rect_t scene_layout_master_rect(void);
scene_rect_t scene_layout_screen_rect(void);
scene_rect_t scene_layout_top_ui_rect(void);
scene_rect_t scene_layout_bottom_ui_rect(void);
scene_rect_t scene_layout_play_rect(void);
scene_rect_t scene_layout_poki_reserve_rect(void);

float scene_layout_compute_scale(int framebuffer_w, int framebuffer_h);
float scene_layout_default_center_x(void);
float scene_layout_default_center_y(void);
scene_view_t scene_layout_compute_view(int framebuffer_w, int framebuffer_h,
                                       float requested_center_x, float requested_center_y);
scene_point_t scene_layout_pointer_to_master(scene_view_t view, float pointer_x, float pointer_y);

#ifdef __cplusplus
}
#endif

#endif /* RB_DARK_RPG_SCENE_LAYOUT_H */
