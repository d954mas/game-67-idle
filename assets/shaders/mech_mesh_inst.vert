precision highp float;

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_uv0;

#include "common/instance.glsl"
#include "common/globals.glsl"

out vec3 v_normal;
out vec3 v_world_pos;
out vec2 v_uv0;
out vec4 v_color;

void main() {
    mat4 world = mat4(
        vec4(a_world_row0.x, a_world_row1.x, a_world_row2.x, 0.0),
        vec4(a_world_row0.y, a_world_row1.y, a_world_row2.y, 0.0),
        vec4(a_world_row0.z, a_world_row1.z, a_world_row2.z, 0.0),
        vec4(a_world_row0.w, a_world_row1.w, a_world_row2.w, 1.0)
    );
    vec4 world_pos = world * vec4(a_position, 1.0);
    v_world_pos = world_pos.xyz;
    v_normal = normalize(mat3(world) * a_normal);
    v_uv0 = a_uv0;
    v_color = a_color;
    gl_Position = view_proj * world_pos;
}
