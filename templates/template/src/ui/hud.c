#include "ui/hud.h"

#include "math/nt_math.h"
#include "render/nt_render_defs.h"
#include "renderers/nt_text_renderer.h"
#include "ui/loc_widgets.h"
#include "window/nt_window.h"

#include "font/nt_font.h"

#include "loc_strings.gen.h"

#include <string.h>

/* This HUD draws straight to the framebuffer, so its sizes are CSS pixels
   scaled by the device ratio -- the same physical size in a hand and on a
   monitor. Anything laid out by nt_ui goes through ui_metrics instead. */
#define HUD_MARGIN_CSS 20.0F
#define HUD_TITLE_CSS 30.0F
#define HUD_HINT_CSS 16.0F
#define HUD_TITLE_Y_CSS 56.0F
#define HUD_HINT_Y_CSS 28.0F

static void hud_text(LocStr text, float x, float y, float size, const float color[4]) {
    float model[16];
    glm_mat4_identity((vec4 *)model);
    glm_translate((vec4 *)model, (vec3){x, y, 0.0F});
    loc_text_draw(text, model, size, color, 0.0F, 0.0F);
}

/* A dark offset copy under the line keeps it readable over whatever the world
   draws behind it, and the shadow tracks the caller's alpha so a fading line
   does not end its life as a dark ghost of itself. */
static void hud_text_shadowed(LocStr text, float x, float y, float size, const float color[4]) {
    const float shadow[4] = {0.07F, 0.10F, 0.13F, 0.55F * color[3]};
    hud_text(text, x + size * 0.07F, y - size * 0.07F, size, shadow);
    hud_text(text, x, y, size, color);
}

/* Localized lines vary wildly in width, and a narrow portrait window is the
   case where the difference stops fitting: measure, then shrink rather than
   run off the edge. */
static void hud_text_fitted(nt_font_t font, LocStr text, float x, float y, float size, const float color[4]) {
    const float max_w = (float)g_nt_window.fb_width - x * 2.0F;
    const float w = nt_font_measure(font, text.s, size, 0.0F).width;
    if (w > max_w && w > 1.0F) {
        size *= max_w / w;
    }
    hud_text_shadowed(text, x, y, size, color);
}

void hud_draw(nt_material_t text_material, nt_resource_t font_resource, nt_font_t font, nt_buffer_t frame_ubo) {
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
    nt_gfx_update_buffer(frame_ubo, 0, &u, sizeof(u));
    nt_gfx_bind_uniform_buffer(frame_ubo, 0);

    nt_font_step();
    nt_text_renderer_set_material(text_material);
    nt_text_renderer_set_font(font);

    const float white[4] = {1.0F, 1.0F, 1.0F, 1.0F};
    const float cyan[4] = {0.35F, 0.82F, 1.0F, 1.0F};
    /* This title must not sit top-left: the resource panel owns that corner
       with the demo gold/hero HUD, and the two overlapped ("Gold 50" read
       through "TEMPLATE"'s letters). Both demo labels sit BOTTOM-left instead.
       This is game-owned demo scaffolding, not a feature; the generic
       resource_panel widget keeps its own top-left placement. */
    const float dpr = g_nt_window.dpr > 0.0F ? g_nt_window.dpr : 1.0F;
    const float x = HUD_MARGIN_CSS * dpr;
    hud_text_fitted(font, loc_hud_title(), x, HUD_TITLE_Y_CSS * dpr, HUD_TITLE_CSS * dpr, white);
    hud_text_fitted(font, loc_hud_hint(), x, HUD_HINT_Y_CSS * dpr, HUD_HINT_CSS * dpr, cyan);
    nt_text_renderer_flush();
}
