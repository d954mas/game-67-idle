#include "scene/scene_layout.h"

#include <assert.h>
#include <math.h>

static void test_rects(void) {
    const scene_rect_t master = scene_layout_master_rect();
    assert(master.x == 0 && master.y == 0 && master.w == 1280 && master.h == 700);

    const scene_rect_t screen = scene_layout_screen_rect();
    assert(screen.x == 160 && screen.y == 80 && screen.w == 960 && screen.h == 540);

    const scene_rect_t top = scene_layout_top_ui_rect();
    assert(top.x == 160 && top.y == 548 && top.w == 960 && top.h == 72);

    const scene_rect_t bottom = scene_layout_bottom_ui_rect();
    assert(bottom.x == 160 && bottom.y == 80 && bottom.w == 960 && bottom.h == 112);

    const scene_rect_t play = scene_layout_play_rect();
    assert(play.x == 160 && play.y == 192 && play.w == 960 && play.h == 356);

    const scene_rect_t poki = scene_layout_poki_reserve_rect();
    assert(poki.x == 160 && poki.y == 548 && poki.w == 112 && poki.h == 72);
}

static void test_scale(void) {
    assert(fabsf(scene_layout_compute_scale(960, 540) - 1.0f) < 0.0001f);
    assert(fabsf(scene_layout_compute_scale(1920, 1080) - 2.0f) < 0.0001f);
    assert(scene_layout_compute_scale(0, 1080) == 0.0f);
}

static void test_view_landscape(void) {
    const scene_view_t view = scene_layout_compute_view(960, 540, scene_layout_default_center_x(), scene_layout_default_center_y());
    assert(fabsf(view.scale - 1.0f) < 0.0001f);
    assert(fabsf(view.view_w - 960.0f) < 0.0001f);
    assert(fabsf(view.view_h - 540.0f) < 0.0001f);
    assert(fabsf(view.view_x - 160.0f) < 0.0001f);
    assert(fabsf(view.view_y - 80.0f) < 0.0001f);
}

static void test_view_ultrawide_reveals_bleed(void) {
    const scene_view_t view = scene_layout_compute_view(2520, 1080, 640.0f, 350.0f);
    assert(fabsf(view.scale - 2.0f) < 0.0001f);
    assert(view.view_w > 960.0f);
    assert(view.view_w < 1280.0f);
    assert(view.view_x < 160.0f);
    assert(view.view_x > 0.0f);
    assert(fabsf(view.view_h - 540.0f) < 0.0001f);

    const scene_view_t extreme = scene_layout_compute_view(2800, 1080, 640.0f, 350.0f);
    assert(extreme.view_w > 1280.0f);
    assert(extreme.view_x < 0.0f);
}

static void test_view_portrait_clamps_center(void) {
    const scene_view_t view = scene_layout_compute_view(540, 960, 640.0f, 350.0f);
    assert(fabsf(view.scale - (960.0f / 700.0f)) < 0.0001f);
    assert(fabsf(view.view_h - 700.0f) < 0.0001f);
    assert(view.view_w < 1280.0f);
    assert(view.view_x >= 0.0f);
    assert(view.view_x + view.view_w <= 1280.0f + 0.0001f);
}

static void test_view_portrait_pan_clamps_to_master(void) {
    const scene_view_t left = scene_layout_compute_view(540, 960, -999.0f, 350.0f);
    const scene_view_t right = scene_layout_compute_view(540, 960, 9999.0f, 350.0f);
    assert(left.view_x >= 0.0f);
    assert(right.view_x + right.view_w <= 1280.0f + 0.0001f);
}

static void test_pointer_to_master_is_y_up(void) {
    const scene_view_t view = scene_layout_compute_view(960, 540, scene_layout_default_center_x(), scene_layout_default_center_y());
    const scene_point_t top_left = scene_layout_pointer_to_master(view, 0.0f, 0.0f);
    const scene_point_t bottom_right = scene_layout_pointer_to_master(view, 960.0f, 540.0f);
    assert(fabsf(top_left.x - 160.0f) < 0.0001f);
    assert(fabsf(top_left.y - 620.0f) < 0.0001f);
    assert(fabsf(bottom_right.x - 1120.0f) < 0.0001f);
    assert(fabsf(bottom_right.y - 80.0f) < 0.0001f);
}

static void test_master_to_screen_tracks_portrait_pan(void) {
    const scene_view_t center = scene_layout_compute_view(390, 844, scene_layout_default_center_x(), scene_layout_default_center_y());
    const scene_view_t right = scene_layout_compute_view(390, 844, scene_layout_default_center_x() + 120.0f, scene_layout_default_center_y());
    const scene_point_t center_screen = scene_layout_master_to_screen(center, 656.0f, 346.0f);
    const scene_point_t right_screen = scene_layout_master_to_screen(right, 656.0f, 346.0f);
    assert(center_screen.x > right_screen.x + 40.0f);
    assert(fabsf(center_screen.y - right_screen.y) < 0.0001f);

    const scene_point_t roundtrip = scene_layout_pointer_to_master(center, center_screen.x, center_screen.y);
    assert(fabsf(roundtrip.x - 656.0f) < 0.0001f);
    assert(fabsf(roundtrip.y - 346.0f) < 0.0001f);
}

int main(void) {
    test_rects();
    test_scale();
    test_view_landscape();
    test_view_ultrawide_reveals_bleed();
    test_view_portrait_clamps_center();
    test_view_portrait_pan_clamps_to_master();
    test_pointer_to_master_is_y_up();
    test_master_to_screen_tracks_portrait_pan();
    return 0;
}
