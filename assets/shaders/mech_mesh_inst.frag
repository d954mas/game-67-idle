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
    float specular = pow(max(dot(normal, half_dir), 0.0), 44.0);
    float broad_spec = pow(max(dot(normal, half_dir), 0.0), 10.0);
    float rim = pow(1.0 - max(dot(normal, view_dir), 0.0), 1.65);
    float bright_color = max(max(v_color.r, v_color.g), v_color.b);
    float chroma = bright_color - min(min(v_color.r, v_color.g), v_color.b);
    float emissive = smoothstep(0.55, 0.95, bright_color) * smoothstep(0.18, 0.75, chroma);
    vec3 base = pow(v_color.rgb, vec3(0.92));
    vec3 lit = base * (0.18 + diffuse * 1.18 + fill * 0.20);
    lit += vec3(1.0, 0.78, 0.42) * specular * 0.42;
    lit += vec3(0.50, 0.82, 1.0) * broad_spec * 0.16;
    lit += vec3(0.02, 0.78, 1.0) * rim * 0.42;
    lit += base * emissive * 0.48;
    lit = clamp(lit, 0.0, 1.18);
    lit = pow(lit, vec3(0.88));
    frag_color = vec4(lit, v_color.a);
}
