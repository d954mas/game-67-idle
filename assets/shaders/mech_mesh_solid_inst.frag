precision mediump float;

#include "common/globals.glsl"

in vec3 v_normal;
in vec3 v_world_pos;
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
    float specular = pow(max(dot(normal, half_dir), 0.0), 22.0);
    float rim = pow(1.0 - max(dot(normal, view_dir), 0.0), 1.6);
    vec3 base = pow(clamp(v_color.rgb, 0.0, 1.0), vec3(0.72));
    vec3 lit = base * (0.58 + diffuse * 0.62 + fill * 0.24);
    lit += vec3(1.0, 0.96, 0.80) * specular * 0.25;
    lit += vec3(0.72, 0.92, 1.0) * rim * 0.16;
    frag_color = vec4(clamp(pow(lit, vec3(0.92)), 0.0, 1.0), v_color.a);
}
