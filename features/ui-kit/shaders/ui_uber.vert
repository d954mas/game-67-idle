precision highp float;
precision highp int;

// The kit's uber sprite shader: plain atlas sprites and the SDF shapes of
// ui_shape.h in one material, so both batch into one draw. Globals at slot 0,
// as sprite.vert.
#include "common/globals.glsl"

layout(location = 0) in vec3 a_position;
layout(location = 2) in vec4 a_color;
layout(location = 3) in vec2 a_texcoord;
// Walker-injected {w/h, w, h, 0} in layout units.
layout(location = 4) in vec4 a_layout;
// ui_shape_pack.h packs these three; all zero is a plain sprite.
layout(location = 5) in vec4 a_shape;
layout(location = 6) in vec4 a_paint;
layout(location = 7) in vec4 a_fx;

out vec2 v_texcoord;
out vec4 v_color;
out vec2 v_uv;
flat out vec4 v_layout;
flat out vec4 v_shape;
flat out vec4 v_paint;
flat out vec4 v_fx;

void main() {
    gl_Position = view_proj * vec4(a_position, 1.0);
    v_texcoord = a_texcoord;
    v_color = a_color;
    // A shape is a GEOMETRY-mode quad the walker aligns to a multiple of four
    // vertices, emitted TL, TR, BR, BL: the corner gives the quad-local 0..1
    // coordinate, y down. Plain sprites never read it.
    int corner = gl_VertexID & 3;
    v_uv = vec2((corner == 1 || corner == 2) ? 1.0 : 0.0, (corner >= 2) ? 1.0 : 0.0);
    v_layout = a_layout;
    v_shape = a_shape;
    v_paint = a_paint;
    v_fx = a_fx;
}
