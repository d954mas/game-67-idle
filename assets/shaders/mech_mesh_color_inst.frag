precision mediump float;

#include "common/globals.glsl"

in vec3 v_normal;
in vec3 v_world_pos;
in vec2 v_uv0;
in vec4 v_color;

out vec4 frag_color;

void main() {
    vec3 normal = normalize(v_normal);
    vec3 light_dir = normalize(vec3(-0.44, 0.86, 0.26));
    vec3 fill_dir = normalize(vec3(0.72, 0.38, -0.54));
    vec3 top_dir = normalize(vec3(0.04, 0.98, 0.18));
    vec3 view_dir = normalize(camera_pos.xyz - v_world_pos);
    float diffuse_raw = max(dot(normal, light_dir), 0.0);
    float diffuse = mix(diffuse_raw, smoothstep(0.12, 0.96, diffuse_raw), 0.55);
    float fill = max(dot(normal, fill_dir), 0.0);
    float top = max(dot(normal, top_dir), 0.0);
    float ground_bounce = clamp(1.0 - normal.y, 0.0, 1.0) * 0.18;
    vec3 half_dir = normalize(light_dir + view_dir);
    float ndh = max(dot(normal, half_dir), 0.0);
    float specular = pow(ndh, 34.0);
    float broad_spec = pow(ndh, 7.0);
    float rim = pow(1.0 - max(dot(normal, view_dir), 0.0), 1.55);
    vec3 avg = vec3((v_color.r + v_color.g + v_color.b) / 3.0);
    vec3 toy_color = mix(avg, clamp(v_color.rgb, 0.0, 1.0), 1.20);
    vec3 base = pow(clamp(toy_color, 0.0, 1.0), vec3(0.70));
    vec3 lit = base * (0.42 + diffuse * 0.82 + fill * 0.22 + top * 0.16);
    lit += base * vec3(0.48, 0.88, 0.42) * ground_bounce;
    lit += vec3(1.0, 0.95, 0.76) * specular * 0.34;
    lit += vec3(0.68, 0.88, 1.0) * broad_spec * 0.13;
    lit += vec3(0.76, 0.94, 1.0) * rim * 0.22;
    frag_color = vec4(clamp(pow(lit, vec3(0.90)), 0.0, 1.0), v_color.a);
}
