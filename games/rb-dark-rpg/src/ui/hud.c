#include "ui/hud.h"

#include "math/nt_math.h"
#include "render/nt_render_defs.h"
#include "renderers/nt_text_renderer.h"
#include "window/nt_window.h"

#include <string.h>

static void hud_text(const char *text, float x, float y, float size, const float color[4]) {
    float model[16];
    glm_mat4_identity((vec4 *)model);
    glm_translate((vec4 *)model, (vec3){x, y, 0.0F});
    nt_text_renderer_draw(text, model, size, color, 0.0F, 0.0F);
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
    nt_gfx_update_buffer(frame_ubo, &u, sizeof(u));
    nt_gfx_bind_uniform_buffer(frame_ubo, 0);

    nt_font_step();
    nt_text_renderer_set_material(text_material);
    nt_text_renderer_set_font(font);

    const float white[4] = {1.0F, 1.0F, 1.0F, 1.0F};
    const float cyan[4] = {0.35F, 0.82F, 1.0F, 1.0F};
    const float h = (float)g_nt_window.fb_height;
    hud_text("RB DARK RPG", 30.0F, h - 50.0F, 30.0F, white);
    hud_text("starter slice - gear (top-right) opens settings", 30.0F, 40.0F, 18.0F, cyan);
    nt_text_renderer_flush();
}
