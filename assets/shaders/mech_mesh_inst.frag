precision mediump float;

#include "common/globals.glsl"

in vec3 v_normal;
in vec3 v_world_pos;
in vec4 v_color;

out vec4 frag_color;

void main() {
    vec3 normal = normalize(v_normal);
    vec3 light_dir = normalize(vec3(-0.45, 0.82, 0.35));
    vec3 view_dir = normalize(camera_pos.xyz - v_world_pos);
    float diffuse = max(dot(normal, light_dir), 0.0);
    float rim = pow(1.0 - max(dot(normal, view_dir), 0.0), 2.0);
    vec3 lit = v_color.rgb * (0.36 + diffuse * 0.84);
    lit += vec3(0.02, 0.75, 1.0) * rim * 0.28;
    frag_color = vec4(lit, v_color.a);
}
