precision highp float;
precision highp int;

// UBO contract: this shader uses "Globals" (defined in common/globals.glsl)
// for view_proj. The engine binds that name to NT_GFX_SLOT_GLOBALS; the game binds
// its view block with nt_gfx_bind_uniform_block(NT_GFX_SLOT_GLOBALS, ...) in
// every pass before nt_sprite_renderer_draw_list; the renderer binds no uniform
// blocks itself.
#include "common/globals.glsl"

// Locations match NT_LOC_POSITION/COLOR/UV0 in the engine's nt_vertex_locations.h.
layout(location = 0) in vec3 a_position;
layout(location = 2) in vec4 a_color;
layout(location = 3) in vec2 a_texcoord;

out vec2 v_texcoord;
out vec4 v_color;

void main() {
    gl_Position = view_proj * vec4(a_position, 1.0);
    v_texcoord = a_texcoord;
    v_color = a_color;
}
