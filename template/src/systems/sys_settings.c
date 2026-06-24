#include "systems/sys_settings.h"

#include "input/nt_input.h"
#include "math/nt_math.h"
#include "render/nt_render_defs.h"
#include "renderers/nt_text_renderer.h"
#include "window/nt_window.h"

#include <stdio.h>
#include <string.h>

#define RESET_HOLD_SECONDS 1.5F

static bool s_open;
static float s_master = 0.8F, s_music = 0.7F, s_sfx = 0.9F;
static int s_drag = -1;
static float s_reset_hold;

float sys_settings_master(void) { return s_master; }
float sys_settings_music(void) { return s_music; }
float sys_settings_sfx(void) { return s_sfx; }

typedef struct {
    float x, y, w, h;
} rect_t;

// gear (top-right) + panel layout, in TEXT coords (y up, 0 = bottom)
static rect_t gear_rect(void) {
    const float w = (float)g_nt_window.fb_width;
    const float h = (float)g_nt_window.fb_height;
    return (rect_t){w - 150.0F, h - 46.0F, 132.0F, 34.0F};
}
static rect_t panel_rect(void) {
    const float cx = (float)g_nt_window.fb_width * 0.5F;
    const float cy = (float)g_nt_window.fb_height * 0.5F;
    return (rect_t){cx - 210.0F, cy - 150.0F, 420.0F, 300.0F};
}
static rect_t slider_bar(int i) {
    const rect_t p = panel_rect();
    const float y = p.y + p.h - 90.0F - (float)i * 44.0F;
    return (rect_t){p.x + 150.0F, y, 220.0F, 20.0F};
}
static rect_t close_rect(void) {
    const rect_t p = panel_rect();
    return (rect_t){p.x + p.w - 96.0F, p.y + p.h - 40.0F, 84.0F, 28.0F};
}
static rect_t reset_rect(void) {
    const rect_t p = panel_rect();
    return (rect_t){p.x + 24.0F, p.y + 24.0F, p.w - 48.0F, 36.0F};
}

static bool hit(rect_t r, float mx, float my) {
    return mx >= r.x && mx <= r.x + r.w && my >= r.y && my <= r.y + r.h;
}
static float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

void sys_settings_update(World *w, float dt) {
    const float mx = g_nt_input.pointers[0].x;
    const float my = (float)g_nt_window.fb_height - g_nt_input.pointers[0].y; // to text coords
    const bool pressed = nt_input_mouse_is_pressed(NT_BUTTON_LEFT);
    const bool down = nt_input_mouse_is_down(NT_BUTTON_LEFT);

    if (pressed && hit(gear_rect(), mx, my)) {
        s_open = !s_open;
        return;
    }
    if (!s_open) {
        return;
    }

    if (pressed && hit(close_rect(), mx, my)) {
        s_open = false;
        return;
    }

    // sliders (master/music/sfx)
    float *vals[3] = {&s_master, &s_music, &s_sfx};
    if (pressed) {
        s_drag = -1;
        for (int i = 0; i < 3; ++i) {
            if (hit(slider_bar(i), mx, my)) { s_drag = i; }
        }
    }
    if (!down) { s_drag = -1; }
    if (s_drag >= 0 && down) {
        const rect_t b = slider_bar(s_drag);
        *vals[s_drag] = clampf((mx - b.x) / b.w, 0.0F, 1.0F);
    }

    // reset: hold to confirm
    if (down && s_drag < 0 && hit(reset_rect(), mx, my)) {
        s_reset_hold += dt;
        if (s_reset_hold >= RESET_HOLD_SECONDS) {
            w->player_x = 0.0F;
            w->player_z = 0.0F;
            w->player_yaw = 0.0F;
            s_reset_hold = 0.0F;
            s_open = false;
        }
    } else {
        s_reset_hold = 0.0F;
    }
}

static void txt(const char *s, float x, float y, float size, const float color[4]) {
    float model[16];
    glm_mat4_identity((vec4 *)model);
    glm_translate((vec4 *)model, (vec3){x, y, 0.0F});
    nt_text_renderer_draw(s, model, size, color, 0.0F, 0.0F);
}

static void bar_str(char *out, size_t cap, float v) {
    const int cells = 18;
    const int filled = (int)(v * (float)cells + 0.5F);
    size_t k = 0;
    out[k++] = '[';
    for (int i = 0; i < cells && k < cap - 4; ++i) { out[k++] = (i < filled) ? '#' : '-'; }
    out[k++] = ']';
    out[k] = '\0';
}

void sys_settings_draw(nt_material_t text_material, nt_resource_t font_resource, nt_font_t font, nt_buffer_t frame_ubo) {
    if (!nt_material_get_info(text_material) || !nt_resource_is_ready(font_resource)) {
        return;
    }
    float view[16];
    float proj[16];
    glm_mat4_identity((vec4 *)view);
    glm_ortho(0.0F, (float)g_nt_window.fb_width, 0.0F, (float)g_nt_window.fb_height, -1.0F, 1.0F, (vec4 *)proj);
    nt_frame_uniforms_t u;
    memset(&u, 0, sizeof(u));
    float vp[16];
    glm_mat4_mul((vec4 *)proj, (vec4 *)view, (vec4 *)vp);
    memcpy(u.view_proj, vp, 64);
    memcpy(u.view, view, 64);
    memcpy(u.proj, proj, 64);
    u.camera_pos[2] = 1.0F;
    nt_gfx_update_buffer(frame_ubo, &u, sizeof(u));
    nt_gfx_bind_uniform_buffer(frame_ubo, 0);
    nt_font_step();
    nt_text_renderer_set_material(text_material);
    nt_text_renderer_set_font(font);

    const float white[4] = {1.0F, 1.0F, 1.0F, 1.0F};
    const float dim[4] = {0.75F, 0.80F, 0.90F, 1.0F};
    const float yellow[4] = {1.0F, 0.86F, 0.25F, 1.0F};
    const float red[4] = {1.0F, 0.35F, 0.30F, 1.0F};

    rect_t g = gear_rect();
    txt("[ SETTINGS ]", g.x, g.y + 6.0F, 20.0F, white);

    if (!s_open) {
        nt_text_renderer_flush();
        return;
    }

    rect_t p = panel_rect();
    txt("SETTINGS", p.x + 24.0F, p.y + p.h - 44.0F, 28.0F, yellow);
    txt("[ CLOSE ]", close_rect().x, close_rect().y + 4.0F, 18.0F, dim);

    const char *labels[3] = {"MASTER", "MUSIC", "SFX"};
    float vals[3] = {s_master, s_music, s_sfx};
    char line[64];
    for (int i = 0; i < 3; ++i) {
        rect_t b = slider_bar(i);
        txt(labels[i], p.x + 24.0F, b.y + 2.0F, 18.0F, white);
        char bar[40];
        bar_str(bar, sizeof(bar), vals[i]);
        (void)snprintf(line, sizeof(line), "%s %d%%", bar, (int)(vals[i] * 100.0F + 0.5F));
        txt(line, b.x, b.y + 2.0F, 18.0F, dim);
    }

    rect_t r = reset_rect();
    char hold[48];
    bar_str(hold, sizeof(hold), s_reset_hold / RESET_HOLD_SECONDS);
    (void)snprintf(line, sizeof(line), "HOLD TO RESET %s", hold);
    txt(line, r.x, r.y + 8.0F, 18.0F, s_reset_hold > 0.0F ? red : dim);

    nt_text_renderer_flush();
}
