precision mediump float;

#include "common/globals.glsl"

in vec3 v_normal;
in vec3 v_world_pos;
in vec2 v_uv0;
in vec4 v_color;

out vec4 frag_color;

void main() {
    vec3 normal = normalize(v_normal);
    vec3 light_dir = normalize(vec3(-0.38, 0.88, 0.30));
    vec3 fill_dir = normalize(vec3(0.70, 0.32, -0.55));
    vec3 view_dir = normalize(camera_pos.xyz - v_world_pos);
    float diffuse = max(dot(normal, light_dir), 0.0);
    float fill = max(dot(normal, fill_dir), 0.0);
    vec3 half_dir = normalize(light_dir + view_dir);
    float specular = pow(max(dot(normal, half_dir), 0.0), 24.0);
    float broad_spec = pow(max(dot(normal, half_dir), 0.0), 7.0);
    float rim = pow(1.0 - max(dot(normal, view_dir), 0.0), 2.0);
    vec3 base = pow(clamp(v_color.rgb, 0.0, 1.0), vec3(0.78));
    vec3 lit = base * (0.48 + diffuse * 0.70 + fill * 0.18);
    lit += vec3(1.0, 0.96, 0.82) * specular * 0.20;
    lit += vec3(0.72, 0.88, 1.0) * broad_spec * 0.10;
    lit += vec3(1.0, 1.0, 1.0) * rim * 0.13;
    frag_color = vec4(clamp(pow(lit, vec3(0.92)), 0.0, 1.0), v_color.a);
}
