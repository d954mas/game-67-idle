precision highp float;
precision highp int;

#include "common/globals.glsl"

layout(location = 0) in vec3 a_position;
layout(location = 2) in vec4 a_color;
layout(location = 3) in vec2 a_texcoord;
layout(location = 4) in vec4 a_fade;

out vec2 v_texcoord;
out vec4 v_color;
out vec4 v_fade;
out float v_local_y;

void main() {
    gl_Position = view_proj * vec4(a_position, 1.0);
    v_texcoord = a_texcoord;
    v_color = a_color;
    v_fade = a_fade;
    v_local_y = a_position.y;
}
